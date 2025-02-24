/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGSMILElement_h
#define SVGSMILElement_h

#include "core/CoreExport.h"
#include "core/SVGNames.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGTests.h"
#include "core/svg/animation/SMILTime.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"

namespace blink {

class ConditionEventListener;
class SMILTimeContainer;
class SVGSMILElement;

// This class implements SMIL interval timing model as needed for SVG animation.
class CORE_EXPORT SVGSMILElement : public SVGElement, public SVGTests {
  USING_GARBAGE_COLLECTED_MIXIN(SVGSMILElement);

 public:
  SVGSMILElement(const QualifiedName&, Document&);
  ~SVGSMILElement() override;

  void parseAttribute(const AttributeModificationParams&) override;
  void svgAttributeChanged(const QualifiedName&) override;
  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;

  virtual bool hasValidTarget();
  virtual void animationAttributeChanged() = 0;

  SMILTimeContainer* timeContainer() const { return m_timeContainer.get(); }

  SVGElement* targetElement() const { return m_targetElement; }
  const QualifiedName& attributeName() const { return m_attributeName; }

  void beginByLinkActivation();

  enum Restart { RestartAlways, RestartWhenNotActive, RestartNever };

  Restart getRestart() const;

  enum FillMode { FillRemove, FillFreeze };

  FillMode fill() const;

  SMILTime dur() const;
  SMILTime repeatDur() const;
  SMILTime repeatCount() const;
  SMILTime maxValue() const;
  SMILTime minValue() const;

  SMILTime elapsed() const;

  SMILTime intervalBegin() const { return m_interval.begin; }
  SMILTime previousIntervalBegin() const { return m_previousIntervalBegin; }
  SMILTime simpleDuration() const;

  void seekToIntervalCorrespondingToTime(double elapsed);
  bool progress(double elapsed, bool seekToTime);
  SMILTime nextProgressTime() const;
  void updateAnimatedValue(SVGSMILElement* resultElement) {
    updateAnimation(m_lastPercent, m_lastRepeat, resultElement);
  }

  void reset();

  static SMILTime parseClockValue(const String&);
  static SMILTime parseOffsetValue(const String&);

  bool isContributing(double elapsed) const;
  bool isFrozen() const;

  unsigned documentOrderIndex() const { return m_documentOrderIndex; }
  void setDocumentOrderIndex(unsigned index) { m_documentOrderIndex = index; }

  virtual void resetAnimatedType() = 0;
  virtual void clearAnimatedType() = 0;
  virtual void applyResultsToTarget() = 0;

  bool animatedTypeIsLocked() const { return m_animatedPropertyLocked; }
  void lockAnimatedType() {
    DCHECK(!m_animatedPropertyLocked);
    m_animatedPropertyLocked = true;
  }
  void unlockAnimatedType() {
    DCHECK(m_animatedPropertyLocked);
    m_animatedPropertyLocked = false;
  }

  void connectSyncBaseConditions();
  void connectEventBaseConditions();

  void scheduleEvent(const AtomicString& eventType);
  void scheduleRepeatEvents(unsigned);
  void dispatchPendingEvent(const AtomicString& eventType);

  virtual bool isSVGDiscardElement() const { return false; }

  DECLARE_VIRTUAL_TRACE();

 protected:
  enum BeginOrEnd { Begin, End };

  void addInstanceTime(
      BeginOrEnd,
      SMILTime,
      SMILTimeWithOrigin::Origin = SMILTimeWithOrigin::ParserOrigin);

  void setInactive() { m_activeState = Inactive; }

  // Sub-classes may need to take action when the target is changed.
  virtual void setTargetElement(SVGElement*);

  void schedule();
  void unscheduleIfScheduled();

  QualifiedName m_attributeName;

 private:
  void buildPendingResource() override;
  void clearResourceAndEventBaseReferences();
  void clearConditions();

  virtual void startedActiveInterval() = 0;
  void endedActiveInterval();
  virtual void updateAnimation(float percent,
                               unsigned repeat,
                               SVGSMILElement* resultElement) = 0;

  bool layoutObjectIsNeeded(const ComputedStyle&) override { return false; }

  SMILTime findInstanceTime(BeginOrEnd,
                            SMILTime minimumTime,
                            bool equalsMinimumOK) const;

  enum IntervalSelector { FirstInterval, NextInterval };

  SMILInterval resolveInterval(IntervalSelector) const;
  void resolveFirstInterval();
  bool resolveNextInterval();
  SMILTime resolveActiveEnd(SMILTime resolvedBegin, SMILTime resolvedEnd) const;
  SMILTime repeatingDuration() const;

  enum RestartedInterval { DidNotRestartInterval, DidRestartInterval };

  RestartedInterval maybeRestartInterval(double elapsed);
  void beginListChanged(SMILTime eventTime);
  void endListChanged(SMILTime eventTime);

  // This represents conditions on elements begin or end list that need to be
  // resolved on runtime, for example
  // <animate begin="otherElement.begin + 8s; button.click" ... />
  class Condition : public GarbageCollectedFinalized<Condition> {
   public:
    enum Type { EventBase, Syncbase, AccessKey };

    static Condition* create(Type type,
                             BeginOrEnd beginOrEnd,
                             const AtomicString& baseID,
                             const AtomicString& name,
                             SMILTime offset,
                             int repeat = -1) {
      return new Condition(type, beginOrEnd, baseID, name, offset, repeat);
    }
    ~Condition();
    DECLARE_TRACE();

    Type getType() const { return m_type; }
    BeginOrEnd getBeginOrEnd() const { return m_beginOrEnd; }
    const AtomicString& name() const { return m_name; }
    SMILTime offset() const { return m_offset; }
    int repeat() const { return m_repeat; }

    void connectSyncBase(SVGSMILElement&);
    void disconnectSyncBase(SVGSMILElement&);
    bool syncBaseEquals(SVGSMILElement& timedElement) const {
      return m_baseElement == timedElement;
    }

    void connectEventBase(SVGSMILElement&);
    void disconnectEventBase(SVGSMILElement&);

   private:
    Condition(Type,
              BeginOrEnd,
              const AtomicString& baseID,
              const AtomicString& name,
              SMILTime offset,
              int repeat);

    SVGElement* lookupEventBase(SVGSMILElement&) const;

    Type m_type;
    BeginOrEnd m_beginOrEnd;
    AtomicString m_baseID;
    AtomicString m_name;
    SMILTime m_offset;
    int m_repeat;
    Member<SVGElement> m_baseElement;
    Member<ConditionEventListener> m_eventListener;
  };
  bool parseCondition(const String&, BeginOrEnd beginOrEnd);
  void parseBeginOrEnd(const String&, BeginOrEnd beginOrEnd);

  void disconnectSyncBaseConditions();
  void disconnectEventBaseConditions();

  void notifyDependentsIntervalChanged();
  void createInstanceTimesFromSyncbase(SVGSMILElement& syncbase);
  void addSyncBaseDependent(SVGSMILElement&);
  void removeSyncBaseDependent(SVGSMILElement&);

  enum ActiveState { Inactive, Active, Frozen };

  ActiveState determineActiveState(SMILTime elapsed) const;
  float calculateAnimationPercentAndRepeat(double elapsed,
                                           unsigned& repeat) const;
  SMILTime calculateNextProgressTime(double elapsed) const;

  Member<SVGElement> m_targetElement;

  HeapVector<Member<Condition>> m_conditions;
  bool m_syncBaseConditionsConnected;
  bool m_hasEndEventConditions;

  bool m_isWaitingForFirstInterval;
  bool m_isScheduled;

  using TimeDependentSet = HeapHashSet<Member<SVGSMILElement>>;
  TimeDependentSet m_syncBaseDependents;

  // Instance time lists
  Vector<SMILTimeWithOrigin> m_beginTimes;
  Vector<SMILTimeWithOrigin> m_endTimes;

  // This is the upcoming or current interval
  SMILInterval m_interval;

  SMILTime m_previousIntervalBegin;

  ActiveState m_activeState;
  float m_lastPercent;
  unsigned m_lastRepeat;

  SMILTime m_nextProgressTime;

  Member<SMILTimeContainer> m_timeContainer;
  unsigned m_documentOrderIndex;

  Vector<unsigned> m_repeatEventCountList;

  mutable SMILTime m_cachedDur;
  mutable SMILTime m_cachedRepeatDur;
  mutable SMILTime m_cachedRepeatCount;
  mutable SMILTime m_cachedMin;
  mutable SMILTime m_cachedMax;

  bool m_animatedPropertyLocked;

  friend class ConditionEventListener;
};

inline bool isSVGSMILElement(const SVGElement& element) {
  return element.hasTagName(SVGNames::setTag) ||
         element.hasTagName(SVGNames::animateTag) ||
         element.hasTagName(SVGNames::animateMotionTag) ||
         element.hasTagName(SVGNames::animateTransformTag) ||
         element.hasTagName((SVGNames::discardTag));
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGSMILElement);

}  // namespace blink

#endif  // SVGSMILElement_h
