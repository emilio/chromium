// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/system/devicemode.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace display {

namespace {

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 100;

struct DisplaySortFunctor {
  bool operator()(const Display& a, const Display& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

struct DisplayInfoSortFunctor {
  bool operator()(const ManagedDisplayInfo& a, const ManagedDisplayInfo& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

Display& GetInvalidDisplay() {
  static Display* invalid_display = new Display();
  return *invalid_display;
}

ManagedDisplayInfo::ManagedDisplayModeList::const_iterator FindDisplayMode(
    const ManagedDisplayInfo& info,
    const scoped_refptr<ManagedDisplayMode>& target_mode) {
  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  return std::find_if(
      modes.begin(), modes.end(),
      [target_mode](const scoped_refptr<ManagedDisplayMode>& mode) {
        return target_mode->IsEquivalent(mode);
      });
}

void SetInternalManagedDisplayModeList(ManagedDisplayInfo* info) {
  scoped_refptr<ManagedDisplayMode> native_mode = new ManagedDisplayMode(
      info->bounds_in_native().size(), 0.0 /* refresh_rate */,
      false /* interlaced */, false /* native_mode */, 1.0 /* ui_scale */,
      info->device_scale_factor());
  info->SetManagedDisplayModes(
      CreateInternalManagedDisplayModeList(native_mode));
}

void MaybeInitInternalDisplay(ManagedDisplayInfo* info) {
  int64_t id = info->id();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kUseFirstDisplayAsInternal)) {
    Display::SetInternalDisplayId(id);
    SetInternalManagedDisplayModeList(info);
  }
}

gfx::Size GetMaxNativeSize(const ManagedDisplayInfo& info) {
  gfx::Size size;
  for (auto& mode : info.display_modes()) {
    if (mode->size().GetArea() > size.GetArea())
      size = mode->size();
  }
  return size;
}

scoped_refptr<ManagedDisplayMode> GetDefaultDisplayMode(
    const ManagedDisplayInfo& info) {
  const auto& modes = info.display_modes();
  auto iter = std::find_if(modes.begin(), modes.end(),
                           [](const scoped_refptr<ManagedDisplayMode>& mode) {
                             return mode->is_default();
                           });

  if (iter == modes.end())
    return scoped_refptr<ManagedDisplayMode>();
  return *iter;
}

}  // namespace

using std::string;
using std::vector;

// static
int64_t DisplayManager::kUnifiedDisplayId = -10;

DisplayManager::DisplayManager(std::unique_ptr<Screen> screen)
    : screen_(std::move(screen)),
      layout_store_(new DisplayLayoutStore),
      weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  configure_displays_ = chromeos::IsRunningAsSystemCompositor();
  change_display_upon_host_resize_ = !configure_displays_;
  unified_desktop_enabled_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableUnifiedDesktop);
#endif
}

DisplayManager::~DisplayManager() {
#if defined(OS_CHROMEOS)
  // Reset the font params.
  gfx::SetFontRenderParamsDeviceScaleFactor(1.0f);
#endif
}

bool DisplayManager::InitFromCommandLine() {
  DisplayInfoList info_list;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kHostWindowBounds))
    return false;
  const string size_str =
      command_line->GetSwitchValueASCII(::switches::kHostWindowBounds);
  for (const std::string& part : base::SplitString(
           size_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    info_list.push_back(ManagedDisplayInfo::CreateFromSpec(part));
    info_list.back().set_native(true);
  }
  MaybeInitInternalDisplay(&info_list[0]);
  if (info_list.size() > 1 &&
      command_line->HasSwitch(::switches::kEnableSoftwareMirroring)) {
    SetMultiDisplayMode(MIRRORING);
  }
  OnNativeDisplaysChanged(info_list);
  return true;
}

void DisplayManager::InitDefaultDisplay() {
  DisplayInfoList info_list;
  info_list.push_back(ManagedDisplayInfo::CreateFromSpec(std::string()));
  info_list.back().set_native(true);
  MaybeInitInternalDisplay(&info_list[0]);
  OnNativeDisplaysChanged(info_list);
}

void DisplayManager::RefreshFontParams() {
#if defined(OS_CHROMEOS)
  // Use the largest device scale factor among currently active displays. Non
  // internal display may have bigger scale factor in case the external display
  // is an 4K display.
  float largest_device_scale_factor = 1.0f;
  for (const Display& display : active_display_list_) {
    const ManagedDisplayInfo& info = display_info_[display.id()];
    largest_device_scale_factor = std::max(
        largest_device_scale_factor, info.GetEffectiveDeviceScaleFactor());
  }
  gfx::SetFontRenderParamsDeviceScaleFactor(largest_device_scale_factor);
#endif  // OS_CHROMEOS
}

const DisplayLayout& DisplayManager::GetCurrentDisplayLayout() const {
  DCHECK_LE(2U, num_connected_displays());
  if (num_connected_displays() > 1) {
    DisplayIdList list = GetCurrentDisplayIdList();
    return layout_store_->GetRegisteredDisplayLayout(list);
  }
  LOG(ERROR) << "DisplayLayout is requested for single display";
  // On release build, just fallback to default instead of blowing up.
  static DisplayLayout layout;
  layout.primary_id = active_display_list_[0].id();
  return layout;
}

const DisplayLayout& DisplayManager::GetCurrentResolvedDisplayLayout() const {
  return current_resolved_layout_ ? *current_resolved_layout_
                                  : GetCurrentDisplayLayout();
}

DisplayIdList DisplayManager::GetCurrentDisplayIdList() const {
  if (IsInUnifiedMode()) {
    return CreateDisplayIdList(software_mirroring_display_list_);
  } else if (IsInMirrorMode()) {
    if (software_mirroring_enabled()) {
      CHECK_EQ(2u, num_connected_displays());
      // This comment is to make it easy to distinguish the crash
      // between two checks.
      CHECK_EQ(1u, active_display_list_.size());
    }
    int64_t ids[] = {active_display_list_[0].id(), mirroring_display_id_};
    return GenerateDisplayIdList(std::begin(ids), std::end(ids));
  } else {
    CHECK_LE(2u, active_display_list_.size());
    return CreateDisplayIdList(active_display_list_);
  }
}

void DisplayManager::SetLayoutForCurrentDisplays(
    std::unique_ptr<DisplayLayout> layout) {
  if (GetNumDisplays() == 1)
    return;
  const DisplayIdList list = GetCurrentDisplayIdList();

  DCHECK(DisplayLayout::Validate(list, *layout));

  const DisplayLayout& current_layout =
      layout_store_->GetRegisteredDisplayLayout(list);

  if (layout->HasSamePlacementList(current_layout))
    return;

  layout_store_->RegisterLayoutForDisplayIdList(list, std::move(layout));
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);

  // TODO(oshima): Call UpdateDisplays instead.
  std::vector<int64_t> updated_ids;
  current_resolved_layout_ = GetCurrentDisplayLayout().Copy();
  ApplyDisplayLayout(current_resolved_layout_.get(), &active_display_list_,
                     &updated_ids);
  for (int64_t id : updated_ids) {
    NotifyMetricsChanged(GetDisplayForId(id),
                         DisplayObserver::DISPLAY_METRIC_BOUNDS |
                             DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  }

  if (delegate_)
    delegate_->PostDisplayConfigurationChange(false);
}

const Display& DisplayManager::GetDisplayForId(int64_t id) const {
  Display* display = const_cast<DisplayManager*>(this)->FindDisplayForId(id);
  return display ? *display : GetInvalidDisplay();
}

const Display& DisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  auto iter = display::FindDisplayContainingPoint(active_display_list_,
                                                  point_in_screen);
  return iter == active_display_list_.end() ? GetInvalidDisplay() : *iter;
}

bool DisplayManager::UpdateWorkAreaOfDisplay(int64_t display_id,
                                             const gfx::Insets& insets) {
  Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  gfx::Rect old_work_area = display->work_area();
  display->UpdateWorkAreaFromInsets(insets);
  bool workarea_changed = old_work_area != display->work_area();
  if (workarea_changed) {
    NotifyMetricsChanged(*display, DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  }
  return workarea_changed;
}

void DisplayManager::SetOverscanInsets(int64_t display_id,
                                       const gfx::Insets& insets_in_dip) {
  bool update = false;
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (insets_in_dip.IsEmpty()) {
        info.set_clear_overscan_insets(true);
      } else {
        info.set_clear_overscan_insets(false);
        info.SetOverscanInsets(insets_in_dip);
      }
      update = true;
    }
    display_info_list.push_back(info);
  }
  if (update) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else {
    display_info_[display_id].SetOverscanInsets(insets_in_dip);
  }
}

void DisplayManager::SetDisplayRotation(int64_t display_id,
                                        Display::Rotation rotation,
                                        Display::RotationSource source) {
  if (IsInUnifiedMode())
    return;

  DisplayInfoList display_info_list;
  bool is_active = false;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (info.GetRotation(source) == rotation &&
          info.GetActiveRotation() == rotation) {
        return;
      }
      info.SetRotation(rotation, source);
      is_active = true;
    }
    display_info_list.push_back(info);
  }
  if (is_active) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else if (display_info_.find(display_id) != display_info_.end()) {
    // Inactive displays can reactivate, ensure they have been updated.
    display_info_[display_id].SetRotation(rotation, source);
  }
}

bool DisplayManager::SetDisplayMode(
    int64_t display_id,
    const scoped_refptr<ManagedDisplayMode>& display_mode) {
  bool change_ui_scale = GetDisplayIdForUIScaling() == display_id;

  DisplayInfoList display_info_list;
  bool display_property_changed = false;
  bool resolution_changed = false;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      auto iter = FindDisplayMode(info, display_mode);
      if (iter == info.display_modes().end()) {
        LOG(WARNING) << "Unsupported display mode was requested:"
                     << "size=" << display_mode->size().ToString()
                     << ", ui scale=" << display_mode->ui_scale()
                     << ", scale factor="
                     << display_mode->device_scale_factor();
        return false;
      }

      if (change_ui_scale) {
        if (info.configured_ui_scale() == display_mode->ui_scale())
          return true;
        info.set_configured_ui_scale(display_mode->ui_scale());
        display_property_changed = true;
      } else {
        display_modes_[display_id] = *iter;
        if (info.bounds_in_native().size() != display_mode->size())
          resolution_changed = true;
        if (info.device_scale_factor() != display_mode->device_scale_factor()) {
          info.set_device_scale_factor(display_mode->device_scale_factor());
          display_property_changed = true;
        }
      }
    }
    display_info_list.push_back(info);
  }
  if (display_property_changed) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  }
  if (resolution_changed && IsInUnifiedMode()) {
    ReconfigureDisplays();
#if defined(OS_CHROMEOS)
  } else if (resolution_changed && configure_displays_) {
    delegate_->display_configurator()->OnConfigurationChanged();
#endif
  }
  return resolution_changed || display_property_changed;
}

void DisplayManager::RegisterDisplayProperty(
    int64_t display_id,
    Display::Rotation rotation,
    float ui_scale,
    const gfx::Insets* overscan_insets,
    const gfx::Size& resolution_in_pixels,
    float device_scale_factor,
    ColorCalibrationProfile color_profile,
    const TouchCalibrationData* touch_calibration_data) {
  if (display_info_.find(display_id) == display_info_.end())
    display_info_[display_id] =
        ManagedDisplayInfo(display_id, std::string(), false);

  // Do not allow rotation in unified desktop mode.
  if (display_id == kUnifiedDisplayId)
    rotation = Display::ROTATE_0;

  display_info_[display_id].SetRotation(rotation,
                                        Display::ROTATION_SOURCE_USER);
  display_info_[display_id].SetRotation(rotation,
                                        Display::ROTATION_SOURCE_ACTIVE);
  display_info_[display_id].SetColorProfile(color_profile);
  // Just in case the preference file was corrupted.
  // TODO(mukai): register |display_modes_| here as well, so the lookup for the
  // default mode in GetActiveModeForDisplayId() gets much simpler.
  if (0.5f <= ui_scale && ui_scale <= 2.0f)
    display_info_[display_id].set_configured_ui_scale(ui_scale);
  if (overscan_insets)
    display_info_[display_id].SetOverscanInsets(*overscan_insets);
  if (touch_calibration_data)
    display_info_[display_id].SetTouchCalibrationData(*touch_calibration_data);
  if (!resolution_in_pixels.IsEmpty()) {
    DCHECK(!Display::IsInternalDisplayId(display_id));
    // Default refresh rate, until OnNativeDisplaysChanged() updates us with the
    // actual display info, is 60 Hz.
    scoped_refptr<ManagedDisplayMode> mode = new ManagedDisplayMode(
        resolution_in_pixels, 60.0f, false, false, 1.0, device_scale_factor);
    display_modes_[display_id] = mode;
  }
}

scoped_refptr<ManagedDisplayMode> DisplayManager::GetActiveModeForDisplayId(
    int64_t display_id) const {
  scoped_refptr<ManagedDisplayMode> selected_mode(
      GetSelectedModeForDisplayId(display_id));
  if (selected_mode)
    return selected_mode;

  // If 'selected' mode is empty, it should return the default mode. This means
  // the native mode for the external display. Unfortunately this is not true
  // for the internal display because restoring UI-scale doesn't register the
  // restored mode to |display_mode_|, so it needs to look up the mode whose
  // UI-scale value matches. See the TODO in RegisterDisplayProperty().
  const ManagedDisplayInfo& info = GetDisplayInfo(display_id);

  for (auto& mode : info.display_modes()) {
    if (GetDisplayIdForUIScaling() == display_id) {
      if (info.configured_ui_scale() == mode->ui_scale())
        return mode.get();
    } else if (mode->native()) {
      return mode.get();
    }
  }
  return selected_mode;
}

void DisplayManager::RegisterDisplayRotationProperties(
    bool rotation_lock,
    Display::Rotation rotation) {
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  registered_internal_display_rotation_lock_ = rotation_lock;
  registered_internal_display_rotation_ = rotation;
  if (delegate_)
    delegate_->PostDisplayConfigurationChange(false);
}

scoped_refptr<ManagedDisplayMode> DisplayManager::GetSelectedModeForDisplayId(
    int64_t id) const {
  std::map<int64_t, scoped_refptr<ManagedDisplayMode>>::const_iterator iter =
      display_modes_.find(id);
  if (iter == display_modes_.end())
    return scoped_refptr<ManagedDisplayMode>();
  return iter->second;
}

bool DisplayManager::IsDisplayUIScalingEnabled() const {
  return GetDisplayIdForUIScaling() != kInvalidDisplayId;
}

gfx::Insets DisplayManager::GetOverscanInsets(int64_t display_id) const {
  std::map<int64_t, ManagedDisplayInfo>::const_iterator it =
      display_info_.find(display_id);
  return (it != display_info_.end()) ? it->second.overscan_insets_in_dip()
                                     : gfx::Insets();
}

void DisplayManager::SetColorCalibrationProfile(
    int64_t display_id,
    ColorCalibrationProfile profile) {
#if defined(OS_CHROMEOS)
  if (!display_info_[display_id].IsColorProfileAvailable(profile))
    return;

  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  // Just sets color profile if it's not running on ChromeOS (like tests).
  if (!configure_displays_ ||
      delegate_->display_configurator()->SetColorCalibrationProfile(display_id,
                                                                    profile)) {
    display_info_[display_id].SetColorProfile(profile);
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Display.ColorProfile", profile,
                              NUM_COLOR_PROFILES);
  }
  if (delegate_)
    delegate_->PostDisplayConfigurationChange(false);
#endif
}

void DisplayManager::OnNativeDisplaysChanged(
    const DisplayInfoList& updated_displays) {
  if (updated_displays.empty()) {
    VLOG(1) << "OnNativeDisplaysChanged(0): # of current displays="
            << active_display_list_.size();
    // If the device is booted without display, or chrome is started
    // without --ash-host-window-bounds on linux desktop, use the
    // default display.
    if (active_display_list_.empty()) {
      DisplayInfoList init_displays;
      init_displays.push_back(
          ManagedDisplayInfo::CreateFromSpec(std::string()));
      MaybeInitInternalDisplay(&init_displays[0]);
      OnNativeDisplaysChanged(init_displays);
    } else {
      // Otherwise don't update the displays when all displays are disconnected.
      // This happens when:
      // - the device is idle and powerd requested to turn off all displays.
      // - the device is suspended. (kernel turns off all displays)
      // - the internal display's brightness is set to 0 and no external
      //   display is connected.
      // - the internal display's brightness is 0 and external display is
      //   disconnected.
      // The display will be updated when one of displays is turned on, and the
      // display list will be updated correctly.
    }
    return;
  }
  first_display_id_ = updated_displays[0].id();
  std::set<gfx::Point> origins;

  if (updated_displays.size() == 1) {
    VLOG(1) << "OnNativeDisplaysChanged(1):" << updated_displays[0].ToString();
  } else {
    VLOG(1) << "OnNativeDisplaysChanged(" << updated_displays.size()
            << ") [0]=" << updated_displays[0].ToString()
            << ", [1]=" << updated_displays[1].ToString();
  }

  bool internal_display_connected = false;
  num_connected_displays_ = updated_displays.size();
  mirroring_display_id_ = kInvalidDisplayId;
  software_mirroring_display_list_.clear();
  DisplayInfoList new_display_info_list;
  for (DisplayInfoList::const_iterator iter = updated_displays.begin();
       iter != updated_displays.end(); ++iter) {
    if (!internal_display_connected)
      internal_display_connected = Display::IsInternalDisplayId(iter->id());
    // Mirrored monitors have the same origins.
    gfx::Point origin = iter->bounds_in_native().origin();
    if (origins.find(origin) != origins.end()) {
      InsertAndUpdateDisplayInfo(*iter);
      mirroring_display_id_ = iter->id();
    } else {
      origins.insert(origin);
      new_display_info_list.push_back(*iter);
    }

    scoped_refptr<ManagedDisplayMode> new_mode(new ManagedDisplayMode(
        iter->bounds_in_native().size(), 0.0 /* refresh rate */,
        false /* interlaced */, false /* native */, iter->configured_ui_scale(),
        iter->device_scale_factor()));
    const ManagedDisplayInfo::ManagedDisplayModeList& display_modes =
        iter->display_modes();
    // This is empty the displays are initialized from InitFromCommandLine.
    if (display_modes.empty())
      continue;
    auto display_modes_iter = FindDisplayMode(*iter, new_mode);
    // Update the actual resolution selected as the resolution request may fail.
    if (display_modes_iter == display_modes.end())
      display_modes_.erase(iter->id());
    else if (display_modes_.find(iter->id()) != display_modes_.end())
      display_modes_[iter->id()] = *display_modes_iter;
  }
  if (Display::HasInternalDisplay() && !internal_display_connected) {
    if (display_info_.find(Display::InternalDisplayId()) ==
        display_info_.end()) {
      // Create a dummy internal display if the chrome restarted
      // in docked mode.
      ManagedDisplayInfo internal_display_info(
          Display::InternalDisplayId(),
          l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_INTERNAL),
          false /*Internal display must not have overscan */);
      internal_display_info.SetBounds(gfx::Rect(0, 0, 800, 600));
      display_info_[Display::InternalDisplayId()] = internal_display_info;
    } else {
      // Internal display is no longer active. Reset its rotation to user
      // preference, so that it is restored when the internal display becomes
      // active again.
      Display::Rotation user_rotation =
          display_info_[Display::InternalDisplayId()].GetRotation(
              Display::ROTATION_SOURCE_USER);
      display_info_[Display::InternalDisplayId()].SetRotation(
          user_rotation, Display::ROTATION_SOURCE_USER);
    }
  }

#if defined(OS_CHROMEOS)
  if (!configure_displays_ && new_display_info_list.size() > 1) {
    DisplayIdList list = GenerateDisplayIdList(
        new_display_info_list.begin(), new_display_info_list.end(),
        [](const ManagedDisplayInfo& info) { return info.id(); });

    const DisplayLayout& layout =
        layout_store_->GetRegisteredDisplayLayout(list);
    // Mirror mode is set by DisplayConfigurator on the device.
    // Emulate it when running on linux desktop.
    if (layout.mirrored)
      SetMultiDisplayMode(MIRRORING);
  }
#endif

  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::UpdateDisplays() {
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplaysWith(display_info_list);
}

void DisplayManager::UpdateDisplaysWith(
    const DisplayInfoList& updated_display_info_list) {
#if defined(OS_WIN)
  DCHECK_EQ(1u, updated_display_info_list.size())
      << ": Multiple display test does not work on Windows bots. Please "
         "skip (don't disable) the test.";
#endif

  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(active_display_list_.begin(), active_display_list_.end(),
            DisplaySortFunctor());
  std::sort(new_display_info_list.begin(), new_display_info_list.end(),
            DisplayInfoSortFunctor());

  if (new_display_info_list.size() > 1) {
    DisplayIdList list = GenerateDisplayIdList(
        new_display_info_list.begin(), new_display_info_list.end(),
        [](const ManagedDisplayInfo& info) { return info.id(); });
    const DisplayLayout& layout =
        layout_store_->GetRegisteredDisplayLayout(list);
    current_default_multi_display_mode_ =
        (layout.default_unified && unified_desktop_enabled_) ? UNIFIED
                                                             : EXTENDED;
  }

  if (multi_display_mode_ != MIRRORING)
    multi_display_mode_ = current_default_multi_display_mode_;

  CreateSoftwareMirroringDisplayInfo(&new_display_info_list);

  // Close the mirroring window if any here to avoid creating two compositor on
  // one display.
  if (delegate_)
    delegate_->CloseMirroringDisplayIfNotNecessary();

  Displays new_displays;
  Displays removed_displays;
  std::map<size_t, uint32_t> display_changes;
  std::vector<size_t> added_display_indices;

  Displays::iterator curr_iter = active_display_list_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  while (curr_iter != active_display_list_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (curr_iter == active_display_list_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    } else if (new_info_iter == new_display_info_list.end()) {
      // more displays in current list.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else if (curr_iter->id() == new_info_iter->id()) {
      const Display& current_display = *curr_iter;
      // Copy the info because |InsertAndUpdateDisplayInfo| updates the
      // instance.
      const ManagedDisplayInfo current_display_info =
          GetDisplayInfo(current_display.id());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const ManagedDisplayInfo& new_display_info =
          GetDisplayInfo(new_display.id());

      uint32_t metrics = DisplayObserver::DISPLAY_METRIC_NONE;

      // At that point the new Display objects we have are not entirely updated,
      // they are missing the translation related to the Display disposition in
      // the layout.
      // Using display.bounds() and display.work_area() would fail most of the
      // time.
      if (force_bounds_changed_ || (current_display_info.bounds_in_native() !=
                                    new_display_info.bounds_in_native()) ||
          (current_display_info.GetOverscanInsetsInPixel() !=
           new_display_info.GetOverscanInsetsInPixel()) ||
          current_display.size() != new_display.size()) {
        metrics |= DisplayObserver::DISPLAY_METRIC_BOUNDS |
                   DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      }

      if (current_display.device_scale_factor() !=
          new_display.device_scale_factor()) {
        metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
      }

      if (current_display.rotation() != new_display.rotation())
        metrics |= DisplayObserver::DISPLAY_METRIC_ROTATION;

      if (metrics != DisplayObserver::DISPLAY_METRIC_NONE) {
        display_changes.insert(
            std::pair<size_t, uint32_t>(new_displays.size(), metrics));
      }

      new_display.UpdateWorkAreaFromInsets(current_display.GetWorkAreaInsets());
      new_displays.push_back(new_display);
      ++curr_iter;
      ++new_info_iter;
    } else if (curr_iter->id() < new_info_iter->id()) {
      // more displays in current list between ids, which means it is deleted.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else {
      // more displays in new list between ids, which means it is added.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    }
  }
  Display old_primary;
  if (delegate_)
    old_primary = screen_->GetPrimaryDisplay();

  // Clear focus if the display has been removed, but don't clear focus if
  // the destkop has been moved from one display to another
  // (mirror -> docked, docked -> single internal).
  bool clear_focus =
      !removed_displays.empty() &&
      !(removed_displays.size() == 1 && added_display_indices.size() == 1);
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(clear_focus);

  std::vector<size_t> updated_indices;
  UpdateNonPrimaryDisplayBoundsForLayout(&new_displays, &updated_indices);
  for (size_t updated_index : updated_indices) {
    if (std::find(added_display_indices.begin(), added_display_indices.end(),
                  updated_index) == added_display_indices.end()) {
      uint32_t metrics = DisplayObserver::DISPLAY_METRIC_BOUNDS |
                         DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      if (display_changes.find(updated_index) != display_changes.end())
        metrics |= display_changes[updated_index];

      display_changes[updated_index] = metrics;
    }
  }

  active_display_list_ = new_displays;
  active_only_display_list_ = active_display_list_;

  RefreshFontParams();
  base::AutoReset<bool> resetter(&change_display_upon_host_resize_, false);

  size_t active_display_list_size = active_display_list_.size();
  is_updating_display_list_ = true;
  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  active_display_list_.insert(active_display_list_.end(),
                              removed_displays.begin(), removed_displays.end());

  for (const auto& display : removed_displays)
    NotifyDisplayRemoved(display);

  for (size_t index : added_display_indices)
    NotifyDisplayAdded(active_display_list_[index]);

  active_display_list_.resize(active_display_list_size);
  is_updating_display_list_ = false;

  bool notify_primary_change =
      delegate_ ? old_primary.id() != screen_->GetPrimaryDisplay().id() : false;

  for (std::map<size_t, uint32_t>::iterator iter = display_changes.begin();
       iter != display_changes.end(); ++iter) {
    uint32_t metrics = iter->second;
    const Display& updated_display = active_display_list_[iter->first];

    if (notify_primary_change &&
        updated_display.id() == screen_->GetPrimaryDisplay().id()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_PRIMARY;
      notify_primary_change = false;
    }
    NotifyMetricsChanged(updated_display, metrics);
  }

  if (notify_primary_change) {
    // This happens when a primary display has moved to anther display without
    // bounds change.
    const Display& primary = screen_->GetPrimaryDisplay();
    if (primary.id() != old_primary.id()) {
      uint32_t metrics = DisplayObserver::DISPLAY_METRIC_PRIMARY;
      if (primary.size() != old_primary.size()) {
        metrics |= (DisplayObserver::DISPLAY_METRIC_BOUNDS |
                    DisplayObserver::DISPLAY_METRIC_WORK_AREA);
      }
      if (primary.device_scale_factor() != old_primary.device_scale_factor())
        metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;

      NotifyMetricsChanged(primary, metrics);
    }
  }

  bool must_clear_window = false;
#if defined(USE_X11) && defined(OS_CHROMEOS)
  must_clear_window =
      !display_changes.empty() && base::SysInfo::IsRunningOnChromeOS();
#endif

  if (delegate_)
    delegate_->PostDisplayConfigurationChange(must_clear_window);

  // Create the mirroring window asynchronously after all displays
  // are added so that it can mirror the display newly added. This can
  // happen when switching from dock mode to software mirror mode.
  CreateMirrorWindowAsyncIfAny();
}

const Display& DisplayManager::GetDisplayAt(size_t index) const {
  DCHECK_LT(index, active_display_list_.size());
  return active_display_list_[index];
}

const Display& DisplayManager::GetPrimaryDisplayCandidate() const {
  if (GetNumDisplays() != 2)
    return active_display_list_[0];
  const DisplayLayout& layout =
      layout_store_->GetRegisteredDisplayLayout(GetCurrentDisplayIdList());
  return GetDisplayForId(layout.primary_id);
}

size_t DisplayManager::GetNumDisplays() const {
  return active_display_list_.size();
}

bool DisplayManager::IsActiveDisplayId(int64_t display_id) const {
  return std::find_if(active_display_list_.begin(), active_display_list_.end(),
                      [display_id](const Display& display) {
                        return display.id() == display_id;
                      }) != active_display_list_.end();
}

bool DisplayManager::IsInMirrorMode() const {
  return mirroring_display_id_ != kInvalidDisplayId;
}

void DisplayManager::SetUnifiedDesktopEnabled(bool enable) {
  unified_desktop_enabled_ = enable;
  // There is no need to update the displays in mirror mode. Doing
  // this in hardware mirroring mode can cause crash because display
  // info in hardware mirroring comes from DisplayConfigurator.
  if (!IsInMirrorMode())
    ReconfigureDisplays();
}

bool DisplayManager::IsInUnifiedMode() const {
  return multi_display_mode_ == UNIFIED &&
         !software_mirroring_display_list_.empty();
}

const ManagedDisplayInfo& DisplayManager::GetDisplayInfo(
    int64_t display_id) const {
  DCHECK_NE(kInvalidDisplayId, display_id);

  std::map<int64_t, ManagedDisplayInfo>::const_iterator iter =
      display_info_.find(display_id);
  CHECK(iter != display_info_.end()) << display_id;
  return iter->second;
}

const Display DisplayManager::GetMirroringDisplayById(
    int64_t display_id) const {
  auto iter = std::find_if(software_mirroring_display_list_.begin(),
                           software_mirroring_display_list_.end(),
                           [display_id](const Display& display) {
                             return display.id() == display_id;
                           });
  return iter == software_mirroring_display_list_.end() ? Display() : *iter;
}

std::string DisplayManager::GetDisplayNameForId(int64_t id) {
  if (id == kInvalidDisplayId)
    return l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_UNKNOWN);

  std::map<int64_t, ManagedDisplayInfo>::const_iterator iter =
      display_info_.find(id);
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(id));
}

int64_t DisplayManager::GetDisplayIdForUIScaling() const {
  // UI Scaling is effective on internal display.
  return Display::HasInternalDisplay() ? Display::InternalDisplayId()
                                       : kInvalidDisplayId;
}

void DisplayManager::SetMirrorMode(bool mirror) {
  // TODO(oshima): Enable mirror mode for 2> displays. crbug.com/589319.
  if (num_connected_displays() != 2)
    return;

#if defined(OS_CHROMEOS)
  if (configure_displays_) {
    MultipleDisplayState new_state = mirror
                                         ? MULTIPLE_DISPLAY_STATE_DUAL_MIRROR
                                         : MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
    delegate_->display_configurator()->SetDisplayMode(new_state);
    return;
  }
#endif
  multi_display_mode_ =
      mirror ? MIRRORING : current_default_multi_display_mode_;
  ReconfigureDisplays();
}

void DisplayManager::AddRemoveDisplay() {
  DCHECK(!active_display_list_.empty());
  DisplayInfoList new_display_info_list;
  const ManagedDisplayInfo& first_display =
      IsInUnifiedMode()
          ? GetDisplayInfo(software_mirroring_display_list_[0].id())
          : GetDisplayInfo(active_display_list_[0].id());
  new_display_info_list.push_back(first_display);
  // Add if there is only one display connected.
  if (num_connected_displays() == 1) {
    const int kVerticalOffsetPx = 100;
    // Layout the 2nd display below the primary as with the real device.
    gfx::Rect host_bounds = first_display.bounds_in_native();
    new_display_info_list.push_back(
        ManagedDisplayInfo::CreateFromSpec(base::StringPrintf(
            "%d+%d-600x%d", host_bounds.x(),
            host_bounds.bottom() + kVerticalOffsetPx, host_bounds.height())));
  }
  num_connected_displays_ = new_display_info_list.size();
  mirroring_display_id_ = kInvalidDisplayId;
  software_mirroring_display_list_.clear();
  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::ToggleDisplayScaleFactor() {
  DCHECK(!active_display_list_.empty());
  DisplayInfoList new_display_info_list;
  for (Displays::const_iterator iter = active_display_list_.begin();
       iter != active_display_list_.end(); ++iter) {
    ManagedDisplayInfo display_info = GetDisplayInfo(iter->id());
    display_info.set_device_scale_factor(
        display_info.device_scale_factor() == 1.0f ? 2.0f : 1.0f);
    new_display_info_list.push_back(display_info);
  }
  AddMirrorDisplayInfoIfAny(&new_display_info_list);
  UpdateDisplaysWith(new_display_info_list);
}

#if defined(OS_CHROMEOS)
void DisplayManager::SetSoftwareMirroring(bool enabled) {
  SetMultiDisplayMode(enabled ? MIRRORING
                              : current_default_multi_display_mode_);
}

bool DisplayManager::SoftwareMirroringEnabled() const {
  return software_mirroring_enabled();
}

void DisplayManager::SetTouchCalibrationData(
    int64_t display_id,
    const TouchCalibrationData::CalibrationPointPairQuad& point_pair_quad,
    const gfx::Size& display_bounds) {
  bool update = false;
  TouchCalibrationData calibration_data(point_pair_quad, display_bounds);
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      info.SetTouchCalibrationData(calibration_data);
      update = true;
    }
    display_info_list.push_back(info);
  }
  if (update)
    UpdateDisplaysWith(display_info_list);
  else
    display_info_[display_id].SetTouchCalibrationData(calibration_data);
}

void DisplayManager::ClearTouchCalibrationData(int64_t display_id) {
  bool update = false;
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      info.clear_touch_calibration_data();
      update = true;
    }
    display_info_list.push_back(info);
  }
  if (update)
    UpdateDisplaysWith(display_info_list);
  else
    display_info_[display_id].clear_touch_calibration_data();
}
#endif

void DisplayManager::SetDefaultMultiDisplayModeForCurrentDisplays(
    MultiDisplayMode mode) {
  DCHECK_NE(MIRRORING, mode);
  DisplayIdList list = GetCurrentDisplayIdList();
  layout_store_->UpdateMultiDisplayState(list, IsInMirrorMode(),
                                         mode == UNIFIED);
  ReconfigureDisplays();
}

void DisplayManager::SetMultiDisplayMode(MultiDisplayMode mode) {
  multi_display_mode_ = mode;
  mirroring_display_id_ = kInvalidDisplayId;
  software_mirroring_display_list_.clear();
}

void DisplayManager::ReconfigureDisplays() {
  DisplayInfoList display_info_list;
  for (const Display& display : active_display_list_) {
    if (display.id() == kUnifiedDisplayId)
      continue;
    display_info_list.push_back(GetDisplayInfo(display.id()));
  }
  for (const Display& display : software_mirroring_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  mirroring_display_id_ = kInvalidDisplayId;
  software_mirroring_display_list_.clear();
  UpdateDisplaysWith(display_info_list);
}

bool DisplayManager::UpdateDisplayBounds(int64_t display_id,
                                         const gfx::Rect& new_bounds) {
  if (change_display_upon_host_resize_) {
    display_info_[display_id].SetBounds(new_bounds);
    // Don't notify observers if the mirrored window has changed.
    if (software_mirroring_enabled() && mirroring_display_id_ == display_id)
      return false;
    Display* display = FindDisplayForId(display_id);
    display->SetSize(display_info_[display_id].size_in_pixel());
    NotifyMetricsChanged(*display, DisplayObserver::DISPLAY_METRIC_BOUNDS);
    return true;
  }
  return false;
}

void DisplayManager::CreateMirrorWindowAsyncIfAny() {
  // Do not post a task if the software mirroring doesn't exist, or
  // during initialization when compositor's init task isn't posted yet.
  // ash::Shell::Init() will call this after the compositor is initialized.
  if (software_mirroring_display_list_.empty() || !delegate_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DisplayManager::CreateMirrorWindowIfAny,
                            weak_ptr_factory_.GetWeakPtr()));
}

void DisplayManager::UpdateInternalManagedDisplayModeListForTest() {
  if (!Display::HasInternalDisplay() ||
      display_info_.count(Display::InternalDisplayId()) == 0)
    return;
  ManagedDisplayInfo* info = &display_info_[Display::InternalDisplayId()];
  SetInternalManagedDisplayModeList(info);
}

bool DisplayManager::ZoomInternalDisplay(bool up) {
  int64_t display_id =
      IsInUnifiedMode() ? kUnifiedDisplayId : GetDisplayIdForUIScaling();
  const ManagedDisplayInfo& display_info = GetDisplayInfo(display_id);

  scoped_refptr<ManagedDisplayMode> mode;
  if (IsInUnifiedMode()) {
    mode = GetDisplayModeForNextResolution(display_info, up);
  } else {
    if (!IsActiveDisplayId(display_info.id()) ||
        !Display::IsInternalDisplayId(display_info.id())) {
      return false;
    }
    mode = GetDisplayModeForNextUIScale(display_info, up);
  }

  return mode ? SetDisplayMode(display_id, mode) : false;
}

bool DisplayManager::ResetDisplayToDefaultMode(int64_t id) {
  if (!IsActiveDisplayId(id) || !Display::IsInternalDisplayId(id))
    return false;

  const ManagedDisplayInfo& info = GetDisplayInfo(id);
  scoped_refptr<ManagedDisplayMode> mode = GetDefaultDisplayMode(info);

  return mode ? SetDisplayMode(id, mode) : false;
}

void DisplayManager::ResetInternalDisplayZoom() {
  if (IsInUnifiedMode()) {
    const ManagedDisplayInfo& display_info =
        GetDisplayInfo(DisplayManager::kUnifiedDisplayId);
    const ManagedDisplayInfo::ManagedDisplayModeList& modes =
        display_info.display_modes();
    auto iter = std::find_if(modes.begin(), modes.end(),
                             [](const scoped_refptr<ManagedDisplayMode>& mode) {
                               return mode->native();
                             });
    SetDisplayMode(kUnifiedDisplayId, *iter);
  } else {
    ResetDisplayToDefaultMode(GetDisplayIdForUIScaling());
  }
}

void DisplayManager::CreateSoftwareMirroringDisplayInfo(
    DisplayInfoList* display_info_list) {
  // Use the internal display or 1st as the mirror source, then scale
  // the root window so that it matches the external display's
  // resolution. This is necessary in order for scaling to work while
  // mirrored.
  switch (multi_display_mode_) {
    case MIRRORING: {
      if (display_info_list->size() != 2)
        return;
      bool zero_is_source =
          first_display_id_ == (*display_info_list)[0].id() ||
          Display::IsInternalDisplayId((*display_info_list)[0].id());
      DCHECK_EQ(MIRRORING, multi_display_mode_);
      mirroring_display_id_ = (*display_info_list)[zero_is_source ? 1 : 0].id();

      int64_t display_id = mirroring_display_id_;
      auto iter =
          std::find_if(display_info_list->begin(), display_info_list->end(),
                       [display_id](const ManagedDisplayInfo& info) {
                         return info.id() == display_id;
                       });
      DCHECK(iter != display_info_list->end());

      ManagedDisplayInfo info = *iter;
      info.SetOverscanInsets(gfx::Insets());
      InsertAndUpdateDisplayInfo(info);
      software_mirroring_display_list_.push_back(
          CreateMirroringDisplayFromDisplayInfoById(mirroring_display_id_,
                                                    gfx::Point(), 1.0f));
      display_info_list->erase(iter);
      break;
    }
    case UNIFIED: {
      if (display_info_list->size() == 1)
        return;
      // TODO(oshima): Currently, all displays are laid out horizontally,
      // from left to right. Allow more flexible layouts, such as
      // right to left, or vertical layouts.
      gfx::Rect unified_bounds;
      software_mirroring_display_list_.clear();
      // 1st Pass. Find the max size.
      int max_height = std::numeric_limits<int>::min();

      int default_height = 0;
      float default_device_scale_factor = 1.0f;
      for (auto& info : *display_info_list) {
        max_height = std::max(max_height, info.size_in_pixel().height());
        if (!default_height || Display::IsInternalDisplayId(info.id())) {
          default_height = info.size_in_pixel().height();
          default_device_scale_factor = info.device_scale_factor();
        }
      }

      ManagedDisplayInfo::ManagedDisplayModeList display_mode_list;
      std::set<std::pair<float, float>> dsf_scale_list;

      // 2nd Pass. Compute the unified display size.
      for (auto& info : *display_info_list) {
        InsertAndUpdateDisplayInfo(info);
        gfx::Point origin(unified_bounds.right(), 0);
        float scale =
            info.size_in_pixel().height() / static_cast<float>(max_height);
        // The display is scaled to fit the unified desktop size.
        Display display = CreateMirroringDisplayFromDisplayInfoById(
            info.id(), origin, 1.0f / scale);
        unified_bounds.Union(display.bounds());

        dsf_scale_list.insert(
            std::make_pair(info.device_scale_factor(), scale));
      }

      ManagedDisplayInfo info(kUnifiedDisplayId, "Unified Desktop", false);

      scoped_refptr<ManagedDisplayMode> native_mode(new ManagedDisplayMode(
          unified_bounds.size(), 60.0f, false, true, 1.0, 1.0));
      ManagedDisplayInfo::ManagedDisplayModeList modes =
          CreateUnifiedManagedDisplayModeList(native_mode, dsf_scale_list);

      // Find the default mode.
      auto iter = std::find_if(
          modes.begin(), modes.end(),
          [default_height, default_device_scale_factor](
              const scoped_refptr<ManagedDisplayMode>& mode) {
            return mode->size().height() == default_height &&
                   mode->device_scale_factor() == default_device_scale_factor;
          });

      scoped_refptr<ManagedDisplayMode> dm(*iter);
      *iter = make_scoped_refptr(new ManagedDisplayMode(
          dm->size(), dm->refresh_rate(), dm->is_interlaced(),
          true /* native */, dm->ui_scale(), dm->device_scale_factor()));

      info.SetManagedDisplayModes(modes);
      info.set_device_scale_factor(dm->device_scale_factor());
      info.SetBounds(gfx::Rect(dm->size()));

      // Forget the configured resolution if the original unified
      // desktop resolution has changed.
      if (display_info_.count(kUnifiedDisplayId) != 0 &&
          GetMaxNativeSize(display_info_[kUnifiedDisplayId]) !=
              unified_bounds.size()) {
        display_modes_.erase(kUnifiedDisplayId);
      }

      // 3rd Pass. Set the selected mode, then recompute the mirroring
      // display size.
      scoped_refptr<ManagedDisplayMode> mode =
          GetSelectedModeForDisplayId(kUnifiedDisplayId);
      if (mode && FindDisplayMode(info, mode) != info.display_modes().end()) {
        info.set_device_scale_factor(mode->device_scale_factor());
        info.SetBounds(gfx::Rect(mode->size()));
      } else {
        display_modes_.erase(kUnifiedDisplayId);
      }

      int unified_display_height = info.size_in_pixel().height();
      gfx::Point origin;
      for (auto& info : *display_info_list) {
        float display_scale = info.size_in_pixel().height() /
                              static_cast<float>(unified_display_height);
        Display display = CreateMirroringDisplayFromDisplayInfoById(
            info.id(), origin, 1.0f / display_scale);
        origin.Offset(display.size().width(), 0);
        display.UpdateWorkAreaFromInsets(gfx::Insets());
        software_mirroring_display_list_.push_back(display);
      }

      display_info_list->clear();
      display_info_list->push_back(info);
      InsertAndUpdateDisplayInfo(info);
      break;
    }
    case EXTENDED:
      break;
  }
}

Display* DisplayManager::FindDisplayForId(int64_t id) {
  auto iter =
      std::find_if(active_display_list_.begin(), active_display_list_.end(),
                   [id](const Display& display) { return display.id() == id; });
  if (iter != active_display_list_.end())
    return &(*iter);
  // TODO(oshima): This happens when windows in unified desktop have
  // been moved to a normal window. Fix this.
  if (id != kUnifiedDisplayId)
    DLOG(WARNING) << "Could not find display:" << id;
  return nullptr;
}

void DisplayManager::AddMirrorDisplayInfoIfAny(
    DisplayInfoList* display_info_list) {
  if (software_mirroring_enabled() && IsInMirrorMode()) {
    display_info_list->push_back(GetDisplayInfo(mirroring_display_id_));
    software_mirroring_display_list_.clear();
  }
}

void DisplayManager::InsertAndUpdateDisplayInfo(
    const ManagedDisplayInfo& new_info) {
  std::map<int64_t, ManagedDisplayInfo>::iterator info =
      display_info_.find(new_info.id());
  if (info != display_info_.end()) {
    info->second.Copy(new_info);
  } else {
    display_info_[new_info.id()] = new_info;
    display_info_[new_info.id()].set_native(false);
    // FHD with 1.25 DSF behaves differently from other configuration.
    // It uses 1.25 DSF only when UI-Scale is set to 0.8.
    // For new users, use the UI-scale to 0.8 so that it will use DSF=1.25
    // internally.
    if (Display::IsInternalDisplayId(new_info.id()) &&
        new_info.bounds_in_native().height() == 1080 &&
        new_info.device_scale_factor() == 1.25f) {
      display_info_[new_info.id()].set_configured_ui_scale(0.8f);
    }
  }
  display_info_[new_info.id()].UpdateDisplaySize();
  OnDisplayInfoUpdated(display_info_[new_info.id()]);
}

void DisplayManager::OnDisplayInfoUpdated(
    const ManagedDisplayInfo& display_info) {
#if defined(OS_CHROMEOS)
  ColorCalibrationProfile color_profile = display_info.color_profile();
  if (color_profile != COLOR_PROFILE_STANDARD) {
    delegate_->display_configurator()->SetColorCalibrationProfile(
        display_info.id(), color_profile);
  }
#endif
}

Display DisplayManager::CreateDisplayFromDisplayInfoById(int64_t id) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const ManagedDisplayInfo& display_info = display_info_[id];

  Display new_display(display_info.id());
  gfx::Rect bounds_in_native(display_info.size_in_pixel());
  float device_scale_factor = display_info.GetEffectiveDeviceScaleFactor();

  // Simply set the origin to (0,0).  The primary display's origin is
  // always (0,0) and the bounds of non-primary display(s) will be updated
  // in |UpdateNonPrimaryDisplayBoundsForLayout| called in |UpdateDisplay|.
  new_display.SetScaleAndBounds(device_scale_factor,
                                gfx::Rect(bounds_in_native.size()));
  new_display.set_rotation(display_info.GetActiveRotation());
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
  return new_display;
}

Display DisplayManager::CreateMirroringDisplayFromDisplayInfoById(
    int64_t id,
    const gfx::Point& origin,
    float scale) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const ManagedDisplayInfo& display_info = display_info_[id];

  Display new_display(display_info.id());
  new_display.SetScaleAndBounds(
      1.0f, gfx::Rect(origin, gfx::ScaleToFlooredSize(
                                  display_info.size_in_pixel(), scale)));
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
  return new_display;
}

void DisplayManager::UpdateNonPrimaryDisplayBoundsForLayout(
    Displays* display_list,
    std::vector<size_t>* updated_indices) {
  if (display_list->size() == 1u)
    return;

  const DisplayLayout& layout = layout_store_->GetRegisteredDisplayLayout(
      CreateDisplayIdList(*display_list));

  // Ignore if a user has a old format (should be extremely rare)
  // and this will be replaced with DCHECK.
  if (layout.primary_id == kInvalidDisplayId)
    return;

  // display_list does not have translation set, so ApplyDisplayLayout cannot
  // provide accurate change information. We'll find the changes after the call.
  current_resolved_layout_ = layout.Copy();
  ApplyDisplayLayout(current_resolved_layout_.get(), display_list, nullptr);
  size_t num_displays = display_list->size();
  for (size_t index = 0; index < num_displays; ++index) {
    const Display& display = (*display_list)[index];
    int64_t id = display.id();
    const Display* active_display = FindDisplayForId(id);
    if (!active_display || (active_display->bounds() != display.bounds()))
      updated_indices->push_back(index);
  }
}

void DisplayManager::CreateMirrorWindowIfAny() {
  if (software_mirroring_display_list_.empty() || !delegate_)
    return;
  DisplayInfoList list;
  for (auto& display : software_mirroring_display_list_)
    list.push_back(GetDisplayInfo(display.id()));
  delegate_->CreateOrUpdateMirroringDisplay(list);
}

void DisplayManager::ApplyDisplayLayout(DisplayLayout* layout,
                                        Displays* display_list,
                                        std::vector<int64_t>* updated_ids) {
  if (multi_display_mode_ == UNIFIED) {
    // Applying the layout in unified mode doesn't make sense, since there's no
    // layout.
    return;
  }

  layout->ApplyToDisplayList(display_list, updated_ids,
                             kMinimumOverlapForInvalidOffset);
}

void DisplayManager::RunPendingTasksForTest() {
  if (!software_mirroring_display_list_.empty())
    base::RunLoop().RunUntilIdle();
}

void DisplayManager::NotifyMetricsChanged(const Display& display,
                                          uint32_t metrics) {
  for (auto& observer : observers_)
    observer.OnDisplayMetricsChanged(display, metrics);
}

void DisplayManager::NotifyDisplayAdded(const Display& display) {
  for (auto& observer : observers_)
    observer.OnDisplayAdded(display);
}

void DisplayManager::NotifyDisplayRemoved(const Display& display) {
  for (auto& observer : observers_)
    observer.OnDisplayRemoved(display);
}

void DisplayManager::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DisplayManager::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

const Display& DisplayManager::GetSecondaryDisplay() const {
  CHECK_LE(2U, GetNumDisplays());
  return GetDisplayAt(0).id() == Screen::GetScreen()->GetPrimaryDisplay().id()
             ? GetDisplayAt(1)
             : GetDisplayAt(0);
}

}  // namespace display
