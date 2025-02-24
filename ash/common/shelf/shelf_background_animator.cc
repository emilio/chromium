// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_background_animator.h"

#include <algorithm>

#include "ash/animation/animation_change_type.h"
#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/color_utils.h"

namespace ash {

ShelfBackgroundAnimator::AnimationValues::AnimationValues() {}

ShelfBackgroundAnimator::AnimationValues::~AnimationValues() {}

void ShelfBackgroundAnimator::AnimationValues::UpdateCurrentValues(double t) {
  current_color_ =
      gfx::Tween::ColorValueBetween(t, initial_color_, target_color_);
}

void ShelfBackgroundAnimator::AnimationValues::SetTargetValues(
    SkColor target_color) {
  initial_color_ = current_color_;
  target_color_ = target_color;
}

bool ShelfBackgroundAnimator::AnimationValues::InitialValuesEqualTargetValuesOf(
    const AnimationValues& other) const {
  return initial_color_ == other.target_color_;
}

ShelfBackgroundAnimator::ShelfBackgroundAnimator(
    ShelfBackgroundType background_type,
    WmShelf* wm_shelf,
    WallpaperController* wallpaper_controller)
    : wm_shelf_(wm_shelf), wallpaper_controller_(wallpaper_controller) {
  if (wallpaper_controller_)
    wallpaper_controller_->AddObserver(this);
  if (wm_shelf_)
    wm_shelf_->AddObserver(this);

  // Initialize animators so that adding observers get notified with consistent
  // values.
  AnimateBackground(background_type, AnimationChangeType::IMMEDIATE);
}

ShelfBackgroundAnimator::~ShelfBackgroundAnimator() {
  if (wallpaper_controller_)
    wallpaper_controller_->RemoveObserver(this);
  if (wm_shelf_)
    wm_shelf_->RemoveObserver(this);
}

void ShelfBackgroundAnimator::AddObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observers_.AddObserver(observer);
  NotifyObserver(observer);
}

void ShelfBackgroundAnimator::RemoveObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ShelfBackgroundAnimator::NotifyObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observer->UpdateShelfBackground(shelf_background_values_.current_color());
  observer->UpdateShelfItemBackground(item_background_values_.current_color());
}

void ShelfBackgroundAnimator::PaintBackground(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  if (target_background_type_ == background_type &&
      change_type == AnimationChangeType::ANIMATE) {
    return;
  }

  AnimateBackground(background_type, change_type);
}

void ShelfBackgroundAnimator::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, animator_.get());
  SetAnimationValues(animation->GetCurrentValue());
}

void ShelfBackgroundAnimator::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, animator_.get());
  SetAnimationValues(animation->GetCurrentValue());
  animator_.reset();
}

void ShelfBackgroundAnimator::AnimationCanceled(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, animator_.get());
  SetAnimationValues(animator_->IsShowing() ? 1.0 : 0.0);
  // Animations are only cancelled when they are being pre-empted so we don't
  // destroy the |animator_| because it may be re-used immediately.
}

void ShelfBackgroundAnimator::OnWallpaperDataChanged() {}

void ShelfBackgroundAnimator::OnWallpaperColorsChanged() {
  AnimateBackground(target_background_type_, AnimationChangeType::ANIMATE);
}

void ShelfBackgroundAnimator::OnBackgroundTypeChanged(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  PaintBackground(background_type, change_type);
}

void ShelfBackgroundAnimator::NotifyObservers() {
  for (auto& observer : observers_)
    NotifyObserver(&observer);
}

void ShelfBackgroundAnimator::AnimateBackground(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  StopAnimator();

  if (change_type == AnimationChangeType::IMMEDIATE) {
    animator_.reset();
    SetTargetValues(background_type);
    SetAnimationValues(1.0);
  } else if (CanReuseAnimator(background_type)) {
    // |animator_| should not be null here as CanReuseAnimator() returns false
    // when it is null.
    if (animator_->IsShowing())
      animator_->Hide();
    else
      animator_->Show();
  } else {
    CreateAnimator(background_type);
    SetTargetValues(background_type);
    animator_->Show();
  }

  if (target_background_type_ != background_type) {
    previous_background_type_ = target_background_type_;
    target_background_type_ = background_type;
  }
}

bool ShelfBackgroundAnimator::CanReuseAnimator(
    ShelfBackgroundType background_type) const {
  if (!animator_)
    return false;

  AnimationValues target_shelf_background_values;
  AnimationValues target_item_background_values;
  GetTargetValues(background_type, &target_shelf_background_values,
                  &target_item_background_values);

  return previous_background_type_ == background_type &&
         shelf_background_values_.InitialValuesEqualTargetValuesOf(
             target_shelf_background_values) &&
         item_background_values_.InitialValuesEqualTargetValuesOf(
             target_item_background_values);
}

void ShelfBackgroundAnimator::CreateAnimator(
    ShelfBackgroundType background_type) {
  int duration_ms = 0;

  switch (background_type) {
    case SHELF_BACKGROUND_DEFAULT:
      duration_ms = 500;
      break;
    case SHELF_BACKGROUND_OVERLAP:
      duration_ms = 500;
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
      duration_ms = 250;
      break;
  }

  animator_.reset(new gfx::SlideAnimation(this));
  animator_->SetSlideDuration(duration_ms);
}

void ShelfBackgroundAnimator::StopAnimator() {
  if (animator_)
    animator_->Stop();
}

void ShelfBackgroundAnimator::SetTargetValues(
    ShelfBackgroundType background_type) {
  GetTargetValues(background_type, &shelf_background_values_,
                  &item_background_values_);
}

void ShelfBackgroundAnimator::GetTargetValues(
    ShelfBackgroundType background_type,
    AnimationValues* shelf_background_values,
    AnimationValues* item_background_values) const {
  int target_shelf_color_alpha = 0;
  int target_item_color_alpha = 0;

  switch (background_type) {
    case SHELF_BACKGROUND_DEFAULT:
      target_shelf_color_alpha = 0;
      target_item_color_alpha = kShelfTranslucentAlpha;
      break;
    case SHELF_BACKGROUND_OVERLAP:
      target_shelf_color_alpha = kShelfTranslucentAlpha;
      target_item_color_alpha = 0;
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
      target_shelf_color_alpha = kMaxAlpha;
      target_item_color_alpha = 0;
      break;
  }

  SkColor target_color = wallpaper_controller_
                             ? wallpaper_controller_->prominent_color()
                             : kShelfDefaultBaseColor;
  if (target_color == SK_ColorTRANSPARENT) {
    target_color = kShelfDefaultBaseColor;
  } else {
    int darkening_alpha = 0;

    switch (background_type) {
      case SHELF_BACKGROUND_DEFAULT:
      case SHELF_BACKGROUND_OVERLAP:
        darkening_alpha = kShelfTranslucentColorDarkenAlpha;
        break;
      case SHELF_BACKGROUND_MAXIMIZED:
        darkening_alpha = kShelfOpaqueColorDarkenAlpha;
        break;
    }
    target_color = color_utils::GetResultingPaintColor(
        SkColorSetA(kShelfDefaultBaseColor, darkening_alpha), target_color);
  }

  shelf_background_values->SetTargetValues(
      SkColorSetA(target_color, target_shelf_color_alpha));
  item_background_values->SetTargetValues(
      SkColorSetA(target_color, target_item_color_alpha));
}

void ShelfBackgroundAnimator::SetAnimationValues(double t) {
  DCHECK_GE(t, 0.0);
  DCHECK_LE(t, 1.0);
  shelf_background_values_.UpdateCurrentValues(t);
  item_background_values_.UpdateCurrentValues(t);
  NotifyObservers();
}

}  // namespace ash
