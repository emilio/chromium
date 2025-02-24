// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTraceEvents_h
#define InspectorTraceEvents_h

#include <memory>

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"

namespace v8 {
class Function;
template <typename T>
class Local;
}

namespace WTF {
class TextPosition;
}

namespace blink {
class Animation;
class CSSStyleSheetResource;
class ContainerNode;
class Document;
class DocumentLoader;
class Element;
class Event;
class ExecutionContext;
struct FetchInitiatorInfo;
class FrameView;
class GraphicsLayer;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class ImageResourceContent;
class InvalidationSet;
class LayoutImage;
class LayoutObject;
class LayoutRect;
class LocalFrame;
class Node;
class PaintLayer;
class QualifiedName;
class Resource;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class StyleChangeReasonForTracing;
class StyleImage;
class WorkerThread;
class XMLHttpRequest;

namespace probe {
class CallFunction;
class ExecuteScript;
class ParseHTML;
}

enum ResourceLoadPriority : int;

class CORE_EXPORT InspectorTraceEvents : public InspectorAgent {
  WTF_MAKE_NONCOPYABLE(InspectorTraceEvents);

 public:
  InspectorTraceEvents() {}

  void init(InstrumentingAgents*,
            protocol::UberDispatcher*,
            protocol::DictionaryValue*) override;
  void dispose() override;

  void willSendRequest(LocalFrame*,
                       unsigned long identifier,
                       DocumentLoader*,
                       ResourceRequest&,
                       const ResourceResponse& redirectResponse,
                       const FetchInitiatorInfo&);
  void didReceiveResourceResponse(LocalFrame*,
                                  unsigned long identifier,
                                  DocumentLoader*,
                                  const ResourceResponse&,
                                  Resource*);
  void didReceiveData(LocalFrame*,
                      unsigned long identifier,
                      const char* data,
                      int dataLength);
  void didFinishLoading(unsigned long identifier,
                        double monotonicFinishTime,
                        int64_t encodedDataLength,
                        int64_t decodedBodyLength);
  void didFailLoading(unsigned long identifier, const ResourceError&);

  void will(const probe::ExecuteScript&);
  void did(const probe::ExecuteScript&);

  void will(const probe::ParseHTML&);
  void did(const probe::ParseHTML&);

  void will(const probe::CallFunction&);
  void did(const probe::CallFunction&);

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<InstrumentingAgents> m_instrumentingAgents;
};

namespace InspectorLayoutEvent {
std::unique_ptr<TracedValue> beginData(FrameView*);
std::unique_ptr<TracedValue> endData(LayoutObject* rootForThisLayout);
}

namespace InspectorScheduleStyleInvalidationTrackingEvent {
extern const char Attribute[];
extern const char Class[];
extern const char Id[];
extern const char Pseudo[];
extern const char RuleSet[];

std::unique_ptr<TracedValue> attributeChange(Element&,
                                             const InvalidationSet&,
                                             const QualifiedName&);
std::unique_ptr<TracedValue> classChange(Element&,
                                         const InvalidationSet&,
                                         const AtomicString&);
std::unique_ptr<TracedValue> idChange(Element&,
                                      const InvalidationSet&,
                                      const AtomicString&);
std::unique_ptr<TracedValue> pseudoChange(Element&,
                                          const InvalidationSet&,
                                          CSSSelector::PseudoType);
std::unique_ptr<TracedValue> ruleSetInvalidation(ContainerNode&,
                                                 const InvalidationSet&);
}  // namespace InspectorScheduleStyleInvalidationTrackingEvent

#define TRACE_SCHEDULE_STYLE_INVALIDATION(element, invalidationSet,          \
                                          changeType, ...)                   \
  TRACE_EVENT_INSTANT1(                                                      \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),   \
      "ScheduleStyleInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data", \
      InspectorScheduleStyleInvalidationTrackingEvent::changeType(           \
          (element), (invalidationSet), ##__VA_ARGS__));

namespace InspectorStyleRecalcInvalidationTrackingEvent {
std::unique_ptr<TracedValue> data(Node*, const StyleChangeReasonForTracing&);
}

String descendantInvalidationSetToIdString(const InvalidationSet&);

namespace InspectorStyleInvalidatorInvalidateEvent {
extern const char ElementHasPendingInvalidationList[];
extern const char InvalidateCustomPseudo[];
extern const char InvalidationSetMatchedAttribute[];
extern const char InvalidationSetMatchedClass[];
extern const char InvalidationSetMatchedId[];
extern const char InvalidationSetMatchedTagName[];
extern const char PreventStyleSharingForParent[];

std::unique_ptr<TracedValue> data(Element&, const char* reason);
std::unique_ptr<TracedValue> selectorPart(Element&,
                                          const char* reason,
                                          const InvalidationSet&,
                                          const String&);
std::unique_ptr<TracedValue> invalidationList(
    ContainerNode&,
    const Vector<RefPtr<InvalidationSet>>&);
}  // namespace InspectorStyleInvalidatorInvalidateEvent

#define TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason)              \
  TRACE_EVENT_INSTANT1(                                                    \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
      "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,    \
      "data",                                                              \
      InspectorStyleInvalidatorInvalidateEvent::data(                      \
          (element), (InspectorStyleInvalidatorInvalidateEvent::reason)))

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(                 \
    element, reason, invalidationSet, singleSelectorPart)                  \
  TRACE_EVENT_INSTANT1(                                                    \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
      "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,    \
      "data",                                                              \
      InspectorStyleInvalidatorInvalidateEvent::selectorPart(              \
          (element), (InspectorStyleInvalidatorInvalidateEvent::reason),   \
          (invalidationSet), (singleSelectorPart)))

// From a web developer's perspective: what caused this layout? This is strictly
// for tracing. Blink logic must not depend on these.
namespace LayoutInvalidationReason {
extern const char Unknown[];
extern const char SizeChanged[];
extern const char AncestorMoved[];
extern const char StyleChange[];
extern const char DomChanged[];
extern const char TextChanged[];
extern const char PrintingChanged[];
extern const char AttributeChanged[];
extern const char ColumnsChanged[];
extern const char ChildAnonymousBlockChanged[];
extern const char AnonymousBlockChange[];
extern const char Fullscreen[];
extern const char ChildChanged[];
extern const char ListValueChange[];
extern const char ImageChanged[];
extern const char LineBoxesChanged[];
extern const char SliderValueChanged[];
extern const char AncestorMarginCollapsing[];
extern const char FieldsetChanged[];
extern const char TextAutosizing[];
extern const char SvgResourceInvalidated[];
extern const char FloatDescendantChanged[];
extern const char CountersChanged[];
extern const char GridChanged[];
extern const char MenuOptionsChanged[];
extern const char RemovedFromLayout[];
extern const char AddedToLayout[];
extern const char TableChanged[];
extern const char PaddingChanged[];
extern const char TextControlChanged[];
// FIXME: This is too generic, we should be able to split out transform and
// size related invalidations.
extern const char SvgChanged[];
extern const char ScrollbarChanged[];
}  // namespace LayoutInvalidationReason

// LayoutInvalidationReasonForTracing is strictly for tracing. Blink logic must
// not depend on this value.
typedef const char LayoutInvalidationReasonForTracing[];

namespace InspectorLayoutInvalidationTrackingEvent {
std::unique_ptr<TracedValue> CORE_EXPORT
data(const LayoutObject*, LayoutInvalidationReasonForTracing);
}

namespace InspectorPaintInvalidationTrackingEvent {
std::unique_ptr<TracedValue> data(const LayoutObject*,
                                  const LayoutObject& paintContainer);
}

namespace InspectorScrollInvalidationTrackingEvent {
std::unique_ptr<TracedValue> data(const LayoutObject&);
}

namespace InspectorChangeResourcePriorityEvent {
std::unique_ptr<TracedValue> data(unsigned long identifier,
                                  const ResourceLoadPriority&);
}

namespace InspectorSendRequestEvent {
std::unique_ptr<TracedValue> data(unsigned long identifier,
                                  LocalFrame*,
                                  const ResourceRequest&);
}

namespace InspectorReceiveResponseEvent {
std::unique_ptr<TracedValue> data(unsigned long identifier,
                                  LocalFrame*,
                                  const ResourceResponse&);
}

namespace InspectorReceiveDataEvent {
std::unique_ptr<TracedValue> data(unsigned long identifier,
                                  LocalFrame*,
                                  int encodedDataLength);
}

namespace InspectorResourceFinishEvent {
std::unique_ptr<TracedValue> data(unsigned long identifier,
                                  double finishTime,
                                  bool didFail,
                                  int64_t encodedDataLength,
                                  int64_t decodedBodyLength);
}

namespace InspectorTimerInstallEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*,
                                  int timerId,
                                  int timeout,
                                  bool singleShot);
}

namespace InspectorTimerRemoveEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, int timerId);
}

namespace InspectorTimerFireEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, int timerId);
}

namespace InspectorIdleCallbackRequestEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, int id, double timeout);
}

namespace InspectorIdleCallbackCancelEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, int id);
}

namespace InspectorIdleCallbackFireEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*,
                                  int id,
                                  double allottedMilliseconds,
                                  bool timedOut);
}

namespace InspectorAnimationFrameEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, int callbackId);
}

namespace InspectorParseHtmlEvent {
std::unique_ptr<TracedValue> beginData(Document*, unsigned startLine);
std::unique_ptr<TracedValue> endData(unsigned endLine);
}

namespace InspectorParseAuthorStyleSheetEvent {
std::unique_ptr<TracedValue> data(const CSSStyleSheetResource*);
}

namespace InspectorXhrReadyStateChangeEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorXhrLoadEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorLayerInvalidationTrackingEvent {
extern const char SquashingLayerGeometryWasUpdated[];
extern const char AddedToSquashingLayer[];
extern const char RemovedFromSquashingLayer[];
extern const char ReflectionLayerChanged[];
extern const char NewCompositedLayer[];

std::unique_ptr<TracedValue> data(const PaintLayer*, const char* reason);
}

#define TRACE_LAYER_INVALIDATION(LAYER, REASON)                            \
  TRACE_EVENT_INSTANT1(                                                    \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
      "LayerInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",       \
      InspectorLayerInvalidationTrackingEvent::data((LAYER), (REASON)));

namespace InspectorPaintEvent {
std::unique_ptr<TracedValue> data(LayoutObject*,
                                  const LayoutRect& clipRect,
                                  const GraphicsLayer*);
}

namespace InspectorPaintImageEvent {
std::unique_ptr<TracedValue> data(const LayoutImage&);
std::unique_ptr<TracedValue> data(const LayoutObject&, const StyleImage&);
std::unique_ptr<TracedValue> data(const LayoutObject*,
                                  const ImageResourceContent&);
}

namespace InspectorCommitLoadEvent {
std::unique_ptr<TracedValue> data(LocalFrame*);
}

namespace InspectorMarkLoadEvent {
std::unique_ptr<TracedValue> data(LocalFrame*);
}

namespace InspectorScrollLayerEvent {
std::unique_ptr<TracedValue> data(LayoutObject*);
}

namespace InspectorUpdateLayerTreeEvent {
std::unique_ptr<TracedValue> data(LocalFrame*);
}

namespace InspectorEvaluateScriptEvent {
std::unique_ptr<TracedValue> data(LocalFrame*,
                                  const String& url,
                                  const WTF::TextPosition&);
}

namespace InspectorParseScriptEvent {
std::unique_ptr<TracedValue> data(unsigned long identifier, const String& url);
}

namespace InspectorCompileScriptEvent {
std::unique_ptr<TracedValue> data(const String& url, const WTF::TextPosition&);
}

namespace InspectorFunctionCallEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*,
                                  const v8::Local<v8::Function>&);
}

namespace InspectorUpdateCountersEvent {
std::unique_ptr<TracedValue> data();
}

namespace InspectorInvalidateLayoutEvent {
std::unique_ptr<TracedValue> data(LocalFrame*);
}

namespace InspectorRecalculateStylesEvent {
std::unique_ptr<TracedValue> data(LocalFrame*);
}

namespace InspectorEventDispatchEvent {
std::unique_ptr<TracedValue> data(const Event&);
}

namespace InspectorTimeStampEvent {
std::unique_ptr<TracedValue> data(ExecutionContext*, const String& message);
}

namespace InspectorTracingSessionIdForWorkerEvent {
std::unique_ptr<TracedValue> data(const String& sessionId,
                                  const String& workerId,
                                  WorkerThread*);
}

namespace InspectorTracingStartedInFrame {
std::unique_ptr<TracedValue> data(const String& sessionId, LocalFrame*);
}

namespace InspectorSetLayerTreeId {
std::unique_ptr<TracedValue> data(const String& sessionId, int layerTreeId);
}

namespace InspectorAnimationEvent {
std::unique_ptr<TracedValue> data(const Animation&);
}

namespace InspectorAnimationStateEvent {
std::unique_ptr<TracedValue> data(const Animation&);
}

namespace InspectorHitTestEvent {
std::unique_ptr<TracedValue> endData(const HitTestRequest&,
                                     const HitTestLocation&,
                                     const HitTestResult&);
}

CORE_EXPORT String toHexString(const void* p);
CORE_EXPORT void setCallStack(TracedValue*);

}  // namespace blink

#endif  // !defined(InspectorTraceEvents_h)
