// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "base/files/file_enumerator.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/capture/video/linux/v4l2_capture_delegate.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

// Base id and class identifiers for Controls to be modified and later tested
// agains default values.
static struct {
  uint32_t control_base;
  uint32_t class_id;
} const kControls[] = {{V4L2_CID_USER_BASE, V4L2_CID_USER_CLASS},
                       {V4L2_CID_CAMERA_CLASS_BASE, V4L2_CID_CAMERA_CLASS}};

// Determines if |control_id| is special, i.e. controls another one's state, or
// if it should be skipped (blacklisted, https://crbug.com/697885).
static bool IsSpecialOrBlacklistedControl(int control_id) {
  switch (control_id) {
    case V4L2_CID_AUTO_WHITE_BALANCE:
    case V4L2_CID_EXPOSURE_AUTO:
    case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
    case V4L2_CID_FOCUS_AUTO:
    case V4L2_CID_PAN_ABSOLUTE:
    case V4L2_CID_TILT_ABSOLUTE:
      return true;
  }
  return false;
}

static void SetControlsToMaxValues(int device_fd) {
  // Set V4L2_CID_AUTO_WHITE_BALANCE to false first.
  v4l2_control auto_white_balance = {};
  auto_white_balance.id = V4L2_CID_AUTO_WHITE_BALANCE;
  auto_white_balance.value = false;
  if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_CTRL, &auto_white_balance)) < 0)
    DPLOG(ERROR) << "VIDIOC_S_CTRL";

  std::vector<struct v4l2_ext_control> special_camera_controls;
  // Set V4L2_CID_EXPOSURE_AUTO to V4L2_EXPOSURE_MANUAL.
  v4l2_ext_control auto_exposure = {};
  auto_exposure.id = V4L2_CID_EXPOSURE_AUTO;
  auto_exposure.value = V4L2_EXPOSURE_MANUAL;
  special_camera_controls.push_back(auto_exposure);
  // Set V4L2_CID_EXPOSURE_AUTO_PRIORITY to false.
  v4l2_ext_control priority_auto_exposure = {};
  priority_auto_exposure.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
  priority_auto_exposure.value = false;
  special_camera_controls.push_back(priority_auto_exposure);
  // Set V4L2_CID_FOCUS_AUTO to false.
  v4l2_ext_control auto_focus = {};
  auto_focus.id = V4L2_CID_FOCUS_AUTO;
  auto_focus.value = false;
  special_camera_controls.push_back(auto_focus);

  struct v4l2_ext_controls ext_controls = {};
  ext_controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
  ext_controls.count = special_camera_controls.size();
  ext_controls.controls = special_camera_controls.data();
  if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_EXT_CTRLS, &ext_controls)) < 0)
    DPLOG(ERROR) << "VIDIOC_S_EXT_CTRLS";

  for (const auto& control : kControls) {
    std::vector<struct v4l2_ext_control> camera_controls;

    v4l2_queryctrl range = {};
    range.id = control.control_base | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      if (IsSpecialOrBlacklistedControl(range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL))
        continue;
      DVLOG(1) << __func__ << " " << range.name << " set to " << range.maximum;

      struct v4l2_ext_control ext_control = {};
      ext_control.id = range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
      ext_control.value = range.maximum;
      camera_controls.push_back(ext_control);
    }

    if (!camera_controls.empty()) {
      struct v4l2_ext_controls ext_controls = {};
      ext_controls.ctrl_class = control.class_id;
      ext_controls.count = camera_controls.size();
      ext_controls.controls = camera_controls.data();
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_EXT_CTRLS, &ext_controls)) < 0)
        DPLOG(ERROR) << "VIDIOC_S_EXT_CTRLS";
    }

    range.id = control.control_base | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      if (IsSpecialOrBlacklistedControl(range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL))
        continue;
      DVLOG(1) << __func__ << " " << range.name << " set to " << range.maximum;

      v4l2_control readback = {};
      readback.id = range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_G_CTRL, &readback)) < 0)
        DPLOG(ERROR) << range.name << ", failed to be read.";
      EXPECT_EQ(range.maximum, readback.value) << " control " << range.name
                                               << " didnt set correctly";
    }
  }
}

static void VerifyUserControlsAreSetToDefaultValues(int device_fd) {
  for (const auto& control : kControls) {
    v4l2_queryctrl range = {};
    range.id = control.control_base | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      DVLOG(1) << __func__ << " " << range.name << ": " << range.minimum << "-"
               << range.maximum << ", default: " << range.default_value;

      v4l2_control current = {};
      current.id = range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_G_CTRL, &current)) < 0)
        DPLOG(ERROR) << "control " << range.name;

      EXPECT_EQ(range.default_value, current.value);
    }
  }
}

class MockVideoCaptureDeviceClient : public VideoCaptureDevice::Client {
 public:
  MOCK_METHOD7(OnIncomingCapturedData,
               void(const uint8_t*,
                    int,
                    const VideoCaptureFormat&,
                    int,
                    base::TimeTicks,
                    base::TimeDelta,
                    int));
  MOCK_METHOD4(ReserveOutputBuffer,
               Buffer(const gfx::Size&,
                      media::VideoPixelFormat,
                      media::VideoPixelStorage,
                      int));
  void OnIncomingCapturedBuffer(Buffer buffer,
                                const VideoCaptureFormat& frame_format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override {
    DoOnIncomingCapturedBuffer();
  }
  MOCK_METHOD0(DoOnIncomingCapturedBuffer, void(void));
  void OnIncomingCapturedBufferExt(
      Buffer buffer,
      const VideoCaptureFormat& format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const VideoFrameMetadata& additional_metadata) override {
    DoOnIncomingCapturedVideoFrame();
  }
  MOCK_METHOD0(DoOnIncomingCapturedVideoFrame, void(void));
  MOCK_METHOD4(
      ResurrectLastOutputBuffer,
      Buffer(const gfx::Size&, VideoPixelFormat, VideoPixelStorage, int));
  MOCK_METHOD2(OnError,
               void(const tracked_objects::Location& from_here,
                    const std::string& reason));
  MOCK_CONST_METHOD0(GetBufferPoolUtilization, double(void));
  MOCK_METHOD0(OnStarted, void(void));
};

class V4L2CaptureDelegateTest : public ::testing::Test {
 public:
  V4L2CaptureDelegateTest()
      : device_descriptor_("Device 0", "/dev/video0"),
        delegate_(new V4L2CaptureDelegate(device_descriptor_,
                                          base::ThreadTaskRunnerHandle::Get(),
                                          50)) {}
  ~V4L2CaptureDelegateTest() override = default;

  base::MessageLoop loop_;
  VideoCaptureDeviceDescriptor device_descriptor_;
  scoped_refptr<V4L2CaptureDelegate> delegate_;
};

}  // anonymous namespace

TEST_F(V4L2CaptureDelegateTest, CreateAndDestroyAndVerifyControls) {
  // Check that there is at least a video device, otherwise bail.
  const base::FilePath path("/dev/");
  base::FileEnumerator enumerator(path, false, base::FileEnumerator::FILES,
                                  "video*");
  if (enumerator.Next().empty()) {
    DLOG(INFO) << " No devices found, skipping test";
    return;
  }

  // Open device, manipulate user and camera controls, and close it.
  {
    base::ScopedFD device_fd(
        HANDLE_EINTR(open(device_descriptor_.device_id.c_str(), O_RDWR)));
    ASSERT_TRUE(device_fd.is_valid());

    SetControlsToMaxValues(device_fd.get());

    base::RunLoop().RunUntilIdle();
  }

  // Start and stop capturing, triggering the resetting of user and camera
  // control values.
  {
    std::unique_ptr<MockVideoCaptureDeviceClient> client(
        new MockVideoCaptureDeviceClient());
    MockVideoCaptureDeviceClient* client_ptr = client.get();
    EXPECT_CALL(*client_ptr, OnStarted());
    delegate_->AllocateAndStart(320 /* width */, 240 /* height */,
                                10.0 /* frame_rate */, std::move(client));

    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*client_ptr, OnIncomingCapturedData(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    run_loop.Run();

    delegate_->StopAndDeAllocate();
    base::RunLoop().RunUntilIdle();
  }

  // Reopen the device and verify all user and camera controls should be back to
  // their |default_value|s.
  {
    base::ScopedFD device_fd(
        HANDLE_EINTR(open(device_descriptor_.device_id.c_str(), O_RDWR)));
    ASSERT_TRUE(device_fd.is_valid());
    VerifyUserControlsAreSetToDefaultValues(device_fd.get());
  }
}

};  // namespace media
