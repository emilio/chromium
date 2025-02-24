// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller.h"

#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Mock;
using testing::NiceMock;
using testing::_;

namespace cc {
namespace {

const float kIdleThicknessScale =
    SingleScrollbarAnimationControllerThinning::kIdleThicknessScale;
const float kDefaultMouseMoveDistanceToTriggerAnimation =
    SingleScrollbarAnimationControllerThinning::
        kDefaultMouseMoveDistanceToTriggerAnimation;
const float kMouseMoveDistanceToTriggerShow =
    ScrollbarAnimationController::kMouseMoveDistanceToTriggerShow;
const int kThumbThickness = 10;

class MockScrollbarAnimationControllerClient
    : public ScrollbarAnimationControllerClient {
 public:
  explicit MockScrollbarAnimationControllerClient(LayerTreeHostImpl* host_impl)
      : host_impl_(host_impl) {}
  virtual ~MockScrollbarAnimationControllerClient() {}

  void PostDelayedScrollbarAnimationTask(const base::Closure& start_fade,
                                         base::TimeDelta delay) override {
    start_fade_ = start_fade;
    delay_ = delay;
  }
  void SetNeedsRedrawForScrollbarAnimation() override {}
  void SetNeedsAnimateForScrollbarAnimation() override {}
  ScrollbarSet ScrollbarsFor(int scroll_layer_id) const override {
    return host_impl_->ScrollbarsFor(scroll_layer_id);
  }
  MOCK_METHOD0(DidChangeScrollbarVisibility, void());

  base::Closure& start_fade() { return start_fade_; }
  base::TimeDelta& delay() { return delay_; }

 private:
  base::Closure start_fade_;
  base::TimeDelta delay_;
  LayerTreeHostImpl* host_impl_;
};

class ScrollbarAnimationControllerAuraOverlayTest : public testing::Test {
 public:
  ScrollbarAnimationControllerAuraOverlayTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_),
        client_(&host_impl_) {}

  void ExpectScrollbarsOpacity(float opacity) {
    EXPECT_FLOAT_EQ(opacity, v_scrollbar_layer_->Opacity());
    EXPECT_FLOAT_EQ(opacity, h_scrollbar_layer_->Opacity());
  }

 protected:
  const base::TimeDelta kShowDelay = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kFadeOutDelay = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kResizeFadeOutDelay = base::TimeDelta::FromSeconds(5);
  const base::TimeDelta kFadeOutDuration = base::TimeDelta::FromSeconds(3);
  const base::TimeDelta kThinningDuration = base::TimeDelta::FromSeconds(2);

  void SetUp() override {
    std::unique_ptr<LayerImpl> scroll_layer =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    std::unique_ptr<LayerImpl> clip =
        LayerImpl::Create(host_impl_.active_tree(), 2);
    clip_layer_ = clip.get();
    scroll_layer->SetScrollClipLayer(clip_layer_->id());
    LayerImpl* scroll_layer_ptr = scroll_layer.get();

    const int kTrackStart = 0;
    const bool kIsLeftSideVerticalScrollbar = false;
    const bool kIsOverlayScrollbar = true;

    std::unique_ptr<SolidColorScrollbarLayerImpl> h_scrollbar =
        SolidColorScrollbarLayerImpl::Create(
            host_impl_.active_tree(), 3, HORIZONTAL, kThumbThickness,
            kTrackStart, kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);
    std::unique_ptr<SolidColorScrollbarLayerImpl> v_scrollbar =
        SolidColorScrollbarLayerImpl::Create(
            host_impl_.active_tree(), 4, VERTICAL, kThumbThickness, kTrackStart,
            kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);
    v_scrollbar_layer_ = v_scrollbar.get();
    h_scrollbar_layer_ = h_scrollbar.get();

    scroll_layer->test_properties()->AddChild(std::move(v_scrollbar));
    scroll_layer->test_properties()->AddChild(std::move(h_scrollbar));
    clip_layer_->test_properties()->AddChild(std::move(scroll_layer));
    host_impl_.active_tree()->SetRootLayerForTesting(std::move(clip));

    v_scrollbar_layer_->SetScrollLayerId(scroll_layer_ptr->id());
    h_scrollbar_layer_->SetScrollLayerId(scroll_layer_ptr->id());
    v_scrollbar_layer_->test_properties()->opacity_can_animate = true;
    h_scrollbar_layer_->test_properties()->opacity_can_animate = true;
    clip_layer_->SetBounds(gfx::Size(100, 100));
    scroll_layer_ptr->SetBounds(gfx::Size(200, 200));
    host_impl_.active_tree()->BuildLayerListAndPropertyTreesForTesting();

    scrollbar_controller_ = ScrollbarAnimationController::
        CreateScrollbarAnimationControllerAuraOverlay(
            scroll_layer_ptr->id(), &client_, kShowDelay, kFadeOutDelay,
            kResizeFadeOutDelay, kFadeOutDuration, kThinningDuration);
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  std::unique_ptr<ScrollbarAnimationController> scrollbar_controller_;
  LayerImpl* clip_layer_;
  SolidColorScrollbarLayerImpl* v_scrollbar_layer_;
  SolidColorScrollbarLayerImpl* h_scrollbar_layer_;
  NiceMock<MockScrollbarAnimationControllerClient> client_;
};

// Check initialization of scrollbar. Should start off invisible and thin.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, Idle) {
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Check that scrollbar appears again when the layer becomes scrollable.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, AppearOnResize) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();
  ExpectScrollbarsOpacity(1);

  // Make the Layer non-scrollable, scrollbar disappears.
  clip_layer_->SetBounds(gfx::Size(200, 200));
  scrollbar_controller_->DidScrollUpdate(false);
  ExpectScrollbarsOpacity(0);

  // Make the layer scrollable, scrollbar appears again.
  clip_layer_->SetBounds(gfx::Size(100, 100));
  scrollbar_controller_->DidScrollUpdate(false);
  ExpectScrollbarsOpacity(1);
}

// Check that scrollbar disappears when the layer becomes non-scrollable.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, HideOnResize) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  EXPECT_EQ(gfx::Size(200, 200), scroll_layer->bounds());

  // Shrink along X axis, horizontal scrollbar should appear.
  clip_layer_->SetBounds(gfx::Size(100, 200));
  EXPECT_EQ(gfx::Size(100, 200), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();

  // Shrink along Y axis and expand along X, horizontal scrollbar
  // should disappear.
  clip_layer_->SetBounds(gfx::Size(200, 100));
  EXPECT_EQ(gfx::Size(200, 100), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, h_scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

// Scroll content. Confirm the scrollbar appears and fades out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, BasicAppearAndFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scrollbar should be invisible.
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  // Scrollbar should appear only on scroll update.
  scrollbar_controller_->DidScrollBegin();
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  scrollbar_controller_->DidScrollUpdate(false);
  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  scrollbar_controller_->DidScrollEnd();
  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  // Scrollbar should fade out over kFadeOutDuration.
  scrollbar_controller_->Animate(time);
  time += kFadeOutDuration;
  scrollbar_controller_->Animate(time);

  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
}

// Scroll content. Move the mouse near the scrollbar and confirm it becomes
// thick. Ensure it remains visible as long as the mouse is near the scrollbar.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MoveNearAndDontFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse near the scrollbar. This should cancel the currently
  // queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Vertical scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse is still near the Scrollbar. Once the thickness animation is
  // complete, the queued delayed fade out animation should be either cancelled
  // or null.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Scroll content. Move the mouse over the scrollbar and confirm it becomes
// thick. Ensure it remains visible as long as the mouse is over the scrollbar.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MoveOverAndDontFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar. This should cancel the currently
  // queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Vertical scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse is still over the Scrollbar. Once the thickness animation is
  // complete, the queued delayed fade out animation should be either cancelled
  // or null.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured before the thickening animation doesn't try
// to fade out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       DontFadeWhileCapturedBeforeThick) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar and capture it. It should become
  // thick without need for an animation.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The fade out animation should have been cleared or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured then move mouse away doesn't try to fade out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       DontFadeWhileCapturedThenAway) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the scrollbar and capture it. It should become
  // thick without need for an animation.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The fade out animation should have been cleared or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Then move mouse away, The fade out animation should have been cleared or
  // cancelled.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);

  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured after a thickening animation doesn't try to
// fade out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, DontFadeWhileCaptured) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar and animate it until it's thick.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is over the scrollbar, it should either clear or cancel the
  // queued fade.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Make sure the queued fade out animation is still null or cancelled after
  // capturing the scrollbar.
  scrollbar_controller_->DidMouseDown();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure releasing a captured scrollbar when the mouse isn't near it, causes
// the scrollbar to fade out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, FadeAfterReleasedFar) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar and capture it.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is still near the scrollbar, the queued fade should be
  // either null or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Now move the mouse away from the scrollbar and release it.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->DidMouseUp();

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The thickness animation is complete, a fade out must be queued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
}

// Make sure releasing a captured scrollbar when the mouse is near/over it,
// doesn't cause the scrollbar to fade out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, DontFadeAfterReleasedNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the scrollbar and capture it.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is over the scrollbar, the queued fade must be either
  // null or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse is still near the scrollbar, releasing it shouldn't do anything.
  scrollbar_controller_->DidMouseUp();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure moving near a scrollbar while it's fading out causes it to reset
// the opacity and thicken.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MoveNearScrollbarWhileFading) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // A fade out animation should have been enqueued. Start it.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  // Proceed half way through the fade out animation.
  time += kFadeOutDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.5f);

  // Now move the mouse near the scrollbar. It should reset opacity to 1
  // instantly and start animating to thick.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure we can't capture scrollbar that's completely faded out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, TestCantCaptureWhenFaded) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  client_.start_fade().Run();
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  // Fade the scrollbar out completely.
  time += kFadeOutDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);

  // Move mouse over the scrollbar. It shouldn't thicken the scrollbar since
  // it's completely faded out.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  client_.start_fade().Reset();

  // Now try to capture the scrollbar. It shouldn't do anything since it's
  // completely faded out.
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().is_null());

  // Similarly, releasing the scrollbar should have no effect.
  scrollbar_controller_->DidMouseUp();
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().is_null());
}

// Initiate a scroll when the pointer is already near the scrollbar. It should
// appear thick and remain thick.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, ScrollWithMouseNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;

  // Since the scrollbar isn't visible yet (because we haven't scrolled), we
  // shouldn't have applied the thickening.
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);

  // Now that we've received a scroll, we should be thick without an animation.
  ExpectScrollbarsOpacity(1);

  // An animation for the fade should be either null or cancelled, since
  // mouse is still near the scrollbar.
  scrollbar_controller_->DidScrollEnd();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Scrollbar should still be thick and visible.
  time += kFadeOutDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Tests that main thread scroll updates immediatley queue a fade out animation
// without requiring a ScrollEnd.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MainThreadScrollQueuesFade) {
  ASSERT_TRUE(client_.start_fade().is_null());

  // A ScrollUpdate without a ScrollBegin indicates a main thread scroll update
  // so we should schedule a fade out animation without waiting for a ScrollEnd
  // (which will never come).
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeOutDelay, client_.delay());

  client_.start_fade().Reset();

  // If we got a ScrollBegin, we shouldn't schedule the fade out animation until
  // we get a corresponding ScrollEnd.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_TRUE(client_.start_fade().is_null());
  scrollbar_controller_->DidScrollEnd();
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeOutDelay, client_.delay());
}

// Make sure that if the scroll update is as a result of a resize, we use the
// resize delay time instead of the default one.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, ResizeFadeDuration) {
  ASSERT_TRUE(client_.delay().is_zero());

  scrollbar_controller_->DidScrollUpdate(true);
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kResizeFadeOutDelay, client_.delay());

  client_.delay() = base::TimeDelta();

  // We should use the gesture delay rather than the resize delay if we're in a
  // gesture scroll, even if the resize param is set.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(true);
  scrollbar_controller_->DidScrollEnd();

  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeOutDelay, client_.delay());
}

// Tests that the fade effect is animated.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, FadeAnimated) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Appearance is instant.
  ExpectScrollbarsOpacity(1);

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeOutDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();

  // Test that at half the fade duration time, the opacity is at half.
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  time += kFadeOutDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.5f);

  time += kFadeOutDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);
}

// Tests that the controller tells the client when the scrollbars hide/show.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, NotifyChangedVisibility) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);

  scrollbar_controller_->DidScrollEnd();

  // Play out the fade out animation. We shouldn't notify that the scrollbars
  // are hidden until the animation is completly over. We can (but don't have
  // to) notify during the animation that the scrollbars are still visible.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(0);
  ASSERT_FALSE(client_.start_fade().is_null());
  client_.start_fade().Run();
  scrollbar_controller_->Animate(time);
  time += kFadeOutDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeOutDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeOutDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.25f);
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  time += kFadeOutDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  ExpectScrollbarsOpacity(0);
  Mock::VerifyAndClearExpectations(&client_);

  // Calling DidScrollUpdate without a begin (i.e. update from commit) should
  // also notify.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);
}

// Move the pointer near each scrollbar. Confirm it gets thick and narrow when
// moved away.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MouseNearEach) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Near vertical scrollbar
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 2);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Near horizontal scrollbar
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 2);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMoveNear(
      HORIZONTAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // An fade out animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeOutDelay, client_.delay());
}

// Move mouse near both scrollbars at the same time.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MouseNearBoth) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Near both Scrollbar
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 1);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move mouse from one to the other scrollbar before animation is finished, then
// away before animation finished.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseNearOtherBeforeAnimationFinished) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Near vertical scrollbar.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Vertical scrollbar animate to half thickened.
  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + (1.0f - kIdleThicknessScale) / 2,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Away vertical scrollbar and near horizontal scrollbar.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->DidMouseMoveNear(HORIZONTAL, 1);
  scrollbar_controller_->Animate(time);

  // Vertical scrollbar animate to thin. horizontal scrollbar animate to
  // thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Away horizontal scrollbar.
  scrollbar_controller_->DidMouseMoveNear(
      HORIZONTAL, kDefaultMouseMoveDistanceToTriggerAnimation);
  scrollbar_controller_->Animate(time);

  // Horizontal scrollbar animate to thin.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // An fade out animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeOutDelay, client_.delay());
}

// Ensure we have a delay fadeout animation after mouse leave without a mouse
// move.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MouseLeaveFadeOut) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse near scrollbar.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  // Should not have delay fadeout animation.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse leave.
  scrollbar_controller_->DidMouseLeave();

  // An fade out animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeOutDelay, client_.delay());
}

// Scrollbars should schedule a delay show when mouse hover hidden scrollbar.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, BasicMouseHoverShow) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse over scrollbar.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);

  // An show animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kShowDelay, client_.delay());

  // Play the delay animation.
  client_.start_fade().Run();
  EXPECT_TRUE(client_.start_fade().IsCancelled());
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
}

// Scrollbars should not schedule a new delay show when the mouse hovers inside
// a scrollbar already scheduled a delay show.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseHoverScrollbarAndMoveInside) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse over scrollbar.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);

  // An show animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kShowDelay, client_.delay());

  base::Closure& fade = client_.start_fade();
  // Move mouse inside scrollbar. should not post a new show.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kMouseMoveDistanceToTriggerShow - kThumbThickness - 1);

  EXPECT_TRUE(fade.Equals(client_.start_fade()));
}

// Scrollbars should cancel delay show when mouse hover hidden scrollbar then
// move out of scrollbar.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseHoverThenOutShouldCancelShow) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse over scrollbar.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);

  // An show animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kShowDelay, client_.delay());

  // Move mouse out of scrollbar，delay show should be canceled.
  scrollbar_controller_->DidMouseMoveNear(
      VERTICAL, kMouseMoveDistanceToTriggerShow - kThumbThickness);
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Scrollbars should cancel delay show when mouse hover hidden scrollbar then
// move out of window.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseHoverThenLeaveShouldCancelShow) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse over scrollbar.
  scrollbar_controller_->DidMouseMoveNear(VERTICAL, 0);

  // An show animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kShowDelay, client_.delay());

  // Move mouse out of window，delay show should be canceled.
  scrollbar_controller_->DidMouseLeave();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

class ScrollbarAnimationControllerAndroidTest
    : public testing::Test,
      public ScrollbarAnimationControllerClient {
 public:
  ScrollbarAnimationControllerAndroidTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_),
        did_request_redraw_(false),
        did_request_animate_(false) {}

  void PostDelayedScrollbarAnimationTask(const base::Closure& start_fade,
                                         base::TimeDelta delay) override {
    start_fade_ = start_fade;
    delay_ = delay;
  }
  void SetNeedsRedrawForScrollbarAnimation() override {
    did_request_redraw_ = true;
  }
  void SetNeedsAnimateForScrollbarAnimation() override {
    did_request_animate_ = true;
  }
  ScrollbarSet ScrollbarsFor(int scroll_layer_id) const override {
    return host_impl_.ScrollbarsFor(scroll_layer_id);
  }
  void DidChangeScrollbarVisibility() override {}

 protected:
  void SetUp() override {
    const int kTrackStart = 0;
    const bool kIsLeftSideVerticalScrollbar = false;
    const bool kIsOverlayScrollbar = true;  // Allow opacity animations.

    std::unique_ptr<LayerImpl> scroll_layer =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
        SolidColorScrollbarLayerImpl::Create(
            host_impl_.active_tree(), 2, orientation(), kThumbThickness,
            kTrackStart, kIsLeftSideVerticalScrollbar, kIsOverlayScrollbar);
    scrollbar_layer_ = scrollbar.get();
    scrollbar_layer_->test_properties()->opacity_can_animate = true;
    std::unique_ptr<LayerImpl> clip =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    clip_layer_ = clip.get();
    scroll_layer->SetScrollClipLayer(clip_layer_->id());
    LayerImpl* scroll_layer_ptr = scroll_layer.get();
    scroll_layer->test_properties()->AddChild(std::move(scrollbar));
    clip->test_properties()->AddChild(std::move(scroll_layer));
    host_impl_.active_tree()->SetRootLayerForTesting(std::move(clip));

    scrollbar_layer_->SetScrollLayerId(scroll_layer_ptr->id());
    clip_layer_->SetBounds(gfx::Size(100, 100));
    scroll_layer_ptr->SetBounds(gfx::Size(200, 200));
    host_impl_.active_tree()->BuildLayerListAndPropertyTreesForTesting();

    scrollbar_controller_ =
        ScrollbarAnimationController::CreateScrollbarAnimationControllerAndroid(
            scroll_layer_ptr->id(), this, base::TimeDelta::FromSeconds(2),
            base::TimeDelta::FromSeconds(5), base::TimeDelta::FromSeconds(3));
  }

  virtual ScrollbarOrientation orientation() const { return HORIZONTAL; }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
  std::unique_ptr<ScrollbarAnimationController> scrollbar_controller_;
  LayerImpl* clip_layer_;
  SolidColorScrollbarLayerImpl* scrollbar_layer_;

  base::Closure start_fade_;
  base::TimeDelta delay_;
  bool did_request_redraw_;
  bool did_request_animate_;
};

class VerticalScrollbarAnimationControllerAndroidTest
    : public ScrollbarAnimationControllerAndroidTest {
 protected:
  ScrollbarOrientation orientation() const override { return VERTICAL; }
};

TEST_F(ScrollbarAnimationControllerAndroidTest, DelayAnimationOnResize) {
  scrollbar_layer_->layer_tree_impl()
      ->property_trees()
      ->effect_tree.OnOpacityAnimated(0.0f,
                                      scrollbar_layer_->effect_tree_index(),
                                      scrollbar_layer_->layer_tree_impl());
  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(true);
  scrollbar_controller_->DidScrollEnd();
  // Normal Animation delay of 2 seconds.
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_EQ(delay_, base::TimeDelta::FromSeconds(2));

  scrollbar_layer_->layer_tree_impl()
      ->property_trees()
      ->effect_tree.OnOpacityAnimated(0.0f,
                                      scrollbar_layer_->effect_tree_index(),
                                      scrollbar_layer_->layer_tree_impl());
  scrollbar_controller_->DidScrollUpdate(true);
  // Delay animation on resize to 5 seconds.
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_EQ(delay_, base::TimeDelta::FromSeconds(5));
}

TEST_F(ScrollbarAnimationControllerAndroidTest, HiddenInBegin) {
  scrollbar_layer_->layer_tree_impl()
      ->property_trees()
      ->effect_tree.OnOpacityAnimated(0.0f,
                                      scrollbar_layer_->effect_tree_index(),
                                      scrollbar_layer_->layer_tree_impl());
  scrollbar_controller_->Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest,
       HiddenAfterNonScrollingGesture) {
  scrollbar_layer_->layer_tree_impl()
      ->property_trees()
      ->effect_tree.OnOpacityAnimated(0.0f,
                                      scrollbar_layer_->effect_tree_index(),
                                      scrollbar_layer_->layer_tree_impl());
  scrollbar_controller_->DidScrollBegin();

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  scrollbar_controller_->DidScrollEnd();

  EXPECT_TRUE(start_fade_.Equals(base::Closure()));

  time += base::TimeDelta::FromSeconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest, HideOnResize) {
  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  EXPECT_EQ(gfx::Size(200, 200), scroll_layer->bounds());

  EXPECT_EQ(HORIZONTAL, scrollbar_layer_->orientation());

  // Shrink along X axis, horizontal scrollbar should appear.
  clip_layer_->SetBounds(gfx::Size(100, 200));
  EXPECT_EQ(gfx::Size(100, 200), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();

  // Shrink along Y axis and expand along X, horizontal scrollbar
  // should disappear.
  clip_layer_->SetBounds(gfx::Size(200, 100));
  EXPECT_EQ(gfx::Size(200, 100), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

TEST_F(VerticalScrollbarAnimationControllerAndroidTest, HideOnResize) {
  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  EXPECT_EQ(gfx::Size(200, 200), scroll_layer->bounds());

  EXPECT_EQ(VERTICAL, scrollbar_layer_->orientation());

  // Shrink along X axis, vertical scrollbar should remain invisible.
  clip_layer_->SetBounds(gfx::Size(100, 200));
  EXPECT_EQ(gfx::Size(100, 200), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();

  // Shrink along Y axis and expand along X, vertical scrollbar should appear.
  clip_layer_->SetBounds(gfx::Size(200, 100));
  EXPECT_EQ(gfx::Size(200, 100), clip_layer_->bounds());

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

TEST_F(ScrollbarAnimationControllerAndroidTest, HideOnUserNonScrollableHorz) {
  EXPECT_EQ(HORIZONTAL, scrollbar_layer_->orientation());

  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  scroll_layer->set_user_scrollable_horizontal(false);

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

TEST_F(ScrollbarAnimationControllerAndroidTest, ShowOnUserNonScrollableVert) {
  EXPECT_EQ(HORIZONTAL, scrollbar_layer_->orientation());

  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  scroll_layer->set_user_scrollable_vertical(false);

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

TEST_F(VerticalScrollbarAnimationControllerAndroidTest,
       HideOnUserNonScrollableVert) {
  EXPECT_EQ(VERTICAL, scrollbar_layer_->orientation());

  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  scroll_layer->set_user_scrollable_vertical(false);

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

TEST_F(VerticalScrollbarAnimationControllerAndroidTest,
       ShowOnUserNonScrollableHorz) {
  EXPECT_EQ(VERTICAL, scrollbar_layer_->orientation());

  LayerImpl* scroll_layer = host_impl_.active_tree()->LayerById(1);
  ASSERT_TRUE(scroll_layer);
  scroll_layer->set_user_scrollable_horizontal(false);

  scrollbar_controller_->DidScrollBegin();

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
}

TEST_F(ScrollbarAnimationControllerAndroidTest, AwakenByScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollBegin();
  EXPECT_FALSE(did_request_animate_);

  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  EXPECT_TRUE(start_fade_.Equals(base::Closure()));

  time += base::TimeDelta::FromSeconds(100);

  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  scrollbar_controller_->DidScrollEnd();
  EXPECT_FALSE(did_request_animate_);
  start_fade_.Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;

  time += base::TimeDelta::FromSeconds(2);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollBegin();
  scrollbar_controller_->DidScrollUpdate(false);
  scrollbar_controller_->DidScrollEnd();

  start_fade_.Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;

  time += base::TimeDelta::FromSeconds(2);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest, AwakenByProgrammaticScroll) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(did_request_animate_);

  start_fade_.Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(did_request_animate_);

  start_fade_.Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  time += base::TimeDelta::FromSeconds(2);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollUpdate(false);
  start_fade_.Run();
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest,
       AnimationPreservedByNonScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollUpdate(false);
  start_fade_.Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollBegin();
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollEnd();
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest,
       AnimationOverriddenByScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(did_request_animate_);
  start_fade_.Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  scrollbar_controller_->DidScrollBegin();
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollUpdate(false);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->Opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollEnd();
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->Opacity());
}

}  // namespace
}  // namespace cc
