// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_MANAGER_DELEGATE_H_
#define UI_AURA_MUS_WINDOW_MANAGER_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "services/ui/public/interfaces/cursor.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/events/mojo/event.mojom.h"

namespace display {
class Display;
}

namespace gfx {
class Insets;
class Rect;
}

namespace ui {
class Event;
}

namespace aura {

class Window;
class WindowTreeHostMus;

// See the mojom with the same name for details on the functions in this
// interface.
class AURA_EXPORT WindowManagerClient {
 public:
  virtual void SetFrameDecorationValues(
      ui::mojom::FrameDecorationValuesPtr values) = 0;
  virtual void SetNonClientCursor(Window* window,
                                  ui::mojom::Cursor non_client_cursor) = 0;

  virtual void AddAccelerators(
      std::vector<ui::mojom::AcceleratorPtr> accelerators,
      const base::Callback<void(bool)>& callback) = 0;
  virtual void RemoveAccelerator(uint32_t id) = 0;
  virtual void AddActivationParent(Window* window) = 0;
  virtual void RemoveActivationParent(Window* window) = 0;
  virtual void ActivateNextWindow() = 0;
  virtual void SetExtendedHitArea(Window* window,
                                  const gfx::Insets& hit_area) = 0;

  // Requests the client embedded in |window| to close the window. Only
  // applicable to top-level windows. If a client is not embedded in |window|,
  // this does nothing.
  virtual void RequestClose(Window* window) = 0;

 protected:
  virtual ~WindowManagerClient() {}
};

// Used by clients implementing a window manager.
// TODO(sky): this should be called WindowManager, but that's rather confusing
// currently.
class AURA_EXPORT WindowManagerDelegate {
 public:
  // Called once to give the delegate access to functions only exposed to
  // the WindowManager.
  virtual void SetWindowManagerClient(WindowManagerClient* client) = 0;

  // A client requested the bounds of |window| to change to |bounds|. Return
  // true if the bounds are allowed to change. A return value of false
  // indicates the change is not allowed.
  // NOTE: This should not change the bounds of |window|. Instead return the
  // bounds the window should be in |bounds|.
  virtual bool OnWmSetBounds(Window* window, gfx::Rect* bounds) = 0;

  // A client requested the shared property named |name| to change to
  // |new_data|. Return true to allow the change to |new_data|, false
  // otherwise. If true is returned the property is set via
  // PropertyConverter::SetPropertyFromTransportValue().
  virtual bool OnWmSetProperty(
      Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) = 0;

  // A client requested to change focusibility of |window|. We currently assume
  // this always succeeds.
  virtual void OnWmSetCanFocus(Window* window, bool can_focus) = 0;

  // A client has requested a new top level window. The delegate should create
  // and parent the window appropriately and return it. |properties| is the
  // supplied properties from the client requesting the new window. The
  // delegate may modify |properties| before calling NewWindow(), but the
  // delegate does *not* own |properties|, they are valid only for the life
  // of OnWmCreateTopLevelWindow(). |window_type| is the type of window
  // requested by the client. Use SetWindowType() with |window_type| (in
  // property_utils.h) to configure the type on the newly created window.
  virtual Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) = 0;

  // Called when a Mus client's jankiness changes. |windows| is the set of
  // windows owned by the window manager in which the client is embedded.
  virtual void OnWmClientJankinessChanged(
      const std::set<Window*>& client_windows,
      bool janky) = 0;

  // When a new display is added OnWmWillCreateDisplay() is called, and then
  // OnWmNewDisplay(). OnWmWillCreateDisplay() is intended to add the display
  // to the set of displays (see Screen).
  virtual void OnWmWillCreateDisplay(const display::Display& display) = 0;

  // Called when a WindowTreeHostMus is created for a new display
  // Called when a display is added. |window_tree_host| is the WindowTreeHost
  // for the new display.
  virtual void OnWmNewDisplay(
      std::unique_ptr<WindowTreeHostMus> window_tree_host,
      const display::Display& display) = 0;

  // Called when a display is removed. |window_tree_host| is the WindowTreeHost
  // for the display.
  virtual void OnWmDisplayRemoved(WindowTreeHostMus* window_tree_host) = 0;

  // Called when a display is modified.
  virtual void OnWmDisplayModified(const display::Display& display) = 0;

  virtual ui::mojom::EventResult OnAccelerator(uint32_t id,
                                               const ui::Event& event);

  virtual void OnWmPerformMoveLoop(
      Window* window,
      ui::mojom::MoveLoopSource source,
      const gfx::Point& cursor_location,
      const base::Callback<void(bool)>& on_done) = 0;

  virtual void OnWmCancelMoveLoop(Window* window) = 0;

  // Called when then client changes the client area of a window.
  virtual void OnWmSetClientArea(
      Window* window,
      const gfx::Insets& insets,
      const std::vector<gfx::Rect>& additional_client_areas) = 0;

  // Returns whether |window| is the current active window.
  virtual bool IsWindowActive(Window* window) = 0;

  // Called when a client requests that its activation be given to another
  // window.
  virtual void OnWmDeactivateWindow(Window* window) = 0;

 protected:
  virtual ~WindowManagerDelegate() {}
};

}  // namespace ui

#endif  // UI_AURA_MUS_WINDOW_MANAGER_DELEGATE_H_
