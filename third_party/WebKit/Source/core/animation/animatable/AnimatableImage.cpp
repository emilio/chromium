/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/animation/animatable/AnimatableImage.h"

#include "core/css/CSSImageValue.h"
#include "core/style/StyleGeneratedImage.h"
#include "wtf/MathExtras.h"

namespace blink {

bool AnimatableImage::usesDefaultInterpolationWith(
    const AnimatableValue* value) const {
  if (!m_value->isImageValue())
    return true;
  if (!toAnimatableImage(value)->toCSSValue()->isImageValue())
    return true;
  return false;
}

PassRefPtr<AnimatableValue> AnimatableImage::interpolateTo(
    const AnimatableValue* value,
    double fraction) const {
  if (fraction <= 0 || fraction >= 1 || usesDefaultInterpolationWith(value))
    return defaultInterpolateTo(this, value, fraction);

  CSSValue* fromValue = toCSSValue();
  CSSValue* toValue = toAnimatableImage(value)->toCSSValue();

  return create(CSSCrossfadeValue::create(
      fromValue, toValue, CSSPrimitiveValue::create(
                              fraction, CSSPrimitiveValue::UnitType::Number)));
}

bool AnimatableImage::equalTo(const AnimatableValue* value) const {
  return dataEquivalent(m_value, toAnimatableImage(value)->m_value);
}

}  // namespace blink
