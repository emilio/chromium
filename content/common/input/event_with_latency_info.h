// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_EVENT_WITH_LATENCY_INFO_H_
#define CONTENT_COMMON_INPUT_EVENT_WITH_LATENCY_INFO_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "third_party/WebKit/public/platform/WebTouchEvent.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/latency_info.h"

namespace content {

template <typename T>
class EventWithLatencyInfo {
 public:
  T event;
  mutable ui::LatencyInfo latency;

  explicit EventWithLatencyInfo(const T& e) : event(e) {}

  EventWithLatencyInfo(const T& e, const ui::LatencyInfo& l)
      : event(e), latency(l) {}

  EventWithLatencyInfo(blink::WebInputEvent::Type type,
                       int modifiers,
                       double timeStampSeconds,
                       const ui::LatencyInfo& l)
      : event(type, modifiers, timeStampSeconds), latency(l) {}

  EventWithLatencyInfo() {}

  bool CanCoalesceWith(const EventWithLatencyInfo& other)
      const WARN_UNUSED_RESULT {
    if (other.event.type() != event.type())
      return false;

    DCHECK_EQ(sizeof(T), event.size());
    DCHECK_EQ(sizeof(T), other.event.size());

    return ui::CanCoalesce(other.event, event);
  }

  void CoalesceWith(const EventWithLatencyInfo& other) {
    // |other| should be a newer event than |this|.
    if (other.latency.trace_id() >= 0 && latency.trace_id() >= 0)
      DCHECK_GT(other.latency.trace_id(), latency.trace_id());

    // New events get coalesced into older events, and the newer timestamp
    // should always be preserved.
    const double time_stamp_seconds = other.event.timeStampSeconds();
    ui::Coalesce(other.event, &event);
    event.setTimeStampSeconds(time_stamp_seconds);

    // When coalescing two input events, we keep the oldest LatencyInfo
    // for Telemetry latency tests, since it will represent the longest
    // latency.
    other.latency = latency;
    other.latency.set_coalesced();
  }
};

typedef EventWithLatencyInfo<blink::WebGestureEvent>
    GestureEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebMouseWheelEvent>
    MouseWheelEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebMouseEvent>
    MouseEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebTouchEvent>
    TouchEventWithLatencyInfo;

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_EVENT_WITH_LATENCY_INFO_H_
