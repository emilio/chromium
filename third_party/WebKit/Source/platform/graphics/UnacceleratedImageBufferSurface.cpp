/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "platform/graphics/UnacceleratedImageBufferSurface.h"

#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/PassRefPtr.h"

namespace blink {

UnacceleratedImageBufferSurface::UnacceleratedImageBufferSurface(
    const IntSize& size,
    OpacityMode opacityMode,
    ImageInitializationMode initializationMode,
    sk_sp<SkColorSpace> colorSpace,
    SkColorType colorType)
    : ImageBufferSurface(size, opacityMode, colorSpace, colorType) {
  SkAlphaType alphaType =
      (Opaque == opacityMode) ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), colorType,
                                       alphaType, colorSpace);
  SkSurfaceProps disableLCDProps(0, kUnknown_SkPixelGeometry);
  m_surface =
      SkSurface::MakeRaster(info, Opaque == opacityMode ? 0 : &disableLCDProps);

  if (!m_surface)
    return;

  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  m_canvas = WTF::wrapUnique(new PaintCanvas(m_surface->getCanvas()));
  m_canvas->save();

  if (initializationMode == InitializeImagePixels)
    clear();
}

UnacceleratedImageBufferSurface::~UnacceleratedImageBufferSurface() {}

PaintCanvas* UnacceleratedImageBufferSurface::canvas() {
  return m_canvas.get();
}

bool UnacceleratedImageBufferSurface::isValid() const {
  return m_surface;
}

sk_sp<SkImage> UnacceleratedImageBufferSurface::newImageSnapshot(
    AccelerationHint,
    SnapshotReason) {
  return m_surface->makeImageSnapshot();
}

}  // namespace blink
