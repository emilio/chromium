// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"

#include <utility>

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/cast_config_controller.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/media_controller.h"
#include "ash/common/new_window_controller.h"
#include "ash/common/session/session_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/app_list_shelf_item_delegate.h"
#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_window_watcher.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/brightness_control_delegate.h"
#include "ash/common/system/chromeos/brightness/brightness_controller_chromeos.h"
#include "ash/common/system/chromeos/keyboard_brightness_controller.h"
#include "ash/common/system/chromeos/network/vpn_list.h"
#include "ash/common/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/common/system/keyboard_brightness_control_delegate.h"
#include "ash/common/system/locale/locale_notification_controller.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/immersive_context_ash.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/window_cycle_controller.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "services/preferences/public/cpp/pref_client_store.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/display/display.h"

namespace ash {

// static
WmShell* WmShell::instance_ = nullptr;

WmShell::~WmShell() {
  session_controller_->RemoveSessionStateObserver(this);
}

// static
void WmShell::Set(WmShell* instance) {
  instance_ = instance;
}

// static
WmShell* WmShell::Get() {
  return instance_;
}

void WmShell::Shutdown() {
  // ShelfWindowWatcher has window observers and a pointer to the shelf model.
  shelf_window_watcher_.reset();
  // ShelfItemDelegate subclasses it owns have complex cleanup to run (e.g. ARC
  // shelf items in Chrome) so explicitly shutdown early.
  shelf_model()->DestroyItemDelegates();
  // Must be destroyed before FocusClient.
  shelf_delegate_.reset();

  // Removes itself as an observer of |pref_store_|.
  shelf_controller_.reset();
}

ShelfModel* WmShell::shelf_model() {
  return shelf_controller_->model();
}

void WmShell::ShowContextMenu(const gfx::Point& location_in_screen,
                              ui::MenuSourceType source_type) {
  // Bail if there is no active user session or if the screen is locked.
  if (GetSessionStateDelegate()->NumberOfLoggedInUsers() < 1 ||
      GetSessionStateDelegate()->IsScreenLocked()) {
    return;
  }

  WmWindow* root = wm::GetRootWindowAt(location_in_screen);
  root->GetRootWindowController()->ShowContextMenu(location_in_screen,
                                                   source_type);
}

void WmShell::CreateShelfView() {
  // Must occur after SessionStateDelegate creation and user login.
  DCHECK(GetSessionStateDelegate());
  DCHECK_GT(GetSessionStateDelegate()->NumberOfLoggedInUsers(), 0);
  CreateShelfDelegate();

  for (WmWindow* root_window : GetAllRootWindows())
    root_window->GetRootWindowController()->CreateShelfView();
}

void WmShell::CreateShelfDelegate() {
  // May be called multiple times as shelves are created and destroyed.
  if (shelf_delegate_)
    return;
  // Must occur after SessionStateDelegate creation and user login because
  // Chrome's implementation of ShelfDelegate assumes it can get information
  // about multi-profile login state.
  DCHECK(GetSessionStateDelegate());
  DCHECK_GT(GetSessionStateDelegate()->NumberOfLoggedInUsers(), 0);
  shelf_delegate_.reset(delegate_->CreateShelfDelegate(shelf_model()));
  shelf_window_watcher_.reset(new ShelfWindowWatcher(shelf_model()));
}

void WmShell::UpdateAfterLoginStatusChange(LoginStatus status) {
  for (WmWindow* root_window : GetAllRootWindows()) {
    root_window->GetRootWindowController()->UpdateAfterLoginStatusChange(
        status);
  }
}

void WmShell::OnLockStateEvent(LockStateObserver::EventType event) {
  for (auto& observer : lock_state_observers_)
    observer.OnLockStateEvent(event);
}

void WmShell::AddLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.AddObserver(observer);
}

void WmShell::RemoveLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.RemoveObserver(observer);
}

void WmShell::SetShelfDelegateForTesting(
    std::unique_ptr<ShelfDelegate> test_delegate) {
  shelf_delegate_ = std::move(test_delegate);
}

WmShell::WmShell(std::unique_ptr<ShellDelegate> shell_delegate)
    : delegate_(std::move(shell_delegate)),
      app_list_(base::MakeUnique<app_list::AppList>()),
      brightness_control_delegate_(
          base::MakeUnique<system::BrightnessControllerChromeos>()),
      cast_config_(base::MakeUnique<CastConfigController>()),
      focus_cycler_(base::MakeUnique<FocusCycler>()),
      immersive_context_(base::MakeUnique<ImmersiveContextAsh>()),
      keyboard_brightness_control_delegate_(
          base::MakeUnique<KeyboardBrightnessController>()),
      locale_notification_controller_(
          base::MakeUnique<LocaleNotificationController>()),
      media_controller_(base::MakeUnique<MediaController>()),
      new_window_controller_(base::MakeUnique<NewWindowController>()),
      session_controller_(base::MakeUnique<SessionController>()),
      shelf_controller_(base::MakeUnique<ShelfController>()),
      shutdown_controller_(base::MakeUnique<ShutdownController>()),
      system_tray_controller_(base::MakeUnique<SystemTrayController>()),
      system_tray_notifier_(base::MakeUnique<SystemTrayNotifier>()),
      vpn_list_(base::MakeUnique<VpnList>()),
      wallpaper_delegate_(delegate_->CreateWallpaperDelegate()),
      window_cycle_controller_(base::MakeUnique<WindowCycleController>()),
      window_selector_controller_(
          base::MakeUnique<WindowSelectorController>()) {
  session_controller_->AddSessionStateObserver(this);

  prefs::mojom::PreferencesServiceFactoryPtr pref_factory_ptr;
  // Can be null in tests.
  if (!delegate_->GetShellConnector())
    return;
  delegate_->GetShellConnector()->BindInterface(prefs::mojom::kServiceName,
                                                &pref_factory_ptr);
  pref_store_ = new preferences::PrefClientStore(std::move(pref_factory_ptr));
}

RootWindowController* WmShell::GetPrimaryRootWindowController() {
  return GetPrimaryRootWindow()->GetRootWindowController();
}

bool WmShell::IsForceMaximizeOnFirstRun() {
  return delegate()->IsForceMaximizeOnFirstRun();
}

bool WmShell::IsSystemModalWindowOpen() {
  if (simulate_modal_window_open_for_testing_)
    return true;

  // Traverse all system modal containers, and find its direct child window
  // with "SystemModal" setting, and visible.
  for (WmWindow* root : GetAllRootWindows()) {
    WmWindow* system_modal =
        root->GetChildByShellWindowId(kShellWindowId_SystemModalContainer);
    if (!system_modal)
      continue;
    for (const WmWindow* child : system_modal->GetChildren()) {
      if (child->IsSystemModal() && child->GetTargetVisibility()) {
        return true;
      }
    }
  }
  return false;
}

void WmShell::CreateModalBackground(WmWindow* window) {
  for (WmWindow* root_window : GetAllRootWindows()) {
    root_window->GetRootWindowController()
        ->GetSystemModalLayoutManager(window)
        ->CreateModalBackground();
  }
}

void WmShell::OnModalWindowRemoved(WmWindow* removed) {
  WmWindow::Windows root_windows = GetAllRootWindows();
  for (WmWindow* root_window : root_windows) {
    if (root_window->GetRootWindowController()
            ->GetSystemModalLayoutManager(removed)
            ->ActivateNextModalWindow()) {
      return;
    }
  }
  for (WmWindow* root_window : root_windows) {
    root_window->GetRootWindowController()
        ->GetSystemModalLayoutManager(removed)
        ->DestroyModalBackground();
  }
}

void WmShell::ShowAppList() {
  // Show the app list on the default display for new windows.
  app_list_->Show(
      Shell::GetWmRootWindowForNewWindows()->GetDisplayNearestWindow().id());
}

void WmShell::DismissAppList() {
  app_list_->Dismiss();
}

void WmShell::ToggleAppList() {
  // Toggle the app list on the default display for new windows.
  app_list_->ToggleAppList(
      Shell::GetWmRootWindowForNewWindows()->GetDisplayNearestWindow().id());
}

bool WmShell::IsApplistVisible() const {
  return app_list_->IsVisible();
}

bool WmShell::GetAppListTargetVisibility() const {
  return app_list_->GetTargetVisibility();
}

void WmShell::SetKeyboardUI(std::unique_ptr<KeyboardUI> keyboard_ui) {
  keyboard_ui_ = std::move(keyboard_ui);
}

void WmShell::SetSystemTrayDelegate(
    std::unique_ptr<SystemTrayDelegate> delegate) {
  DCHECK(delegate);
  system_tray_delegate_ = std::move(delegate);
  system_tray_delegate_->Initialize();
  // Accesses WmShell in its constructor.
  logout_confirmation_controller_.reset(new LogoutConfirmationController(
      base::Bind(&SystemTrayController::SignOut,
                 base::Unretained(system_tray_controller_.get()))));
}

void WmShell::DeleteSystemTrayDelegate() {
  DCHECK(system_tray_delegate_);
  // Accesses WmShell in its destructor.
  logout_confirmation_controller_.reset();
  system_tray_delegate_.reset();
}

void WmShell::DeleteWindowCycleController() {
  window_cycle_controller_.reset();
}

void WmShell::DeleteWindowSelectorController() {
  window_selector_controller_.reset();
}

void WmShell::CreateMaximizeModeController() {
  maximize_mode_controller_.reset(new MaximizeModeController);
}

void WmShell::DeleteMaximizeModeController() {
  maximize_mode_controller_.reset();
}

void WmShell::CreateMruWindowTracker() {
  mru_window_tracker_.reset(new MruWindowTracker);
}

void WmShell::DeleteMruWindowTracker() {
  mru_window_tracker_.reset();
}

void WmShell::SetAcceleratorController(
    std::unique_ptr<AcceleratorController> accelerator_controller) {
  accelerator_controller_ = std::move(accelerator_controller);
}

void WmShell::SessionStateChanged(session_manager::SessionState state) {
  // Create the shelf when a session becomes active. It's safe to do this
  // multiple times (e.g. initial login vs. multiprofile add session).
  if (state == session_manager::SessionState::ACTIVE)
    CreateShelfView();
}

}  // namespace ash
