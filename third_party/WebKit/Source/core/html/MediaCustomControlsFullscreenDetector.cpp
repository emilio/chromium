// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/MediaCustomControlsFullscreenDetector.h"

#include "core/dom/Fullscreen.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/html/HTMLVideoElement.h"
#include "core/layout/IntersectionGeometry.h"

namespace blink {

namespace {

constexpr double kCheckFullscreenIntervalSeconds = 1.0f;
constexpr float kMostlyFillViewportThresholdOfOccupationProportion = 0.85f;
constexpr float kMostlyFillViewportThresholdOfVisibleProportion = 0.75f;

}  // anonymous namespace

MediaCustomControlsFullscreenDetector::MediaCustomControlsFullscreenDetector(
    HTMLVideoElement& video)
    : EventListener(CPPEventListenerType),
      ContextLifecycleObserver(nullptr),
      m_videoElement(video),
      m_checkViewportIntersectionTimer(
          TaskRunnerHelper::get(TaskType::Unthrottled, &video.document()),
          this,
          &MediaCustomControlsFullscreenDetector::
              onCheckViewportIntersectionTimerFired) {
  videoElement().addEventListener(EventTypeNames::DOMNodeInsertedIntoDocument,
                                  this, false);
  videoElement().addEventListener(EventTypeNames::DOMNodeRemovedFromDocument,
                                  this, false);

  videoElement().addEventListener(EventTypeNames::loadedmetadata, this, true);
}

bool MediaCustomControlsFullscreenDetector::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaCustomControlsFullscreenDetector::attach() {
  setContext(&videoElement().document());
  videoElement().document().addEventListener(
      EventTypeNames::webkitfullscreenchange, this, true);
  videoElement().document().addEventListener(EventTypeNames::fullscreenchange,
                                             this, true);
}

void MediaCustomControlsFullscreenDetector::detach() {
  setContext(nullptr);
  videoElement().document().removeEventListener(
      EventTypeNames::webkitfullscreenchange, this, true);
  videoElement().document().removeEventListener(
      EventTypeNames::fullscreenchange, this, true);
  m_checkViewportIntersectionTimer.stop();
}

bool MediaCustomControlsFullscreenDetector::computeIsDominantVideoForTests(
    const IntRect& targetRect,
    const IntRect& rootRect,
    const IntRect& intersectionRect) {
  if (targetRect.isEmpty() || rootRect.isEmpty())
    return false;

  const float xOccupationProportion =
      1.0f * intersectionRect.width() / rootRect.width();
  const float yOccupationProportion =
      1.0f * intersectionRect.height() / rootRect.height();

  // If the viewport is mostly occupied by the video, return true.
  if (std::min(xOccupationProportion, yOccupationProportion) >=
      kMostlyFillViewportThresholdOfOccupationProportion) {
    return true;
  }

  // If neither of the dimensions of the viewport is mostly occupied by the
  // video, return false.
  if (std::max(xOccupationProportion, yOccupationProportion) <
      kMostlyFillViewportThresholdOfOccupationProportion) {
    return false;
  }

  // If the video is mostly visible in the indominant dimension, return true.
  // Otherwise return false.
  if (xOccupationProportion > yOccupationProportion) {
    return targetRect.height() *
               kMostlyFillViewportThresholdOfVisibleProportion <
           intersectionRect.height();
  }
  return targetRect.width() * kMostlyFillViewportThresholdOfVisibleProportion <
         intersectionRect.width();
}

void MediaCustomControlsFullscreenDetector::handleEvent(
    ExecutionContext* context,
    Event* event) {
  DCHECK(event->type() == EventTypeNames::DOMNodeInsertedIntoDocument ||
         event->type() == EventTypeNames::DOMNodeRemovedFromDocument ||
         event->type() == EventTypeNames::loadedmetadata ||
         event->type() == EventTypeNames::webkitfullscreenchange ||
         event->type() == EventTypeNames::fullscreenchange);

  // Don't early return when the video is inserted into/removed from the
  // document, as the document might already be in fullscreen.
  if (event->type() == EventTypeNames::DOMNodeInsertedIntoDocument) {
    attach();
  } else if (event->type() == EventTypeNames::DOMNodeRemovedFromDocument) {
    detach();
  }

  // Video is not loaded yet.
  if (videoElement().getReadyState() < HTMLMediaElement::kHaveMetadata)
    return;

  if (!videoElement().isConnected() || !isVideoOrParentFullscreen()) {
    m_checkViewportIntersectionTimer.stop();

    if (videoElement().webMediaPlayer())
      videoElement().webMediaPlayer()->setIsEffectivelyFullscreen(false);

    return;
  }

  m_checkViewportIntersectionTimer.startOneShot(kCheckFullscreenIntervalSeconds,
                                                BLINK_FROM_HERE);
}

void MediaCustomControlsFullscreenDetector::contextDestroyed(
    ExecutionContext*) {
  if (videoElement().webMediaPlayer())
    videoElement().webMediaPlayer()->setIsEffectivelyFullscreen(false);

  detach();
}

void MediaCustomControlsFullscreenDetector::
    onCheckViewportIntersectionTimerFired(TimerBase*) {
  DCHECK(isVideoOrParentFullscreen());
  IntersectionGeometry geometry(nullptr, videoElement(), Vector<Length>(),
                                true);
  geometry.computeGeometry();

  bool isDominant = computeIsDominantVideoForTests(
      geometry.targetIntRect(), geometry.rootIntRect(),
      geometry.intersectionIntRect());

  if (videoElement().webMediaPlayer())
    videoElement().webMediaPlayer()->setIsEffectivelyFullscreen(isDominant);
}

bool MediaCustomControlsFullscreenDetector::isVideoOrParentFullscreen() {
  Element* fullscreenElement =
      Fullscreen::currentFullScreenElementFrom(videoElement().document());
  if (!fullscreenElement)
    return false;

  return fullscreenElement->contains(&videoElement());
}

DEFINE_TRACE(MediaCustomControlsFullscreenDetector) {
  EventListener::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  visitor->trace(m_videoElement);
}

}  // namespace blink
