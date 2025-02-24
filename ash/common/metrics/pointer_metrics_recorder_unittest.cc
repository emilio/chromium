// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/metrics/pointer_metrics_recorder.h"

#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shared/app_types.h"
#include "ash/test/ash_test_base.h"
#include "base/test/histogram_tester.h"
#include "ui/events/event.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget.h"

using views::PointerWatcher;

namespace ash {
namespace {

const char kFormFactorHistogramName[] = "Event.DownEventCount.PerFormFactor";
const char kInputHistogramName[] = "Event.DownEventCount.PerInput";
const char kDestinationHistogramName[] = "Event.DownEventCount.PerDestination";

// Test fixture for the PointerMetricsRecorder class.
class PointerMetricsRecorderTest : public test::AshTestBase {
 public:
  PointerMetricsRecorderTest();
  ~PointerMetricsRecorderTest() override;

  // test::AshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  // The test target.
  std::unique_ptr<PointerMetricsRecorder> pointer_metrics_recorder_;

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerMetricsRecorderTest);
};

PointerMetricsRecorderTest::PointerMetricsRecorderTest() {}

PointerMetricsRecorderTest::~PointerMetricsRecorderTest() {}

void PointerMetricsRecorderTest::SetUp() {
  test::AshTestBase::SetUp();
  pointer_metrics_recorder_.reset(new PointerMetricsRecorder());
  histogram_tester_.reset(new base::HistogramTester());
}

void PointerMetricsRecorderTest::TearDown() {
  pointer_metrics_recorder_.reset();
  test::AshTestBase::TearDown();
}

}  // namespace

// Verifies that histogram is not recorded when receiving events that are not
// down events.
TEST_F(PointerMetricsRecorderTest, NonDownEventsInAllPointerHistogram) {
  std::unique_ptr<views::Widget> target =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  const ui::PointerEvent pointer_event(
      ui::ET_POINTER_UP, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE),
      base::TimeTicks());
  pointer_metrics_recorder_->OnPointerEventObserved(pointer_event, gfx::Point(),
                                                    target.get());

  histogram_tester_->ExpectTotalCount(kFormFactorHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kInputHistogramName, 0);
  histogram_tester_->ExpectTotalCount(kDestinationHistogramName, 0);
}

// Verifies that down events from different inputs are recorded.
TEST_F(PointerMetricsRecorderTest, DownEventPerInput) {
  std::unique_ptr<views::Widget> target =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());

  const ui::PointerEvent unknown_event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_UNKNOWN),
      base::TimeTicks());
  pointer_metrics_recorder_->OnPointerEventObserved(unknown_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kInputHistogramName, 0, 1);

  const ui::PointerEvent mouse_event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE),
      base::TimeTicks());
  pointer_metrics_recorder_->OnPointerEventObserved(mouse_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kInputHistogramName, 1, 1);

  const ui::PointerEvent stylus_event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_PEN),
      base::TimeTicks());
  pointer_metrics_recorder_->OnPointerEventObserved(stylus_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kInputHistogramName, 2, 1);

  const ui::PointerEvent stylus_event2(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_ERASER),
      base::TimeTicks());
  pointer_metrics_recorder_->OnPointerEventObserved(stylus_event2, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kInputHistogramName, 2, 2);

  const ui::PointerEvent touch_event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks());
  pointer_metrics_recorder_->OnPointerEventObserved(touch_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kInputHistogramName, 3, 1);
}

// Verifies that down events in different form factors are recorded.
TEST_F(PointerMetricsRecorderTest, DownEventPerFormFactor) {
  std::unique_ptr<views::Widget> target =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  const ui::PointerEvent pointer_event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE),
      base::TimeTicks());

  // Enable maximize mode
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  pointer_metrics_recorder_->OnPointerEventObserved(pointer_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kFormFactorHistogramName, 1, 1);

  // Disable maximize mode
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
  pointer_metrics_recorder_->OnPointerEventObserved(pointer_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kFormFactorHistogramName, 0, 1);
}

// Verifies that down events dispatched to different destinations are recorded.
TEST_F(PointerMetricsRecorderTest, DownEventPerDestination) {
  std::unique_ptr<views::Widget> target =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  const ui::PointerEvent pointer_event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), 0, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE),
      base::TimeTicks());

  WmWindow* window = WmWindow::Get(target->GetNativeWindow());
  CHECK(window);

  window->SetAppType(static_cast<int>(AppType::OTHERS));
  pointer_metrics_recorder_->OnPointerEventObserved(pointer_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kDestinationHistogramName, 0, 1);

  window->SetAppType(static_cast<int>(AppType::BROWSER));
  pointer_metrics_recorder_->OnPointerEventObserved(pointer_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kDestinationHistogramName, 1, 1);

  window->SetAppType(static_cast<int>(AppType::CHROME_APP));
  pointer_metrics_recorder_->OnPointerEventObserved(pointer_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kDestinationHistogramName, 2, 1);

  window->SetAppType(static_cast<int>(AppType::ARC_APP));
  pointer_metrics_recorder_->OnPointerEventObserved(pointer_event, gfx::Point(),
                                                    target.get());
  histogram_tester_->ExpectBucketCount(kDestinationHistogramName, 3, 1);
}

}  // namespace ash
