// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/root_window_transformers.h"

#include <memory>

#include "ash/common/shelf/shelf_widget.h"
#include "ash/display/display_util.h"
#include "ash/host/root_window_transformer.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/mirror_window_test_api.h"
#include "base/synchronization/waitable_event.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const char kWallpaperView[] = "WallpaperView";

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler()
      : target_root_(nullptr),
        touch_radius_x_(0.0),
        touch_radius_y_(0.0),
        scroll_x_offset_(0.0),
        scroll_y_offset_(0.0),
        scroll_x_offset_ordinal_(0.0),
        scroll_y_offset_ordinal_(0.0) {}
  ~TestEventHandler() override {}

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->flags() & ui::EF_IS_SYNTHESIZED)
      return;
    aura::Window* target = static_cast<aura::Window*>(event->target());
    mouse_location_ = event->root_location();
    target_root_ = target->GetRootWindow();
    event->StopPropagation();
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    // Only record when the target is the wallpaper, which covers the entire
    // root window.
    if (target->GetName() != kWallpaperView)
      return;
    touch_radius_x_ = event->pointer_details().radius_x;
    touch_radius_y_ = event->pointer_details().radius_y;
    event->StopPropagation();
  }

  void OnScrollEvent(ui::ScrollEvent* event) override {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    // Only record when the target is the wallpaper, which covers the entire
    // root window.
    if (target->GetName() != kWallpaperView)
      return;

    if (event->type() == ui::ET_SCROLL) {
      scroll_x_offset_ = event->x_offset();
      scroll_y_offset_ = event->y_offset();
      scroll_x_offset_ordinal_ = event->x_offset_ordinal();
      scroll_y_offset_ordinal_ = event->y_offset_ordinal();
    }
    event->StopPropagation();
  }

  std::string GetLocationAndReset() {
    std::string result = mouse_location_.ToString();
    mouse_location_.SetPoint(0, 0);
    target_root_ = nullptr;
    return result;
  }

  float touch_radius_x() const { return touch_radius_x_; }
  float touch_radius_y() const { return touch_radius_y_; }
  float scroll_x_offset() const { return scroll_x_offset_; }
  float scroll_y_offset() const { return scroll_y_offset_; }
  float scroll_x_offset_ordinal() const { return scroll_x_offset_ordinal_; }
  float scroll_y_offset_ordinal() const { return scroll_y_offset_ordinal_; }

 private:
  gfx::Point mouse_location_;
  aura::Window* target_root_;

  float touch_radius_x_;
  float touch_radius_y_;
  float scroll_x_offset_;
  float scroll_y_offset_;
  float scroll_x_offset_ordinal_;
  float scroll_y_offset_ordinal_;

  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

class RootWindowTransformersTest : public test::AshTestBase {
 public:
  RootWindowTransformersTest(){};
  ~RootWindowTransformersTest() override{};

  float GetStoredUIScale(int64_t id) {
    return display_manager()->GetDisplayInfo(id).GetEffectiveUIScale();
  };

  std::unique_ptr<RootWindowTransformer>
  CreateCurrentRootWindowTransformerForMirroring() {
    DCHECK(display_manager()->IsInMirrorMode());
    const display::ManagedDisplayInfo& mirror_display_info =
        display_manager()->GetDisplayInfo(
            display_manager()->mirroring_display_id());
    const display::ManagedDisplayInfo& source_display_info =
        display_manager()->GetDisplayInfo(
            display::Screen::GetScreen()->GetPrimaryDisplay().id());
    return std::unique_ptr<RootWindowTransformer>(
        CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                      mirror_display_info));
  };

  DISALLOW_COPY_AND_ASSIGN(RootWindowTransformersTest);
};

}  // namespace

// using RootWindowTransformersTest = test::AshTestBase;

#if defined(OS_WIN)
// TODO(scottmg): RootWindow doesn't get resized on Windows
// Ash. http://crbug.com/247916.
#define MAYBE_RotateAndMagnify DISABLED_RotateAndMagniy
#define MAYBE_TouchScaleAndMagnify DISABLED_TouchScaleAndMagnify
#define MAYBE_ConvertHostToRootCoords DISABLED_ConvertHostToRootCoords
#else
#define MAYBE_RotateAndMagnify RotateAndMagniy
#define MAYBE_TouchScaleAndMagnify TouchScaleAndMagnify
#define MAYBE_ConvertHostToRootCoords ConvertHostToRootCoords
#endif

TEST_F(RootWindowTransformersTest, MAYBE_RotateAndMagnify) {
  MagnificationController* magnifier =
      Shell::GetInstance()->magnification_controller();

  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  int64_t display2_id = display_manager()->GetSecondaryDisplay().id();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ui::test::EventGenerator generator1(root_windows[0]);
  ui::test::EventGenerator generator2(root_windows[1]);

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("120,0 150x200",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(40, 80);
  EXPECT_EQ("50,90", event_handler.GetLocationAndReset());
  EXPECT_EQ("50,90",
            aura::Env::GetInstance()->last_mouse_location().ToString());
  EXPECT_EQ(display::Display::ROTATE_0,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  display_manager()->SetDisplayRotation(
      display1.id(), display::Display::ROTATE_90,
      display::Display::ROTATION_SOURCE_ACTIVE);
  // Move the cursor to the center of the first root window.
  generator1.MoveMouseToInHost(59, 100);

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("200,0 150x200",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(39, 120);
  EXPECT_EQ("110,70", event_handler.GetLocationAndReset());
  EXPECT_EQ("110,70",
            aura::Env::GetInstance()->last_mouse_location().ToString());
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(
          display_manager(), display::DisplayPlacement::BOTTOM, 50));
  EXPECT_EQ("50,120 150x200",
            display_manager()->GetSecondaryDisplay().bounds().ToString());

  display_manager()->SetDisplayRotation(
      display2_id, display::Display::ROTATE_270,
      display::Display::ROTATION_SOURCE_ACTIVE);
  // Move the cursor to the center of the second root window.
  generator2.MoveMouseToInHost(151, 199);

  magnifier->SetEnabled(true);
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("50,120 200x150",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator2.MoveMouseToInHost(172, 219);
  EXPECT_EQ("95,80", event_handler.GetLocationAndReset());
  EXPECT_EQ("145,200",
            aura::Env::GetInstance()->last_mouse_location().ToString());
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_270,
            GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  display_manager()->SetDisplayRotation(
      display1.id(), display::Display::ROTATE_180,
      display::Display::ROTATION_SOURCE_ACTIVE);
  // Move the cursor to the center of the first root window.
  generator1.MoveMouseToInHost(59, 99);

  magnifier->SetEnabled(true);
  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  // Dislay must share at least 100, so the x's offset becomes 20.
  EXPECT_EQ("20,200 200x150",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(39, 59);
  EXPECT_EQ("70,120", event_handler.GetLocationAndReset());
  EXPECT_EQ(display::Display::ROTATE_180,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_270,
            GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, ScaleAndMagnify) {
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("600x400*2@1.5,500x300");

  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display1.id());
  display::Display display2 = display_manager()->GetSecondaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  MagnificationController* magnifier =
      Shell::GetInstance()->magnification_controller();

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ("0,0 450x300", display1.bounds().ToString());
  EXPECT_EQ("0,0 450x300", root_windows[0]->bounds().ToString());
  EXPECT_EQ("450,0 500x300", display2.bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredUIScale(display2.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(500, 200);
  EXPECT_EQ("299,150", event_handler.GetLocationAndReset());
  magnifier->SetEnabled(false);

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display1.id(), 1.25f);
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display2 = display_manager()->GetSecondaryDisplay();
  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ("0,0 375x250", display1.bounds().ToString());
  EXPECT_EQ("0,0 375x250", root_windows[0]->bounds().ToString());
  EXPECT_EQ("375,0 500x300", display2.bounds().ToString());
  EXPECT_EQ(1.25f, GetStoredUIScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredUIScale(display2.id()));
  magnifier->SetEnabled(false);

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, MAYBE_TouchScaleAndMagnify) {
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("200x200*2");
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];
  ui::test::EventGenerator generator(root_window);
  MagnificationController* magnifier =
      Shell::GetInstance()->magnification_controller();

  magnifier->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());
  magnifier->SetScale(2.5f, false);
  EXPECT_FLOAT_EQ(2.5f, magnifier->GetScale());
  generator.PressMoveAndReleaseTouchTo(50, 50);
  // Default test touches have radius_x/y = 1.0, with device scale
  // factor = 2, the scaled radius_x/y should be 0.5.
  EXPECT_FLOAT_EQ(0.2f, event_handler.touch_radius_x());
  EXPECT_FLOAT_EQ(0.2f, event_handler.touch_radius_y());

  generator.ScrollSequence(gfx::Point(0, 0),
                           base::TimeDelta::FromMilliseconds(100), 10.0, 1.0, 5,
                           1);

  // ordinal_offset is invariant to the device scale factor.
  EXPECT_FLOAT_EQ(event_handler.scroll_x_offset(),
                  event_handler.scroll_x_offset_ordinal());
  EXPECT_FLOAT_EQ(event_handler.scroll_y_offset(),
                  event_handler.scroll_y_offset_ordinal());
  magnifier->SetEnabled(false);

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, MAYBE_ConvertHostToRootCoords) {
  TestEventHandler event_handler;
  Shell::GetInstance()->AddPreTargetHandler(&event_handler);
  MagnificationController* magnifier =
      Shell::GetInstance()->magnification_controller();

  // Test 1
  UpdateDisplay("600x400*2/r@1.5");

  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 300x450", display1.bounds().ToString());
  EXPECT_EQ("0,0 300x450", root_windows[0]->bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ("150,224", event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ("150,224", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ("187,261", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ("237,299", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("137,348", event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  // Test 2
  UpdateDisplay("600x400*2/u@1.5");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 450x300", display1.bounds().ToString());
  EXPECT_EQ("0,0 450x300", root_windows[0]->bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));

  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ("224,149", event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ("224,148", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ("261,111", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ("299,60", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("348,159", event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  // Test 3
  UpdateDisplay("600x400*2/l@1.5");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 300x450", display1.bounds().ToString());
  EXPECT_EQ("0,0 300x450", root_windows[0]->bounds().ToString());
  EXPECT_EQ(1.5f, GetStoredUIScale(display1.id()));

  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ("149,225", event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ("148,224", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ("111,187", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ("60,149", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("159,99", event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, LetterBoxPillarBox) {
  test::MirrorWindowTestApi test_api;
  display_manager()->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  UpdateDisplay("400x200,500x500");
  std::unique_ptr<RootWindowTransformer> transformer(
      CreateCurrentRootWindowTransformerForMirroring());
  // Y margin must be margin is (500 - 500/400 * 200) / 2 = 125.
  EXPECT_EQ("0,125,0,125", transformer->GetHostInsets().ToString());

  UpdateDisplay("200x400,500x500");
  // The aspect ratio is flipped, so X margin is now 125.
  transformer = CreateCurrentRootWindowTransformerForMirroring();
  EXPECT_EQ("125,0,125,0", transformer->GetHostInsets().ToString());
}

}  // namespace ash
