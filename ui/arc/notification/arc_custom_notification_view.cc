// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_custom_notification_view.h"

#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/transform.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/custom_notification_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/message_center/views/toast_contents_view.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace arc {

class ArcCustomNotificationView::EventForwarder : public ui::EventHandler {
 public:
  explicit EventForwarder(ArcCustomNotificationView* owner) : owner_(owner) {}
  ~EventForwarder() override = default;

 private:
  // ui::EventHandler
  void OnEvent(ui::Event* event) override {
    // Do not forward event targeted to the floating close button so that
    // keyboard press and tap are handled properly.
    if (owner_->floating_control_buttons_widget_ && event->target() &&
        owner_->floating_control_buttons_widget_->GetNativeWindow() ==
            event->target()) {
      return;
    }

    if (event->IsScrollEvent()) {
      ForwardScrollEvent(event->AsScrollEvent());
    } else if (event->IsMouseWheelEvent()) {
      ForwardMouseWheelEvent(event->AsMouseWheelEvent());
    } else if (!event->IsTouchEvent()) {
      // TODO(yoshiki): Use a better tigger (eg. focusing EditText on
      // notification) than clicking (crbug.com/697379).
      if (event->type() == ui::ET_MOUSE_PRESSED)
        owner_->ActivateToast();

      // Forward the rest events to |owner_| except touches because View
      // should no longer receive touch events. See View::OnTouchEvent.
      owner_->OnEvent(event);
    }
  }

  void ForwardScrollEvent(ui::ScrollEvent* event) {
    views::Widget* widget = owner_->GetWidget();
    if (!widget)
      return;

    event->target()->ConvertEventToTarget(widget->GetNativeWindow(), event);
    widget->OnScrollEvent(event);
  }

  void ForwardMouseWheelEvent(ui::MouseWheelEvent* event) {
    views::Widget* widget = owner_->GetWidget();
    if (!widget)
      return;

    event->target()->ConvertEventToTarget(widget->GetNativeWindow(), event);
    widget->OnMouseEvent(event);
  }

  ArcCustomNotificationView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(EventForwarder);
};

class ArcCustomNotificationView::SlideHelper
    : public ui::LayerAnimationObserver {
 public:
  explicit SlideHelper(ArcCustomNotificationView* owner) : owner_(owner) {
    owner_->parent()->layer()->GetAnimator()->AddObserver(this);

    // Reset opacity to 1 to handle to case when the surface is sliding before
    // getting managed by this class, e.g. sliding in a popup before showing
    // in a message center view.
    if (owner_->surface_ && owner_->surface_->window())
      owner_->surface_->window()->layer()->SetOpacity(1.0f);
  }
  ~SlideHelper() override {
    owner_->parent()->layer()->GetAnimator()->RemoveObserver(this);
  }

  void Update() {
    const bool has_animation =
        owner_->parent()->layer()->GetAnimator()->is_animating();
    const bool has_transform = !owner_->parent()->GetTransform().IsIdentity();
    const bool sliding = has_transform || has_animation;
    if (sliding_ == sliding)
      return;

    sliding_ = sliding;

    if (sliding_)
      OnSlideStart();
    else
      OnSlideEnd();
  }

 private:
  void OnSlideStart() {
    if (!owner_->surface_ || !owner_->surface_->window())
      return;
    surface_copy_ = ::wm::RecreateLayers(owner_->surface_->window());
    // |surface_copy_| is at (0, 0) in owner_->layer().
    surface_copy_->root()->SetBounds(gfx::Rect(surface_copy_->root()->size()));
    owner_->layer()->Add(surface_copy_->root());
    owner_->surface_->window()->layer()->SetOpacity(0.0f);
  }

  void OnSlideEnd() {
    if (!owner_->surface_ || !owner_->surface_->window())
      return;
    owner_->surface_->window()->layer()->SetOpacity(1.0f);
    owner_->Layout();
    surface_copy_.reset();
  }

  // ui::LayerAnimationObserver
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* seq) override {
    Update();
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* seq) override {
    Update();
  }
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* seq) override {}

  ArcCustomNotificationView* const owner_;
  bool sliding_ = false;
  std::unique_ptr<ui::LayerTreeOwner> surface_copy_;

  DISALLOW_COPY_AND_ASSIGN(SlideHelper);
};

class ArcCustomNotificationView::ControlButton
    : public message_center::PaddedButton {
 public:
  explicit ControlButton(ArcCustomNotificationView* owner)
      : message_center::PaddedButton(owner), owner_(owner) {}

  void OnFocus() override {
    message_center::PaddedButton::OnFocus();
    owner_->UpdateControlButtonsVisibility();
  }

  void OnBlur() override {
    message_center::PaddedButton::OnBlur();
    owner_->UpdateControlButtonsVisibility();
  }

  void HideInkDrop() { AnimateInkDrop(views::InkDropState::HIDDEN, nullptr); }

 private:
  ArcCustomNotificationView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(ControlButton);
};

class ArcCustomNotificationView::ContentViewDelegate
    : public message_center::CustomNotificationContentViewDelegate {
 public:
  explicit ContentViewDelegate(ArcCustomNotificationView* owner)
      : owner_(owner) {}

  bool IsCloseButtonFocused() const override {
    if (owner_->close_button_ == nullptr)
      return false;
    return owner_->close_button_->HasFocus();
  }

  void RequestFocusOnCloseButton() override {
    if (owner_->close_button_)
      owner_->close_button_->RequestFocus();
    owner_->UpdateControlButtonsVisibility();
  }

  bool IsPinned() const override {
    return owner_->item_->pinned();
  }

  void UpdateControlButtonsVisibility() override {
    owner_->UpdateControlButtonsVisibility();
  }

 private:
  ArcCustomNotificationView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewDelegate);
};

ArcCustomNotificationView::ArcCustomNotificationView(
    ArcCustomNotificationItem* item)
    : item_(item),
      notification_key_(item->notification_key()),
      event_forwarder_(new EventForwarder(this)) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  item_->IncrementWindowRefCount();
  item_->AddObserver(this);

  ArcNotificationSurfaceManager::Get()->AddObserver(this);
  exo::NotificationSurface* surface =
      ArcNotificationSurfaceManager::Get()->GetSurface(notification_key_);
  if (surface)
    OnNotificationSurfaceAdded(surface);

  // Create a layer as an anchor to insert surface copy during a slide.
  SetPaintToLayer();
  UpdatePreferredSize();
}

ArcCustomNotificationView::~ArcCustomNotificationView() {
  SetSurface(nullptr);
  if (item_) {
    item_->DecrementWindowRefCount();
    item_->RemoveObserver(this);
  }

  if (ArcNotificationSurfaceManager::Get())
    ArcNotificationSurfaceManager::Get()->RemoveObserver(this);
}

std::unique_ptr<message_center::CustomNotificationContentViewDelegate>
ArcCustomNotificationView::CreateContentViewDelegate() {
  return base::MakeUnique<ArcCustomNotificationView::ContentViewDelegate>(this);
}

void ArcCustomNotificationView::CreateFloatingControlButtons() {
  // Floating close button is a transient child of |surface_| and also part
  // of the hosting widget's focus chain. It could only be created when both
  // are present.
  if (!surface_ || !GetWidget())
    return;

  // Creates the control_buttons_view_, which collects all control buttons into
  // a horizontal box.
  control_buttons_view_ = new views::View();
  control_buttons_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  settings_button_ = new ControlButton(this);
  settings_button_->SetImage(views::CustomButton::STATE_NORMAL,
                             message_center::GetSettingsIcon());
  settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  control_buttons_view_->AddChildView(settings_button_);

  close_button_ = new ControlButton(this);
  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          message_center::GetCloseIcon());
  close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
  control_buttons_view_->AddChildView(close_button_);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = surface_->window();

  floating_control_buttons_widget_.reset(new views::Widget);
  floating_control_buttons_widget_->Init(params);
  floating_control_buttons_widget_->SetContentsView(control_buttons_view_);

  // Put the close button into the focus chain.
  floating_control_buttons_widget_->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());
  floating_control_buttons_widget_->SetFocusTraversableParentView(this);

  Layout();
}

void ArcCustomNotificationView::SetSurface(exo::NotificationSurface* surface) {
  if (surface_ == surface)
    return;

  // Reset |floating_control_buttons_widget_| when |surface_| is changed.
  floating_control_buttons_widget_.reset();

  if (surface_ && surface_->window()) {
    surface_->window()->RemoveObserver(this);
    surface_->window()->RemovePreTargetHandler(event_forwarder_.get());
  }

  surface_ = surface;

  if (surface_ && surface_->window()) {
    surface_->window()->AddObserver(this);
    surface_->window()->AddPreTargetHandler(event_forwarder_.get());

    if (GetWidget())
      AttachSurface();
  }
}

void ArcCustomNotificationView::UpdatePreferredSize() {
  gfx::Size preferred_size =
      surface_ ? surface_->GetSize() : item_ ? item_->snapshot().size()
                                             : gfx::Size();
  if (preferred_size.IsEmpty())
    return;

  if (preferred_size.width() != message_center::kNotificationWidth) {
    const float scale = static_cast<float>(message_center::kNotificationWidth) /
                        preferred_size.width();
    preferred_size.SetSize(message_center::kNotificationWidth,
                           preferred_size.height() * scale);
  }

  SetPreferredSize(preferred_size);
}

void ArcCustomNotificationView::UpdateControlButtonsVisibility() {
  if (!surface_ || !floating_control_buttons_widget_)
    return;

  const bool target_visiblity =
      surface_->window()->GetBoundsInScreen().Contains(
          display::Screen::GetScreen()->GetCursorScreenPoint()) ||
      close_button_->HasFocus() || settings_button_->HasFocus();
  if (target_visiblity == floating_control_buttons_widget_->IsVisible())
    return;

  if (target_visiblity)
    floating_control_buttons_widget_->Show();
  else
    floating_control_buttons_widget_->Hide();
}

void ArcCustomNotificationView::UpdatePinnedState() {
  DCHECK(item_);

  if (item_->pinned() && floating_control_buttons_widget_) {
    floating_control_buttons_widget_.reset();
  } else if (!item_->pinned() && !floating_control_buttons_widget_) {
    CreateFloatingControlButtons();
  }
}

void ArcCustomNotificationView::UpdateSnapshot() {
  // Bail if we have a |surface_| because it controls the sizes and paints UI.
  if (surface_)
    return;

  UpdatePreferredSize();
  SchedulePaint();
}

void ArcCustomNotificationView::AttachSurface() {
  if (!GetWidget())
    return;

  UpdatePreferredSize();
  Attach(surface_->window());

  // The texture for this window can be placed at subpixel position
  // with fractional scale factor. Force to align it at the pixel
  // boundary here, and when layout is updated in Layout().
  ash::wm::SnapWindowToPixelBoundary(surface_->window());

  // Creates slide helper after this view is added to its parent.
  slide_helper_.reset(new SlideHelper(this));

  // Invokes Update() in case surface is attached during a slide.
  slide_helper_->Update();

  // Updates pinned state to create or destroy the floating close button
  // after |surface_| is attached to a widget.
  if (item_)
    UpdatePinnedState();
}

void ArcCustomNotificationView::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  views::Widget* widget = GetWidget();

  if (!details.is_add) {
    // Resets slide helper when this view is removed from its parent.
    slide_helper_.reset();

    // Bail if this view is no longer attached to a widget or native_view() has
    // attached to a different widget.
    if (!widget || (native_view() &&
                    views::Widget::GetTopLevelWidgetForNativeView(
                        native_view()) != widget)) {
      return;
    }
  }

  views::NativeViewHost::ViewHierarchyChanged(details);

  if (!widget || !surface_ || !details.is_add)
    return;

  AttachSurface();
}

void ArcCustomNotificationView::Layout() {
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  views::NativeViewHost::Layout();

  if (!surface_ || !GetWidget())
    return;

  const gfx::Rect contents_bounds = GetContentsBounds();

  // Scale notification surface if necessary.
  gfx::Transform transform;
  const gfx::Size surface_size = surface_->GetSize();
  const gfx::Size contents_size = contents_bounds.size();
  if (!surface_size.IsEmpty() && !contents_size.IsEmpty()) {
    transform.Scale(
        static_cast<float>(contents_size.width()) / surface_size.width(),
        static_cast<float>(contents_size.height()) / surface_size.height());
  }

  // Apply the transform to the surface content so that close button can
  // be positioned without the need to consider the transform.
  surface_->window()->children()[0]->SetTransform(transform);

  if (!floating_control_buttons_widget_)
    return;

  gfx::Rect control_buttons_bounds(contents_bounds);
  const int buttons_width = close_button_->GetPreferredSize().width() +
                            settings_button_->GetPreferredSize().width();
  control_buttons_bounds.set_x(control_buttons_bounds.right() - buttons_width -
                               message_center::kControlButtonPadding);
  control_buttons_bounds.set_y(control_buttons_bounds.y() +
                               message_center::kControlButtonPadding);
  control_buttons_bounds.set_height(close_button_->GetPreferredSize().height());
  control_buttons_bounds.set_width(buttons_width);
  floating_control_buttons_widget_->SetBounds(control_buttons_bounds);

  UpdateControlButtonsVisibility();

  ash::wm::SnapWindowToPixelBoundary(surface_->window());
}

void ArcCustomNotificationView::OnPaint(gfx::Canvas* canvas) {
  views::NativeViewHost::OnPaint(canvas);

  // Bail if there is a |surface_| or no item or no snapshot image.
  if (surface_ || !item_ || item_->snapshot().isNull())
    return;
  const gfx::Rect contents_bounds = GetContentsBounds();
  canvas->DrawImageInt(item_->snapshot(), 0, 0, item_->snapshot().width(),
                       item_->snapshot().height(), contents_bounds.x(),
                       contents_bounds.y(), contents_bounds.width(),
                       contents_bounds.height(), false);
}

void ArcCustomNotificationView::OnKeyEvent(ui::KeyEvent* event) {
  // Forward to parent CustomNotificationView to handle keyboard dismissal.
  parent()->OnKeyEvent(event);
}

void ArcCustomNotificationView::OnGestureEvent(ui::GestureEvent* event) {
  // Forward to parent CustomNotificationView to handle sliding out.
  parent()->OnGestureEvent(event);

  // |slide_helper_| could be null before |surface_| is attached.
  if (slide_helper_)
    slide_helper_->Update();
}

void ArcCustomNotificationView::OnMouseEntered(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
}

void ArcCustomNotificationView::OnMouseExited(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
}

void ArcCustomNotificationView::OnFocus() {
  CHECK_EQ(message_center::CustomNotificationView::kViewClassName,
           parent()->GetClassName());

  NativeViewHost::OnFocus();
  static_cast<message_center::CustomNotificationView*>(parent())
      ->OnContentFocused();
}

void ArcCustomNotificationView::OnBlur() {
  CHECK_EQ(message_center::CustomNotificationView::kViewClassName,
           parent()->GetClassName());

  NativeViewHost::OnBlur();
  static_cast<message_center::CustomNotificationView*>(parent())
      ->OnContentBlured();
}

void ArcCustomNotificationView::ActivateToast() {
  if (message_center::ToastContentsView::kViewClassName ==
      parent()->parent()->GetClassName()) {
    static_cast<message_center::ToastContentsView*>(parent()->parent())
        ->ActivateToast();
  }
}

views::FocusTraversable* ArcCustomNotificationView::GetFocusTraversable() {
  if (floating_control_buttons_widget_)
    return static_cast<views::internal::RootView*>(
        floating_control_buttons_widget_->GetRootView());
  return nullptr;
}

void ArcCustomNotificationView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (item_ && !item_->pinned() && sender == close_button_) {
    CHECK_EQ(message_center::CustomNotificationView::kViewClassName,
             parent()->GetClassName());
    static_cast<message_center::CustomNotificationView*>(parent())
        ->OnCloseButtonPressed();
  }
  if (item_ && settings_button_ && sender == settings_button_) {
    settings_button_->HideInkDrop();
    item_->OpenSettings();
  }
}

void ArcCustomNotificationView::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (in_layout_)
    return;

  UpdatePreferredSize();
  Layout();
}

void ArcCustomNotificationView::OnWindowDestroying(aura::Window* window) {
  SetSurface(nullptr);
}

void ArcCustomNotificationView::OnItemDestroying() {
  item_->RemoveObserver(this);
  item_ = nullptr;

  // Reset |surface_| with |item_| since no one is observing the |surface_|
  // after |item_| is gone and this view should be removed soon.
  SetSurface(nullptr);
}

void ArcCustomNotificationView::OnItemUpdated() {
  UpdatePinnedState();
  UpdateSnapshot();
}

void ArcCustomNotificationView::OnNotificationSurfaceAdded(
    exo::NotificationSurface* surface) {
  if (surface->notification_id() != notification_key_)
    return;

  SetSurface(surface);
}

void ArcCustomNotificationView::OnNotificationSurfaceRemoved(
    exo::NotificationSurface* surface) {
  if (surface->notification_id() != notification_key_)
    return;

  SetSurface(nullptr);
}

}  // namespace arc
