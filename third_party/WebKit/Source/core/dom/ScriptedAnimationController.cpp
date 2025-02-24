/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/ScriptedAnimationController.h"

#include "core/css/MediaQueryListListener.h"
#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"

namespace blink {

std::pair<EventTarget*, StringImpl*> eventTargetKey(const Event* event) {
  return std::make_pair(event->target(), event->type().impl());
}

ScriptedAnimationController::ScriptedAnimationController(Document* document)
    : m_document(document), m_callbackCollection(document), m_suspendCount(0) {}

DEFINE_TRACE(ScriptedAnimationController) {
  visitor->trace(m_document);
  visitor->trace(m_callbackCollection);
  visitor->trace(m_eventQueue);
  visitor->trace(m_mediaQueryListListeners);
  visitor->trace(m_perFrameEvents);
}

void ScriptedAnimationController::suspend() {
  ++m_suspendCount;
}

void ScriptedAnimationController::resume() {
  // It would be nice to put an DCHECK(m_suspendCount > 0) here, but in WK1
  // resume() can be called even when suspend hasn't (if a tab was created in
  // the background).
  if (m_suspendCount > 0)
    --m_suspendCount;
  scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::dispatchEventsAndCallbacksForPrinting() {
  dispatchEvents(EventNames::MediaQueryListEvent);
  callMediaQueryListListeners();
}

ScriptedAnimationController::CallbackId
ScriptedAnimationController::registerCallback(FrameRequestCallback* callback) {
  CallbackId id = m_callbackCollection.registerCallback(callback);
  scheduleAnimationIfNeeded();
  return id;
}

void ScriptedAnimationController::cancelCallback(CallbackId id) {
  m_callbackCollection.cancelCallback(id);
}

void ScriptedAnimationController::runTasks() {
  Vector<std::unique_ptr<WTF::Closure>> tasks;
  tasks.swap(m_taskQueue);
  for (auto& task : tasks)
    (*task)();
}

void ScriptedAnimationController::dispatchEvents(
    const AtomicString& eventInterfaceFilter) {
  HeapVector<Member<Event>> events;
  if (eventInterfaceFilter.isEmpty()) {
    events.swap(m_eventQueue);
    m_perFrameEvents.clear();
  } else {
    HeapVector<Member<Event>> remaining;
    for (auto& event : m_eventQueue) {
      if (event && event->interfaceName() == eventInterfaceFilter) {
        m_perFrameEvents.erase(eventTargetKey(event.get()));
        events.push_back(event.release());
      } else {
        remaining.push_back(event.release());
      }
    }
    remaining.swap(m_eventQueue);
  }

  for (const auto& event : events) {
    EventTarget* eventTarget = event->target();
    // FIXME: we should figure out how to make dispatchEvent properly virtual to
    // avoid special casting window.
    // FIXME: We should not fire events for nodes that are no longer in the
    // tree.
    probe::AsyncTask asyncTask(eventTarget->getExecutionContext(), event);
    if (LocalDOMWindow* window = eventTarget->toLocalDOMWindow())
      window->dispatchEvent(event, nullptr);
    else
      eventTarget->dispatchEvent(event);
  }
}

void ScriptedAnimationController::executeCallbacks(double monotonicTimeNow) {
  // dispatchEvents() runs script which can cause the document to be destroyed.
  if (!m_document)
    return;

  double highResNowMs =
      1000.0 *
      m_document->loader()->timing().monotonicTimeToZeroBasedDocumentTime(
          monotonicTimeNow);
  double legacyHighResNowMs =
      1000.0 *
      m_document->loader()->timing().monotonicTimeToPseudoWallTime(
          monotonicTimeNow);
  m_callbackCollection.executeCallbacks(highResNowMs, legacyHighResNowMs);
}

void ScriptedAnimationController::callMediaQueryListListeners() {
  MediaQueryListListeners listeners;
  listeners.swap(m_mediaQueryListListeners);

  for (const auto& listener : listeners) {
    listener->notifyMediaQueryChanged();
  }
}

bool ScriptedAnimationController::hasScheduledItems() const {
  if (m_suspendCount)
    return false;

  return !m_callbackCollection.isEmpty() || !m_taskQueue.isEmpty() ||
         !m_eventQueue.isEmpty() || !m_mediaQueryListListeners.isEmpty();
}

void ScriptedAnimationController::serviceScriptedAnimations(
    double monotonicTimeNow) {
  if (!hasScheduledItems())
    return;

  callMediaQueryListListeners();
  dispatchEvents();
  runTasks();
  executeCallbacks(monotonicTimeNow);

  scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::enqueueTask(
    std::unique_ptr<WTF::Closure> task) {
  m_taskQueue.push_back(std::move(task));
  scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::enqueueEvent(Event* event) {
  probe::asyncTaskScheduled(event->target()->getExecutionContext(),
                            event->type(), event);
  m_eventQueue.push_back(event);
  scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::enqueuePerFrameEvent(Event* event) {
  if (!m_perFrameEvents.insert(eventTargetKey(event)).isNewEntry)
    return;
  enqueueEvent(event);
}

void ScriptedAnimationController::enqueueMediaQueryChangeListeners(
    HeapVector<Member<MediaQueryListListener>>& listeners) {
  for (const auto& listener : listeners) {
    m_mediaQueryListListeners.insert(listener);
  }
  scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::scheduleAnimationIfNeeded() {
  if (!hasScheduledItems())
    return;

  if (!m_document)
    return;

  if (FrameView* frameView = m_document->view())
    frameView->scheduleAnimation();
}

}  // namespace blink
