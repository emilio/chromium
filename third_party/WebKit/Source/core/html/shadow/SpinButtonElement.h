/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpinButtonElement_h
#define SpinButtonElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLDivElement.h"
#include "core/page/PopupOpeningObserver.h"
#include "platform/Timer.h"

namespace blink {

class CORE_EXPORT SpinButtonElement final : public HTMLDivElement,
                                            public PopupOpeningObserver {
 public:
  enum UpDownState {
    Indeterminate,  // Hovered, but the event is not handled.
    Down,
    Up,
  };
  enum EventDispatch {
    EventDispatchAllowed,
    EventDispatchDisallowed,
  };
  class SpinButtonOwner : public GarbageCollectedMixin {
   public:
    virtual ~SpinButtonOwner() {}
    virtual void focusAndSelectSpinButtonOwner() = 0;
    virtual bool shouldSpinButtonRespondToMouseEvents() = 0;
    virtual bool shouldSpinButtonRespondToWheelEvents() = 0;
    virtual void spinButtonDidReleaseMouseCapture(EventDispatch) = 0;
    virtual void spinButtonStepDown() = 0;
    virtual void spinButtonStepUp() = 0;
  };

  // The owner of SpinButtonElement must call removeSpinButtonOwner
  // because SpinButtonElement can be outlive SpinButtonOwner
  // implementation, e.g. during event handling.
  static SpinButtonElement* create(Document&, SpinButtonOwner&);
  UpDownState getUpDownState() const { return m_upDownState; }
  void releaseCapture(EventDispatch = EventDispatchAllowed);
  void removeSpinButtonOwner() { m_spinButtonOwner = nullptr; }

  void step(int amount);

  bool willRespondToMouseMoveEvents() override;
  bool willRespondToMouseClickEvents() override;

  void forwardEvent(Event*);

  DECLARE_VIRTUAL_TRACE();

 private:
  SpinButtonElement(Document&, SpinButtonOwner&);

  void detachLayoutTree(const AttachContext&) override;
  bool isSpinButtonElement() const override { return true; }
  bool isDisabledFormControl() const override {
    return ownerShadowHost() && ownerShadowHost()->isDisabledFormControl();
  }
  bool matchesReadOnlyPseudoClass() const override;
  bool matchesReadWritePseudoClass() const override;
  void defaultEventHandler(Event*) override;
  void willOpenPopup() override;
  void doStepAction(int);
  void startRepeatingTimer();
  void stopRepeatingTimer();
  void repeatingTimerFired(TimerBase*);
  void setHovered(bool = true) override;
  bool shouldRespondToMouseEvents();
  bool isMouseFocusable() const override { return false; }

  Member<SpinButtonOwner> m_spinButtonOwner;
  bool m_capturing;
  UpDownState m_upDownState;
  UpDownState m_pressStartingState;
  TaskRunnerTimer<SpinButtonElement> m_repeatingTimer;
};

DEFINE_TYPE_CASTS(SpinButtonElement,
                  Node,
                  node,
                  toElement(node)->isSpinButtonElement(),
                  toElement(node).isSpinButtonElement());

}  // namespace blink

#endif
