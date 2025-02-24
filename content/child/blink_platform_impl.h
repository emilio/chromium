// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
#define CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "content/child/webfallbackthemeengine_impl.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/public_features.h"
#include "ui/base/layout.h"

#if BUILDFLAG(USE_DEFAULT_RENDER_THEME)
#include "content/child/webthemeengine_impl_default.h"
#elif defined(OS_WIN)
#include "content/child/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#elif defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#endif

namespace base {
class WaitableEvent;
}

namespace blink {
namespace scheduler {
class WebThreadBase;
}
}

namespace content {

class NotificationDispatcher;
class ThreadSafeSender;
class WebCryptoImpl;

class CONTENT_EXPORT BlinkPlatformImpl
    : NON_EXPORTED_BASE(public blink::Platform) {
 public:
  BlinkPlatformImpl();
  explicit BlinkPlatformImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~BlinkPlatformImpl() override;

  // Platform methods (partial implementation):
  blink::WebThemeEngine* themeEngine() override;
  blink::WebFallbackThemeEngine* fallbackThemeEngine() override;
  blink::Platform::FileHandle databaseOpenFile(
      const blink::WebString& vfs_file_name,
      int desired_flags) override;
  int databaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;
  long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name) override;
  long long databaseGetFileSize(const blink::WebString& vfs_file_name) override;
  long long databaseGetSpaceAvailableForOrigin(
      const blink::WebSecurityOrigin& origin) override;
  bool databaseSetFileSize(const blink::WebString& vfs_file_name,
                           long long size) override;
  size_t actualMemoryUsageMB() override;
  size_t numberOfProcessors() override;

  void bindServiceConnector(
      mojo::ScopedMessagePipeHandle remote_handle) override;

  size_t maxDecodedImageBytes() override;
  uint32_t getUniqueIdForProcess() override;
  blink::WebString userAgent() override;
  blink::WebURLError cancelledError(const blink::WebURL& url) const override;
  blink::WebThread* createThread(const char* name) override;
  blink::WebThread* currentThread() override;
  void recordAction(const blink::UserMetricsAction&) override;

  blink::WebData loadResource(const char* name) override;
  blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name) override;
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      int numeric_value);
  blink::WebString queryLocalizedString(blink::WebLocalizedString::Name name,
                                        const blink::WebString& value) override;
  blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  void suddenTerminationChanged(bool enabled) override {}
  blink::WebThread* compositorThread() const override;
  blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;
  void didStartWorkerThread() override;
  void willStopWorkerThread() override;
  bool allowScriptExtensionForServiceWorker(
      const blink::WebURL& script_url) override;
  blink::WebCrypto* crypto() override;
  blink::WebNotificationManager* notificationManager() override;
  blink::WebPushProvider* pushProvider() override;

  blink::WebString domCodeStringFromEnum(int dom_code) override;
  int domEnumFromCodeString(const blink::WebString& codeString) override;
  blink::WebString domKeyStringFromEnum(int dom_key) override;
  int domKeyEnumFromString(const blink::WebString& key_string) override;

  // This class does *not* own the compositor thread. It is the responsibility
  // of the caller to ensure that the compositor thread is cleared before it is
  // destructed.
  void SetCompositorThread(blink::scheduler::WebThreadBase* compositor_thread);

  blink::WebFeaturePolicy* createFeaturePolicy(
      const blink::WebFeaturePolicy* parentPolicy,
      const blink::WebParsedFeaturePolicyHeader& containerPolicy,
      const blink::WebParsedFeaturePolicyHeader& policyHeader,
      const blink::WebSecurityOrigin& origin) override;
  blink::WebFeaturePolicy* duplicateFeaturePolicyWithOrigin(
      const blink::WebFeaturePolicy& policy,
      const blink::WebSecurityOrigin& new_origin) override;

 private:
  void InternalInit();
  void WaitUntilWebThreadTLSUpdate(blink::scheduler::WebThreadBase* thread);
  void UpdateWebThreadTLS(blink::WebThread* thread, base::WaitableEvent* event);

  bool IsMainThread() const;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  WebThemeEngineImpl native_theme_engine_;
  WebFallbackThemeEngineImpl fallback_theme_engine_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  webcrypto::WebCryptoImpl web_crypto_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  blink::scheduler::WebThreadBase* compositor_thread_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
