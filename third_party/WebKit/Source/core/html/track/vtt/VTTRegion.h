/*
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VTTRegion_h
#define VTTRegion_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/Timer.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class ExceptionState;
class HTMLDivElement;
class VTTCueBox;
class VTTScanner;

class VTTRegion final : public GarbageCollectedFinalized<VTTRegion>,
                        public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VTTRegion* create() { return new VTTRegion; }

  virtual ~VTTRegion();

  const String& id() const { return m_id; }
  void setId(const String&);

  double width() const { return m_width; }
  void setWidth(double, ExceptionState&);

  int lines() const { return m_lines; }
  void setLines(int, ExceptionState&);

  double regionAnchorX() const { return m_regionAnchor.x(); }
  void setRegionAnchorX(double, ExceptionState&);

  double regionAnchorY() const { return m_regionAnchor.y(); }
  void setRegionAnchorY(double, ExceptionState&);

  double viewportAnchorX() const { return m_viewportAnchor.x(); }
  void setViewportAnchorX(double, ExceptionState&);

  double viewportAnchorY() const { return m_viewportAnchor.y(); }
  void setViewportAnchorY(double, ExceptionState&);

  const AtomicString scroll() const;
  void setScroll(const AtomicString&);

  void setRegionSettings(const String&);

  bool isScrollingRegion() { return m_scroll; }

  HTMLDivElement* getDisplayTree(Document&);

  void appendVTTCueBox(VTTCueBox*);
  void displayLastVTTCueBox();
  void willRemoveVTTCueBox(VTTCueBox*);

  DECLARE_TRACE();

 private:
  VTTRegion();

  void prepareRegionDisplayTree();

  // The timer is needed to continue processing when cue scrolling ended.
  void startTimer();
  void stopTimer();
  void scrollTimerFired(TimerBase*);

  enum RegionSetting {
    None,
    Id,
    Width,
    Height,
    RegionAnchor,
    ViewportAnchor,
    Scroll
  };
  RegionSetting scanSettingName(VTTScanner&);
  void parseSettingValue(RegionSetting, VTTScanner&);

  static const AtomicString& textTrackCueContainerScrollingClass();

  String m_id;
  double m_width;
  unsigned m_lines;
  FloatPoint m_regionAnchor;
  FloatPoint m_viewportAnchor;
  bool m_scroll;

  // The cue container is the container that is scrolled up to obtain the
  // effect of scrolling cues when this is enabled for the regions.
  Member<HTMLDivElement> m_cueContainer;
  Member<HTMLDivElement> m_regionDisplayTree;

  // Keep track of the current numeric value of the css "top" property.
  double m_currentTop;

  // The timer is used to display the next cue line after the current one has
  // been displayed. It's main use is for scrolling regions and it triggers as
  // soon as the animation for rolling out one line has finished, but
  // currently it is used also for non-scrolling regions to use a single
  // code path.
  TaskRunnerTimer<VTTRegion> m_scrollTimer;
};

}  // namespace blink

#endif  // VTTRegion_h
