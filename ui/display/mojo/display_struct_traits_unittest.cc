// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_snapshot_mojo.h"
#include "ui/display/mojo/display_struct_traits_test.mojom.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace {

constexpr int64_t kDisplayId1 = 123;
constexpr int64_t kDisplayId2 = 456;
constexpr int64_t kDisplayId3 = 789;

class DisplayStructTraitsTest : public testing::Test,
                                public mojom::DisplayStructTraitsTest {
 public:
  DisplayStructTraitsTest() {}

 protected:
  mojom::DisplayStructTraitsTestPtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // mojom::DisplayStructTraitsTest:
  void EchoDisplay(const Display& in,
                   const EchoDisplayCallback& callback) override {
    callback.Run(in);
  }

  void EchoDisplayMode(std::unique_ptr<DisplayMode> in,
                       const EchoDisplayModeCallback& callback) override {
    callback.Run(std::move(in));
  }

  void EchoDisplaySnapshotMojo(
      std::unique_ptr<DisplaySnapshotMojo> in,
      const EchoDisplaySnapshotMojoCallback& callback) override {
    callback.Run(std::move(in));
  }

  void EchoDisplayPlacement(
      const DisplayPlacement& in,
      const EchoDisplayPlacementCallback& callback) override {
    callback.Run(in);
  }

  void EchoDisplayLayout(std::unique_ptr<display::DisplayLayout> in,
                         const EchoDisplayLayoutCallback& callback) override {
    callback.Run(std::move(in));
  }

  void EchoHDCPState(display::HDCPState in,
                     const EchoHDCPStateCallback& callback) override {
    callback.Run(in);
  }

  void EchoGammaRampRGBEntry(
      const GammaRampRGBEntry& in,
      const EchoGammaRampRGBEntryCallback& callback) override {
    callback.Run(in);
  }

  base::MessageLoop loop_;  // A MessageLoop is needed for Mojo IPC to work.
  mojo::BindingSet<mojom::DisplayStructTraitsTest> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(DisplayStructTraitsTest);
};

void CheckDisplaysEqual(const Display& input, const Display& output) {
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.id(), output.id());
  EXPECT_EQ(input.bounds(), output.bounds());
  EXPECT_EQ(input.work_area(), output.work_area());
  EXPECT_EQ(input.device_scale_factor(), output.device_scale_factor());
  EXPECT_EQ(input.rotation(), output.rotation());
  EXPECT_EQ(input.touch_support(), output.touch_support());
  EXPECT_EQ(input.maximum_cursor_size(), output.maximum_cursor_size());
}

void CheckDisplayLayoutsEqual(const DisplayLayout& input,
                              const DisplayLayout& output) {
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.placement_list, output.placement_list);
  EXPECT_EQ(input.mirrored, output.mirrored);
  EXPECT_EQ(input.default_unified, output.default_unified);
  EXPECT_EQ(input.primary_id, output.primary_id);
}

bool CompareModes(const DisplayMode& lhs, const DisplayMode& rhs) {
  return lhs.size() == rhs.size() &&
         lhs.is_interlaced() == rhs.is_interlaced() &&
         lhs.refresh_rate() == rhs.refresh_rate();
}

void CheckDisplaySnapShotMojoEqual(const DisplaySnapshotMojo& input,
                                   const DisplaySnapshotMojo& output) {
  // We want to test each component individually to make sure each data member
  // was correctly serialized and deserialized.
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.display_id(), output.display_id());
  EXPECT_EQ(input.origin(), output.origin());
  EXPECT_EQ(input.physical_size(), output.physical_size());
  EXPECT_EQ(input.type(), output.type());
  EXPECT_EQ(input.is_aspect_preserving_scaling(),
            output.is_aspect_preserving_scaling());
  EXPECT_EQ(input.has_overscan(), output.has_overscan());
  EXPECT_EQ(input.has_color_correction_matrix(),
            output.has_color_correction_matrix());
  EXPECT_EQ(input.display_name(), output.display_name());
  EXPECT_EQ(input.sys_path(), output.sys_path());
  EXPECT_EQ(input.product_id(), output.product_id());
  EXPECT_EQ(input.modes().size(), output.modes().size());

  for (size_t i = 0; i < input.modes().size(); i++)
    EXPECT_TRUE(CompareModes(*input.modes()[i], *output.modes()[i]));

  EXPECT_EQ(input.edid(), output.edid());

  if (!input.current_mode())
    EXPECT_EQ(nullptr, output.current_mode());
  else
    EXPECT_TRUE(CompareModes(*input.current_mode(), *output.current_mode()));

  if (!input.native_mode())
    EXPECT_EQ(nullptr, output.native_mode());
  else
    EXPECT_TRUE(CompareModes(*input.native_mode(), *output.native_mode()));

  EXPECT_EQ(input.maximum_cursor_size(), output.maximum_cursor_size());
}

}  // namespace

TEST_F(DisplayStructTraitsTest, DefaultDisplayValues) {
  Display input(5);

  Display output;
  GetTraitsTestProxy()->EchoDisplay(input, &output);

  CheckDisplaysEqual(input, output);
}

TEST_F(DisplayStructTraitsTest, SetAllDisplayValues) {
  const gfx::Rect bounds(100, 200, 500, 600);
  const gfx::Rect work_area(150, 250, 400, 500);
  const gfx::Size maximum_cursor_size(64, 64);

  Display input(246345234, bounds);
  input.set_work_area(work_area);
  input.set_device_scale_factor(2.0f);
  input.set_rotation(Display::ROTATE_270);
  input.set_touch_support(Display::TOUCH_SUPPORT_AVAILABLE);
  input.set_maximum_cursor_size(maximum_cursor_size);

  Display output;
  GetTraitsTestProxy()->EchoDisplay(input, &output);

  CheckDisplaysEqual(input, output);
}

TEST_F(DisplayStructTraitsTest, DefaultDisplayMode) {
  std::unique_ptr<DisplayMode> input =
      base::MakeUnique<DisplayMode>(gfx::Size(1024, 768), true, 61.0);

  mojom::DisplayStructTraitsTestPtr proxy = GetTraitsTestProxy();
  std::unique_ptr<DisplayMode> output;

  proxy->EchoDisplayMode(input->Clone(), &output);

  // We want to test each component individually to make sure each data member
  // was correctly serialized and deserialized.
  EXPECT_EQ(input->size(), output->size());
  EXPECT_EQ(input->is_interlaced(), output->is_interlaced());
  EXPECT_EQ(input->refresh_rate(), output->refresh_rate());
}

TEST_F(DisplayStructTraitsTest, DisplayPlacementFlushAtTop) {
  DisplayPlacement input;
  input.display_id = kDisplayId1;
  input.parent_display_id = kDisplayId2;
  input.position = DisplayPlacement::TOP;
  input.offset = 0;
  input.offset_reference = DisplayPlacement::TOP_LEFT;

  DisplayPlacement output;
  GetTraitsTestProxy()->EchoDisplayPlacement(input, &output);

  EXPECT_EQ(input, output);
}

TEST_F(DisplayStructTraitsTest, DisplayPlacementWithOffset) {
  DisplayPlacement input;
  input.display_id = kDisplayId1;
  input.parent_display_id = kDisplayId2;
  input.position = DisplayPlacement::BOTTOM;
  input.offset = -100;
  input.offset_reference = DisplayPlacement::BOTTOM_RIGHT;

  DisplayPlacement output;
  GetTraitsTestProxy()->EchoDisplayPlacement(input, &output);

  EXPECT_EQ(input, output);
}

TEST_F(DisplayStructTraitsTest, DisplayLayoutTwoExtended) {
  DisplayPlacement placement;
  placement.display_id = kDisplayId1;
  placement.parent_display_id = kDisplayId2;
  placement.position = DisplayPlacement::RIGHT;
  placement.offset = 0;
  placement.offset_reference = DisplayPlacement::TOP_LEFT;

  auto input = base::MakeUnique<DisplayLayout>();
  input->placement_list.push_back(placement);
  input->primary_id = kDisplayId2;
  input->mirrored = false;
  input->default_unified = true;

  std::unique_ptr<DisplayLayout> output;
  GetTraitsTestProxy()->EchoDisplayLayout(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

TEST_F(DisplayStructTraitsTest, DisplayLayoutThreeExtended) {
  DisplayPlacement placement1;
  placement1.display_id = kDisplayId2;
  placement1.parent_display_id = kDisplayId1;
  placement1.position = DisplayPlacement::LEFT;
  placement1.offset = 0;
  placement1.offset_reference = DisplayPlacement::TOP_LEFT;

  DisplayPlacement placement2;
  placement2.display_id = kDisplayId3;
  placement2.parent_display_id = kDisplayId1;
  placement2.position = DisplayPlacement::RIGHT;
  placement2.offset = -100;
  placement2.offset_reference = DisplayPlacement::BOTTOM_RIGHT;

  auto input = base::MakeUnique<DisplayLayout>();
  input->placement_list.push_back(placement1);
  input->placement_list.push_back(placement2);
  input->primary_id = kDisplayId1;
  input->mirrored = false;
  input->default_unified = false;

  std::unique_ptr<DisplayLayout> output;
  GetTraitsTestProxy()->EchoDisplayLayout(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

TEST_F(DisplayStructTraitsTest, DisplayLayoutTwoMirrored) {
  DisplayPlacement placement;
  placement.display_id = kDisplayId1;
  placement.parent_display_id = kDisplayId2;
  placement.position = DisplayPlacement::RIGHT;
  placement.offset = 0;
  placement.offset_reference = DisplayPlacement::TOP_LEFT;

  auto input = base::MakeUnique<DisplayLayout>();
  input->placement_list.push_back(placement);
  input->primary_id = kDisplayId2;
  input->mirrored = true;
  input->default_unified = true;

  std::unique_ptr<DisplayLayout> output;
  GetTraitsTestProxy()->EchoDisplayLayout(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

TEST_F(DisplayStructTraitsTest, BasicGammaRampRGBEntry) {
  const GammaRampRGBEntry input{259, 81, 16};

  GammaRampRGBEntry output;
  GetTraitsTestProxy()->EchoGammaRampRGBEntry(input, &output);

  EXPECT_EQ(input.r, output.r);
  EXPECT_EQ(input.g, output.g);
  EXPECT_EQ(input.b, output.b);
}

// One display mode, current and native mode nullptr.
TEST_F(DisplayStructTraitsTest, DisplaySnapshotCurrentAndNativeModesNull) {
  // Prepare sample input with random values.
  const int64_t display_id = 7;
  const gfx::Point origin(1, 2);
  const gfx::Size physical_size(5, 9);
  const gfx::Size maximum_cursor_size(3, 5);
  const DisplayConnectionType type =
      display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
  const bool is_aspect_preserving_scaling = true;
  const bool has_overscan = true;
  const bool has_color_correction_matrix = true;
  const std::string display_name("whatever display_name");
  const base::FilePath sys_path = base::FilePath::FromUTF8Unsafe("a/cb");
  const int64_t product_id = 19;

  const DisplayMode display_mode(gfx::Size(13, 11), true, 40.0f);

  display::DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());

  const DisplayMode* current_mode = nullptr;
  const DisplayMode* native_mode = nullptr;
  const std::vector<uint8_t> edid = {1};

  std::unique_ptr<DisplaySnapshotMojo> input =
      base::MakeUnique<DisplaySnapshotMojo>(
          display_id, origin, physical_size, type, is_aspect_preserving_scaling,
          has_overscan, has_color_correction_matrix, display_name, sys_path,
          product_id, std::move(modes), edid, current_mode, native_mode,
          maximum_cursor_size);

  std::unique_ptr<DisplaySnapshotMojo> output;
  GetTraitsTestProxy()->EchoDisplaySnapshotMojo(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

// One display mode that is the native mode and no current mode.
TEST_F(DisplayStructTraitsTest, DisplaySnapshotCurrentModeNull) {
  // Prepare sample input with random values.
  const int64_t display_id = 6;
  const gfx::Point origin(11, 32);
  const gfx::Size physical_size(55, 49);
  const gfx::Size maximum_cursor_size(13, 95);
  const DisplayConnectionType type = display::DISPLAY_CONNECTION_TYPE_VGA;
  const bool is_aspect_preserving_scaling = true;
  const bool has_overscan = true;
  const bool has_color_correction_matrix = true;
  const std::string display_name("whatever display_name");
  const base::FilePath sys_path = base::FilePath::FromUTF8Unsafe("z/b");
  const int64_t product_id = 9;

  const DisplayMode display_mode(gfx::Size(13, 11), true, 50.0f);

  display::DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());

  const DisplayMode* current_mode = nullptr;
  const DisplayMode* native_mode = modes[0].get();
  const std::vector<uint8_t> edid = {1};

  std::unique_ptr<DisplaySnapshotMojo> input =
      base::MakeUnique<DisplaySnapshotMojo>(
          display_id, origin, physical_size, type, is_aspect_preserving_scaling,
          has_overscan, has_color_correction_matrix, display_name, sys_path,
          product_id, std::move(modes), edid, current_mode, native_mode,
          maximum_cursor_size);

  std::unique_ptr<DisplaySnapshotMojo> output;
  GetTraitsTestProxy()->EchoDisplaySnapshotMojo(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

// Multiple display modes, both native and current mode set.
TEST_F(DisplayStructTraitsTest, DisplaySnapshotExternal) {
  // Prepare sample input from external display.
  const int64_t display_id = 9834293210466051;
  const gfx::Point origin(0, 1760);
  const gfx::Size physical_size(520, 320);
  const gfx::Size maximum_cursor_size(4, 5);
  const DisplayConnectionType type = display::DISPLAY_CONNECTION_TYPE_HDMI;
  const bool is_aspect_preserving_scaling = false;
  const bool has_overscan = false;
  const bool has_color_correction_matrix = false;
  const std::string display_name("HP Z24i");
  const base::FilePath sys_path = base::FilePath::FromUTF8Unsafe("a/cb");
  const int64_t product_id = 139;

  const DisplayMode display_mode(gfx::Size(1024, 768), false, 60.0f);
  const DisplayMode display_current_mode(gfx::Size(1440, 900), false, 59.89f);
  const DisplayMode display_native_mode(gfx::Size(1920, 1200), false, 59.89f);

  display::DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());
  modes.push_back(display_current_mode.Clone());
  modes.push_back(display_native_mode.Clone());

  const DisplayMode* current_mode = modes[1].get();
  const DisplayMode* native_mode = modes[2].get();
  const std::vector<uint8_t> edid = {2, 3, 4, 5};

  std::unique_ptr<DisplaySnapshotMojo> input =
      base::MakeUnique<DisplaySnapshotMojo>(
          display_id, origin, physical_size, type, is_aspect_preserving_scaling,
          has_overscan, has_color_correction_matrix, display_name, sys_path,
          product_id, std::move(modes), edid, current_mode, native_mode,
          maximum_cursor_size);

  std::unique_ptr<DisplaySnapshotMojo> output;
  GetTraitsTestProxy()->EchoDisplaySnapshotMojo(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

TEST_F(DisplayStructTraitsTest, DisplaySnapshotInternal) {
  // Prepare sample input from Pixel's internal display.
  const int64_t display_id = 13761487533244416;
  const gfx::Point origin(0, 0);
  const gfx::Size physical_size(270, 180);
  const gfx::Size maximum_cursor_size(64, 64);
  const DisplayConnectionType type = display::DISPLAY_CONNECTION_TYPE_INTERNAL;
  const bool is_aspect_preserving_scaling = true;
  const bool has_overscan = false;
  const bool has_color_correction_matrix = false;
  const std::string display_name("");
  const base::FilePath sys_path;
  const int64_t product_id = 139;

  const DisplayMode display_mode(gfx::Size(2560, 1700), false, 95.96f);

  display::DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());

  const DisplayMode* current_mode = modes[0].get();
  const DisplayMode* native_mode = modes[0].get();
  const std::vector<uint8_t> edid = {2, 3};

  std::unique_ptr<DisplaySnapshotMojo> input =
      base::MakeUnique<DisplaySnapshotMojo>(
          display_id, origin, physical_size, type, is_aspect_preserving_scaling,
          has_overscan, has_color_correction_matrix, display_name, sys_path,
          product_id, std::move(modes), edid, current_mode, native_mode,
          maximum_cursor_size);

  std::unique_ptr<DisplaySnapshotMojo> output;
  GetTraitsTestProxy()->EchoDisplaySnapshotMojo(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

TEST_F(DisplayStructTraitsTest, HDCPStateBasic) {
  const display::HDCPState input(HDCP_STATE_ENABLED);
  display::HDCPState output;
  GetTraitsTestProxy()->EchoHDCPState(input, &output);
  EXPECT_EQ(input, output);
}

}  // namespace display