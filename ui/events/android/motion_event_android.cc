// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android.h"

#include <android/input.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "jni/MotionEvent_jni.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using namespace JNI_MotionEvent;

namespace ui {
namespace {

MotionEventAndroid::Action FromAndroidAction(int android_action) {
  switch (android_action) {
    case ACTION_DOWN:
      return MotionEventAndroid::ACTION_DOWN;
    case ACTION_UP:
      return MotionEventAndroid::ACTION_UP;
    case ACTION_MOVE:
      return MotionEventAndroid::ACTION_MOVE;
    case ACTION_CANCEL:
      return MotionEventAndroid::ACTION_CANCEL;
    case ACTION_POINTER_DOWN:
      return MotionEventAndroid::ACTION_POINTER_DOWN;
    case ACTION_POINTER_UP:
      return MotionEventAndroid::ACTION_POINTER_UP;
    case ACTION_HOVER_ENTER:
      return MotionEventAndroid::ACTION_HOVER_ENTER;
    case ACTION_HOVER_EXIT:
      return MotionEventAndroid::ACTION_HOVER_EXIT;
    case ACTION_HOVER_MOVE:
      return MotionEventAndroid::ACTION_HOVER_MOVE;
    case ACTION_BUTTON_PRESS:
      return MotionEventAndroid::ACTION_BUTTON_PRESS;
    case ACTION_BUTTON_RELEASE:
      return MotionEventAndroid::ACTION_BUTTON_RELEASE;
    default:
      NOTREACHED() << "Invalid Android MotionEvent action: " << android_action;
  };
  return MotionEventAndroid::ACTION_CANCEL;
}

MotionEventAndroid::ToolType FromAndroidToolType(int android_tool_type) {
  switch (android_tool_type) {
    case TOOL_TYPE_UNKNOWN:
      return MotionEventAndroid::TOOL_TYPE_UNKNOWN;
    case TOOL_TYPE_FINGER:
      return MotionEventAndroid::TOOL_TYPE_FINGER;
    case TOOL_TYPE_STYLUS:
      return MotionEventAndroid::TOOL_TYPE_STYLUS;
    case TOOL_TYPE_MOUSE:
      return MotionEventAndroid::TOOL_TYPE_MOUSE;
    case TOOL_TYPE_ERASER:
      return MotionEventAndroid::TOOL_TYPE_ERASER;
    default:
      NOTREACHED() << "Invalid Android MotionEvent tool type: "
                   << android_tool_type;
  };
  return MotionEventAndroid::TOOL_TYPE_UNKNOWN;
}

int FromAndroidButtonState(int button_state) {
  int result = 0;
  if ((button_state & BUTTON_BACK) != 0)
    result |= MotionEventAndroid::BUTTON_BACK;
  if ((button_state & BUTTON_FORWARD) != 0)
    result |= MotionEventAndroid::BUTTON_FORWARD;
  if ((button_state & BUTTON_PRIMARY) != 0)
    result |= MotionEventAndroid::BUTTON_PRIMARY;
  if ((button_state & BUTTON_SECONDARY) != 0)
    result |= MotionEventAndroid::BUTTON_SECONDARY;
  if ((button_state & BUTTON_TERTIARY) != 0)
    result |= MotionEventAndroid::BUTTON_TERTIARY;
  if ((button_state & BUTTON_STYLUS_PRIMARY) != 0)
    result |= MotionEventAndroid::BUTTON_STYLUS_PRIMARY;
  if ((button_state & BUTTON_STYLUS_SECONDARY) != 0)
    result |= MotionEventAndroid::BUTTON_STYLUS_SECONDARY;
  return result;
}

int ToEventFlags(int meta_state, int button_state) {
  int flags = ui::EF_NONE;

  if ((meta_state & AMETA_SHIFT_ON) != 0)
    flags |= ui::EF_SHIFT_DOWN;
  if ((meta_state & AMETA_CTRL_ON) != 0)
    flags |= ui::EF_CONTROL_DOWN;
  if ((meta_state & AMETA_ALT_ON) != 0)
    flags |= ui::EF_ALT_DOWN;
  if ((meta_state & AMETA_META_ON) != 0)
    flags |= ui::EF_COMMAND_DOWN;
  if ((meta_state & AMETA_CAPS_LOCK_ON) != 0)
    flags |= ui::EF_CAPS_LOCK_ON;

  if ((button_state & BUTTON_BACK) != 0)
    flags |= ui::EF_BACK_MOUSE_BUTTON;
  if ((button_state & BUTTON_FORWARD) != 0)
    flags |= ui::EF_FORWARD_MOUSE_BUTTON;
  if ((button_state & BUTTON_PRIMARY) != 0)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if ((button_state & BUTTON_SECONDARY) != 0)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if ((button_state & BUTTON_TERTIARY) != 0)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if ((button_state & BUTTON_STYLUS_PRIMARY) != 0)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if ((button_state & BUTTON_STYLUS_SECONDARY) != 0)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;

  return flags;
}

base::TimeTicks FromAndroidTime(int64_t time_ms) {
  base::TimeTicks timestamp =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}

float ToValidFloat(float x) {
  if (std::isnan(x))
    return 0.f;

  // Wildly large orientation values have been observed in the wild after device
  // rotation. There's not much we can do in that case other than simply
  // sanitize results beyond an absurd and arbitrary threshold.
  if (std::abs(x) > 1e5f)
    return 0.f;

  return x;
}

size_t ToValidHistorySize(jint history_size, ui::MotionEvent::Action action) {
  DCHECK_GE(history_size, 0);
  // While the spec states that only ACTION_MOVE events should contain
  // historical entries, it's possible that an embedder could repurpose an
  // ACTION_MOVE event into a different kind of event. In that case, the
  // historical values are meaningless, and should not be exposed.
  if (action != ui::MotionEvent::ACTION_MOVE)
    return 0;
  return history_size;
}

}  // namespace

MotionEventAndroid::Pointer::Pointer(jint id,
                                     jfloat pos_x_pixels,
                                     jfloat pos_y_pixels,
                                     jfloat touch_major_pixels,
                                     jfloat touch_minor_pixels,
                                     jfloat orientation_rad,
                                     jfloat tilt_rad,
                                     jint tool_type)
    : id(id),
      pos_x_pixels(pos_x_pixels),
      pos_y_pixels(pos_y_pixels),
      touch_major_pixels(touch_major_pixels),
      touch_minor_pixels(touch_minor_pixels),
      orientation_rad(orientation_rad),
      tilt_rad(tilt_rad),
      tool_type(tool_type) {
}

MotionEventAndroid::CachedPointer::CachedPointer()
    : id(0),
      touch_major(0),
      touch_minor(0),
      orientation(0),
      tilt(0),
      tool_type(TOOL_TYPE_UNKNOWN) {
}

MotionEventAndroid::MotionEventAndroid(float pix_to_dip,
                                       JNIEnv* env,
                                       jobject event,
                                       jlong time_ms,
                                       jint android_action,
                                       jint pointer_count,
                                       jint history_size,
                                       jint action_index,
                                       jint android_action_button,
                                       jint android_button_state,
                                       jint android_meta_state,
                                       jfloat raw_offset_x_pixels,
                                       jfloat raw_offset_y_pixels,
                                       const Pointer* const pointer0,
                                       const Pointer* const pointer1)
    : pix_to_dip_(pix_to_dip),
      cached_time_(FromAndroidTime(time_ms)),
      cached_action_(FromAndroidAction(android_action)),
      cached_pointer_count_(pointer_count),
      cached_history_size_(ToValidHistorySize(history_size, cached_action_)),
      cached_action_index_(action_index),
      cached_action_button_(android_action_button),
      cached_button_state_(FromAndroidButtonState(android_button_state)),
      cached_flags_(ToEventFlags(android_meta_state, android_button_state)),
      cached_raw_position_offset_(ToDips(raw_offset_x_pixels),
                                  ToDips(raw_offset_y_pixels)),
      unique_event_id_(ui::GetNextTouchEventId()) {
  DCHECK_GT(cached_pointer_count_, 0U);
  DCHECK(cached_pointer_count_ == 1 || pointer1);

  event_.Reset(env, event);
  if (cached_pointer_count_ > MAX_POINTERS_TO_CACHE || cached_history_size_ > 0)
    DCHECK(event_.obj());

  cached_pointers_[0] = FromAndroidPointer(*pointer0);
  if (cached_pointer_count_ > 1)
    cached_pointers_[1] = FromAndroidPointer(*pointer1);
}

MotionEventAndroid::MotionEventAndroid(const MotionEventAndroid& e)
    : event_(e.event_),
      pix_to_dip_(e.pix_to_dip_),
      cached_time_(e.cached_time_),
      cached_action_(e.cached_action_),
      cached_pointer_count_(e.cached_pointer_count_),
      cached_history_size_(e.cached_history_size_),
      cached_action_index_(e.cached_action_index_),
      cached_action_button_(e.cached_action_button_),
      cached_button_state_(e.cached_button_state_),
      cached_flags_(e.cached_flags_),
      cached_raw_position_offset_(e.cached_raw_position_offset_),
      unique_event_id_(ui::GetNextTouchEventId()) {
  for (size_t i = 0; i < cached_pointer_count_; i++)
    cached_pointers_[i] = e.cached_pointers_[i];
}

std::unique_ptr<MotionEventAndroid> MotionEventAndroid::Offset(float x,
                                                               float y) const {
  std::unique_ptr<MotionEventAndroid> event(new MotionEventAndroid(*this));
  for (size_t i = 0; i < cached_pointer_count_; i++) {
    event->cached_pointers_[i] = OffsetCachedPointer(cached_pointers_[i], x, y);
  }
  return event;
}

MotionEventAndroid::~MotionEventAndroid() {
}

uint32_t MotionEventAndroid::GetUniqueEventId() const {
  return unique_event_id_;
}

MotionEventAndroid::Action MotionEventAndroid::GetAction() const {
  return cached_action_;
}

int MotionEventAndroid::GetActionButton() const {
  return cached_action_button_;
}

ScopedJavaLocalRef<jobject> MotionEventAndroid::GetJavaObject() const {
  return ScopedJavaLocalRef<jobject>(event_);
}

int MotionEventAndroid::GetActionIndex() const {
  DCHECK(cached_action_ == MotionEvent::ACTION_POINTER_UP ||
         cached_action_ == MotionEvent::ACTION_POINTER_DOWN)
      << "Invalid action for GetActionIndex(): " << cached_action_;
  DCHECK_GE(cached_action_index_, 0);
  DCHECK_LT(cached_action_index_, static_cast<int>(cached_pointer_count_));
  return cached_action_index_;
}

size_t MotionEventAndroid::GetPointerCount() const {
  return cached_pointer_count_;
}

int MotionEventAndroid::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].id;
  return Java_MotionEvent_getPointerId(AttachCurrentThread(), event_,
                                       pointer_index);
}

float MotionEventAndroid::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].position.x();
  return ToDips(
      Java_MotionEvent_getXF_I(AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].position.y();
  return ToDips(
      Java_MotionEvent_getYF_I(AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetRawX(size_t pointer_index) const {
  return GetX(pointer_index) + cached_raw_position_offset_.x();
}

float MotionEventAndroid::GetRawY(size_t pointer_index) const {
  return GetY(pointer_index) + cached_raw_position_offset_.y();
}

float MotionEventAndroid::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].touch_major;
  return ToDips(Java_MotionEvent_getTouchMajorF_I(AttachCurrentThread(), event_,
                                                  pointer_index));
}

float MotionEventAndroid::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].touch_minor;
  return ToDips(Java_MotionEvent_getTouchMinorF_I(AttachCurrentThread(), event_,
                                                  pointer_index));
}

float MotionEventAndroid::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].orientation;
  return ToValidFloat(Java_MotionEvent_getOrientationF_I(
      AttachCurrentThread(), event_, pointer_index));
}

float MotionEventAndroid::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  // Note that this early return is a special case exercised only in testing, as
  // caching the pressure values is not a worthwhile optimization (they're
  // accessed at most once per event instance).
  if (!event_.obj())
    return 0.f;
  if (cached_action_ == MotionEvent::ACTION_UP)
    return 0.f;
  return Java_MotionEvent_getPressureF_I(AttachCurrentThread(), event_,
                                         pointer_index);
}

float MotionEventAndroid::GetTilt(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].tilt;
  if (!event_.obj())
    return 0.f;
  return ToValidFloat(Java_MotionEvent_getAxisValueF_I_I(
      AttachCurrentThread(), event_, AXIS_TILT, pointer_index));
}

base::TimeTicks MotionEventAndroid::GetEventTime() const {
  return cached_time_;
}

size_t MotionEventAndroid::GetHistorySize() const {
  return cached_history_size_;
}

base::TimeTicks MotionEventAndroid::GetHistoricalEventTime(
    size_t historical_index) const {
  return FromAndroidTime(Java_MotionEvent_getHistoricalEventTime(
      AttachCurrentThread(), event_, historical_index));
}

float MotionEventAndroid::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  return ToDips(Java_MotionEvent_getHistoricalTouchMajorF_I_I(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalX(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(Java_MotionEvent_getHistoricalXF_I_I(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

float MotionEventAndroid::GetHistoricalY(size_t pointer_index,
                                         size_t historical_index) const {
  return ToDips(Java_MotionEvent_getHistoricalYF_I_I(
      AttachCurrentThread(), event_, pointer_index, historical_index));
}

ui::MotionEvent::ToolType MotionEventAndroid::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index < MAX_POINTERS_TO_CACHE)
    return cached_pointers_[pointer_index].tool_type;
  return FromAndroidToolType(Java_MotionEvent_getToolType(
      AttachCurrentThread(), event_, pointer_index));
}

int MotionEventAndroid::GetButtonState() const {
  return cached_button_state_;
}

int MotionEventAndroid::GetFlags() const {
  return cached_flags_;
}

float MotionEventAndroid::ToDips(float pixels) const {
  return pixels * pix_to_dip_;
}

MotionEventAndroid::CachedPointer MotionEventAndroid::FromAndroidPointer(
    const Pointer& pointer) const {
  CachedPointer result;
  result.id = pointer.id;
  result.position =
      gfx::PointF(ToDips(pointer.pos_x_pixels), ToDips(pointer.pos_y_pixels));
  result.touch_major = ToDips(pointer.touch_major_pixels);
  result.touch_minor = ToDips(pointer.touch_minor_pixels);
  result.orientation = ToValidFloat(pointer.orientation_rad);
  result.tilt = ToValidFloat(pointer.tilt_rad);
  result.tool_type = FromAndroidToolType(pointer.tool_type);
  return result;
}

MotionEventAndroid::CachedPointer MotionEventAndroid::OffsetCachedPointer(
    const CachedPointer& pointer,
    float x,
    float y) const {
  CachedPointer result;
  result.id = pointer.id;
  result.position = gfx::PointF(pointer.position.x() + ToDips(x),
                                pointer.position.y() + ToDips(y));
  result.touch_major = pointer.touch_major;
  result.touch_minor = pointer.touch_minor;
  result.orientation = pointer.orientation;
  result.tilt = pointer.tilt;
  result.tool_type = pointer.tool_type;
  return result;
}

}  // namespace ui
