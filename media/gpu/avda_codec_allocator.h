// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_CODEC_ALLOCATOR_H_
#define MEDIA_GPU_AVDA_CODEC_ALLOCATOR_H_

#include <stddef.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_drm_bridge_cdm_context.h"
#include "media/base/media.h"
#include "media/base/surface_manager.h"
#include "media/base/video_codecs.h"
#include "media/gpu/avda_surface_bundle.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// For TaskRunnerFor. These are used as vector indices, so please update
// AVDACodecAllocator's constructor if you add / change them.
enum TaskType {
  // Task for an autodetected MediaCodec instance.
  AUTO_CODEC = 0,

  // Task for a software-codec-required MediaCodec.
  SW_CODEC = 1,
};

// Configuration info for MediaCodec.
// This is used to shuttle configuration info between threads without needing
// to worry about the lifetime of the AVDA instance.
class CodecConfig : public base::RefCountedThreadSafe<CodecConfig> {
 public:
  CodecConfig();

  VideoCodec codec = kUnknownVideoCodec;

  // The surface that MediaCodec is configured to output to.
  scoped_refptr<AVDASurfaceBundle> surface_bundle;

  // The MediaCrypto that MediaCodec is configured with for an encrypted stream.
  MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto;

  // Whether the encryption scheme requires us to use a protected surface.
  bool needs_protected_surface = false;

  // The initial coded size. The actual size might change at any time, so this
  // is only a hint.
  gfx::Size initial_expected_coded_size;

  // The type of allocation to use for. We use this to select the right thread
  // for construction / destruction, and to decide if we should restrict the
  // codec to be software only.
  TaskType task_type;

  // Codec specific data (SPS and PPS for H264).
  std::vector<uint8_t> csd0;
  std::vector<uint8_t> csd1;

 protected:
  friend class base::RefCountedThreadSafe<CodecConfig>;
  virtual ~CodecConfig();

 private:
  DISALLOW_COPY_AND_ASSIGN(CodecConfig);
};

class AVDACodecAllocatorClient {
 public:
  // Called when the requested SurfaceView becomes available after a call to
  // AllocateSurface()
  virtual void OnSurfaceAvailable(bool success) = 0;

  // Called when the allocated surface is being destroyed. This must either
  // replace the surface with MediaCodec#setSurface, or release the MediaCodec
  // it's attached to. The client no longer owns the surface and doesn't
  // need to call DeallocateSurface();
  virtual void OnSurfaceDestroyed() = 0;

  // Called on the main thread when a new MediaCodec is configured.
  // |media_codec| will be null if configuration failed.
  virtual void OnCodecConfigured(
      std::unique_ptr<MediaCodecBridge> media_codec) = 0;

 protected:
  ~AVDACodecAllocatorClient() {}
};

// AVDACodecAllocator manages threads for allocating and releasing MediaCodec
// instances.  These activities can hang, depending on android version, due
// to mediaserver bugs.  AVDACodecAllocator detects these cases, and reports
// on them to allow software fallback if the HW path is hung up.
class MEDIA_GPU_EXPORT AVDACodecAllocator {
 public:
  static AVDACodecAllocator* Instance();

  // Called synchronously when the given surface is being destroyed on the
  // browser UI thread.
  void OnSurfaceDestroyed(int surface_id);

  // Make sure the construction threads are started for |client|. Returns true
  // on success.
  bool StartThread(AVDACodecAllocatorClient* client);

  void StopThread(AVDACodecAllocatorClient* client);

  // Returns true if the caller now owns the surface, or false if someone else
  // owns the surface. |client| will be notified when the surface is available
  // via OnSurfaceAvailable().
  bool AllocateSurface(AVDACodecAllocatorClient* client, int surface_id);

  // Relinquish ownership of the surface or stop waiting for it to be available.
  // The caller must guarantee that when calling this the surface is either no
  // longer attached to a MediaCodec, or the MediaCodec it was attached to is
  // was released with ReleaseMediaCodec().
  void DeallocateSurface(AVDACodecAllocatorClient* client, int surface_id);

  // Create and configure a MediaCodec synchronously.
  std::unique_ptr<MediaCodecBridge> CreateMediaCodecSync(
      scoped_refptr<CodecConfig> codec_config);

  // Create and configure a MediaCodec asynchronously. The result is delivered
  // via OnCodecConfigured().
  virtual void CreateMediaCodecAsync(
      base::WeakPtr<AVDACodecAllocatorClient> client,
      scoped_refptr<CodecConfig> codec_config);

  // Asynchronously release |media_codec| with the attached surface.  We will
  // drop our reference to |surface_bundle| on the main thread after the codec
  // is deallocated, since the codec isn't using it anymore.  We will not take
  // other action on it (e.g., calling ReleaseSurfaceTexture if it has one),
  // since some other codec might be going to use it.  We just want to be sure
  // that it outlives |media_codec|.
  // TODO(watk): Bundle the MediaCodec and surface together so you can't get
  // this pairing wrong.
  void ReleaseMediaCodec(
      std::unique_ptr<MediaCodecBridge> media_codec,
      TaskType task_type,
      const scoped_refptr<AVDASurfaceBundle>& surface_bundle);

  // Returns a hint about whether the construction thread has hung for
  // |task_type|.  Note that if a thread isn't started, then we'll just return
  // "not hung", since it'll run on the current thread anyway.  The hang
  // detector will see no pending jobs in that case, so it's automatic.
  bool IsThreadLikelyHung(TaskType task_type);

  // Return true if and only if there is any AVDA registered.
  bool IsAnyRegisteredAVDA();

  // Return the task type to use for a new codec allocation, or nullopt if
  // both threads are hung.
  base::Optional<TaskType> TaskTypeForAllocation();

  // Return the task runner for tasks of type |type|.
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerFor(TaskType task_type);

  // Return a reference to the thread for unit tests.
  base::Thread& GetThreadForTesting(TaskType task_type);

 protected:
  // |tick_clock| and |stop_event| are for tests only.
  AVDACodecAllocator(base::TickClock* tick_clock = nullptr,
                     base::WaitableEvent* stop_event = nullptr);
  virtual ~AVDACodecAllocator();

  // Forward |media_codec|, which is configured to output to |surface_bundle|,
  // to |client| if |client| is still around.  Otherwise, release the codec and
  // then drop our ref to |surface_bundle|.
  void ForwardOrDropCodec(base::WeakPtr<AVDACodecAllocatorClient> client,
                          TaskType task_type,
                          scoped_refptr<AVDASurfaceBundle> surface_bundle,
                          std::unique_ptr<MediaCodecBridge> media_codec);

 private:
  friend class AVDACodecAllocatorTest;

  struct OwnerRecord {
    AVDACodecAllocatorClient* owner = nullptr;
    AVDACodecAllocatorClient* waiter = nullptr;
  };

  class HangDetector : public base::MessageLoop::TaskObserver {
   public:
    HangDetector(base::TickClock* tick_clock);
    void WillProcessTask(const base::PendingTask& pending_task) override;
    void DidProcessTask(const base::PendingTask& pending_task) override;
    bool IsThreadLikelyHung();

   private:
    base::Lock lock_;

    // Non-null when a task is currently running.
    base::TimeTicks task_start_time_;

    base::TickClock* tick_clock_;

    DISALLOW_COPY_AND_ASSIGN(HangDetector);
  };

  // Handy combination of a thread and hang detector for it.
  struct ThreadAndHangDetector {
    ThreadAndHangDetector(const std::string& name, base::TickClock* tick_clock)
        : thread(name), hang_detector(tick_clock) {}
    base::Thread thread;
    HangDetector hang_detector;
  };

  // Called on the gpu main thread when a codec is freed on a codec thread.
  // |surface_bundle| is the surface bundle that the codec was using.
  void OnMediaCodecReleased(scoped_refptr<AVDASurfaceBundle> surface_bundle);

  // Stop the thread indicated by |index|. This signals stop_event_for_testing_
  // after both threads are stopped.
  void StopThreadTask(size_t index);

  // All registered AVDAs.
  std::set<AVDACodecAllocatorClient*> clients_;

  // Indexed by surface id.
  std::map<int32_t, OwnerRecord> surface_owners_;

  // Waitable events for ongoing release tasks indexed by surface id so we can
  // wait on the codec release if the surface attached to it is being destroyed.
  std::map<int32_t, base::WaitableEvent> pending_codec_releases_;

  // Threads for each of TaskType.  They are started / stopped as avda instances
  // show and and request them.  The vector indicies must match TaskType.
  std::vector<ThreadAndHangDetector*> threads_;

  base::ThreadChecker thread_checker_;

  base::WaitableEvent* stop_event_for_testing_;

  // For canceling pending StopThreadTask()s.
  base::WeakPtrFactory<AVDACodecAllocator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(AVDACodecAllocator);
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_CODEC_ALLOCATOR_H_
