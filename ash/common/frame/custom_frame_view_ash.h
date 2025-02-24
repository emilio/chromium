// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_FRAME_CUSTOM_FRAME_VIEW_ASH_H_
#define ASH_COMMON_FRAME_CUSTOM_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/window_style.mojom.h"
#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class Widget;
}

namespace ash {

class FrameCaptionButtonContainerView;
class HeaderView;
class ImmersiveFullscreenController;
class ImmersiveFullscreenControllerDelegate;

// A NonClientFrameView used for packaged apps, dialogs and other non-browser
// windows. It supports immersive fullscreen. When in immersive fullscreen, the
// client view takes up the entire widget and the window header is an overlay.
// The window header overlay slides onscreen when the user hovers the mouse at
// the top of the screen. See also views::CustomFrameView and
// BrowserNonClientFrameViewAsh.
class ASH_EXPORT CustomFrameViewAsh : public views::NonClientFrameView {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // |enable_immersive| controls whether ImmersiveFullscreenController is
  // created for the CustomFrameViewAsh; if true and a WindowStateDelegate has
  // not been set on the WindowState associated with |frame|, then an
  // ImmersiveFullscreenController is created.
  // If ImmersiveFullscreenControllerDelegate is not supplied, HeaderView is
  // used as the ImmersiveFullscreenControllerDelegate.
  explicit CustomFrameViewAsh(
      views::Widget* frame,
      ImmersiveFullscreenControllerDelegate* immersive_delegate = nullptr,
      bool enable_immersive = true,
      mojom::WindowStyle window_style = mojom::WindowStyle::DEFAULT);
  ~CustomFrameViewAsh() override;

  // Inits |immersive_fullscreen_controller| so that the controller reveals
  // and hides |header_view_| in immersive fullscreen.
  // CustomFrameViewAsh does not take ownership of
  // |immersive_fullscreen_controller|.
  void InitImmersiveFullscreenControllerForView(
      ImmersiveFullscreenController* immersive_fullscreen_controller);

  // Sets the active and inactive frame colors. Note the inactive frame color
  // will have some transparency added when the frame is drawn.
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);

  // Sets the height of the header. If |height| has no value (the default), the
  // preferred height is used.
  void SetHeaderHeight(base::Optional<int> height);

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  void ActivationChanged(bool active) override;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void SchedulePaintInRect(const gfx::Rect& r) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // Get the view of the header.
  views::View* GetHeaderView();

  const views::View* GetAvatarIconViewForTest() const;

 private:
  class OverlayView;
  friend class TestWidgetConstraintsDelegate;

  // views::NonClientFrameView:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Returns the container for the minimize/maximize/close buttons that is held
  // by the HeaderView. Used in testing.
  FrameCaptionButtonContainerView* GetFrameCaptionButtonContainerViewForTest();

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  // Not owned.
  views::Widget* frame_;

  // View which contains the title and window controls.
  HeaderView* header_view_;

  OverlayView* overlay_view_;

  ImmersiveFullscreenControllerDelegate* immersive_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAsh);
};

}  // namespace ash

#endif  // ASH_COMMON_FRAME_CUSTOM_FRAME_VIEW_ASH_H_
