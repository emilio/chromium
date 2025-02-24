// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_CLIENT_H_
#define UI_ANDROID_VIEW_CLIENT_H_

#include "ui/android/ui_android_export.h"

namespace ui {

class MotionEventAndroid;

// Client interface used to forward events from Java to native views.
// Calls are dispatched to its children along the hierarchy of ViewAndroid.
// Use bool return type to stop propagating the call i.e. overriden method
// should return true to indicate that the event was handled and stop
// the processing.
class UI_ANDROID_EXPORT ViewClient {
 public:
  virtual bool OnTouchEvent(const MotionEventAndroid& event,
                            bool for_touch_handle);
  virtual bool OnMouseEvent(const MotionEventAndroid& event);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_CLIENT_H_
