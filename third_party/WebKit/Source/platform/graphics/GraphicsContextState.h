// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GraphicsContextState_h
#define GraphicsContextState_h

#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/StrokeData.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// Encapsulates the state information we store for each pushed graphics state.
// Only GraphicsContext can use this class.
class PLATFORM_EXPORT GraphicsContextState final {
  USING_FAST_MALLOC(GraphicsContextState);

 public:
  static std::unique_ptr<GraphicsContextState> create() {
    return WTF::wrapUnique(new GraphicsContextState());
  }

  static std::unique_ptr<GraphicsContextState> createAndCopy(
      const GraphicsContextState& other) {
    return WTF::wrapUnique(new GraphicsContextState(other));
  }

  void copy(const GraphicsContextState&);

  // PaintFlags objects that reflect the current state. If the length of the
  // path to be stroked is known, pass it in for correct dash or dot placement.
  const PaintFlags& strokeFlags(int strokedPathLength = 0) const;
  const PaintFlags& fillFlags() const { return m_fillFlags; }

  uint16_t saveCount() const { return m_saveCount; }
  void incrementSaveCount() { ++m_saveCount; }
  void decrementSaveCount() { --m_saveCount; }

  // Stroke data
  Color strokeColor() const { return m_strokeFlags.getColor(); }
  void setStrokeColor(const Color&);

  const StrokeData& getStrokeData() const { return m_strokeData; }
  void setStrokeStyle(StrokeStyle);
  void setStrokeThickness(float);
  void setLineCap(LineCap);
  void setLineJoin(LineJoin);
  void setMiterLimit(float);
  void setLineDash(const DashArray&, float);

  // Fill data
  Color fillColor() const { return m_fillFlags.getColor(); }
  void setFillColor(const Color&);

  // Shadow. (This will need tweaking if we use draw loopers for other things.)
  SkDrawLooper* drawLooper() const {
    DCHECK_EQ(m_fillFlags.getLooper(), m_strokeFlags.getLooper());
    return m_fillFlags.getLooper();
  }
  void setDrawLooper(sk_sp<SkDrawLooper>);

  // Text. (See TextModeFill & friends.)
  TextDrawingModeFlags textDrawingMode() const { return m_textDrawingMode; }
  void setTextDrawingMode(TextDrawingModeFlags mode) {
    m_textDrawingMode = mode;
  }

  SkColorFilter* getColorFilter() const {
    DCHECK_EQ(m_fillFlags.getColorFilter(), m_strokeFlags.getColorFilter());
    return m_fillFlags.getColorFilter();
  }
  void setColorFilter(sk_sp<SkColorFilter>);

  // Image interpolation control.
  InterpolationQuality getInterpolationQuality() const {
    return m_interpolationQuality;
  }
  void setInterpolationQuality(InterpolationQuality);

  bool shouldAntialias() const { return m_shouldAntialias; }
  void setShouldAntialias(bool);

 private:
  GraphicsContextState();
  explicit GraphicsContextState(const GraphicsContextState&);
  GraphicsContextState& operator=(const GraphicsContextState&);

  // This is mutable to enable dash path effect updates when the paint is
  // fetched for use.
  mutable PaintFlags m_strokeFlags;
  PaintFlags m_fillFlags;

  StrokeData m_strokeData;

  TextDrawingModeFlags m_textDrawingMode;

  InterpolationQuality m_interpolationQuality;

  uint16_t m_saveCount;

  bool m_shouldAntialias : 1;
};

}  // namespace blink

#endif  // GraphicsContextState_h
