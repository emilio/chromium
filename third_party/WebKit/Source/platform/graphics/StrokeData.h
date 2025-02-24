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

#ifndef StrokeData_h
#define StrokeData_h

#include "platform/PlatformExport.h"
#include "platform/graphics/DashArray.h"
#include "platform/graphics/Gradient.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/Pattern.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPathEffect.h"
#include "wtf/Allocator.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

// Encapsulates stroke geometry information.
// It is pulled out of GraphicsContextState to enable other methods to use it.
class PLATFORM_EXPORT StrokeData final {
  DISALLOW_NEW();

 public:
  StrokeData()
      : m_style(SolidStroke),
        m_thickness(0),
        m_lineCap(PaintFlags::kDefault_Cap),
        m_lineJoin(PaintFlags::kDefault_Join),
        m_miterLimit(4) {}

  StrokeStyle style() const { return m_style; }
  void setStyle(StrokeStyle style) { m_style = style; }

  float thickness() const { return m_thickness; }
  void setThickness(float thickness) { m_thickness = thickness; }

  void setLineCap(LineCap cap) { m_lineCap = (PaintFlags::Cap)cap; }

  void setLineJoin(LineJoin join) { m_lineJoin = (PaintFlags::Join)join; }

  float miterLimit() const { return m_miterLimit; }
  void setMiterLimit(float miterLimit) { m_miterLimit = miterLimit; }

  void setLineDash(const DashArray&, float);

  // Sets everything on the paint except the pattern, gradient and color.
  // If a non-zero length is provided, the number of dashes/dots on a
  // dashed/dotted line will be adjusted to start and end that length with a
  // dash/dot.
  void setupPaint(PaintFlags*, int length = 0) const;

  // Setup any DashPathEffect on the paint. If a non-zero length is provided,
  // and no line dash has been set, the number of dashes/dots on a dashed/dotted
  // line will be adjusted to start and end that length with a dash/dot.
  void setupPaintDashPathEffect(PaintFlags*, int) const;

  // Determine whether a stroked line should be drawn using dashes. In practice,
  // we draw dashes when a dashed stroke is specified or when a dotted stroke
  // is specified but the line width is too small to draw circles.
  static bool strokeIsDashed(float width, StrokeStyle);

 private:
  StrokeStyle m_style;
  float m_thickness;
  PaintFlags::Cap m_lineCap;
  PaintFlags::Join m_lineJoin;
  float m_miterLimit;
  sk_sp<SkPathEffect> m_dash;
};

}  // namespace blink

#endif  // StrokeData_h
