/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "core/mathml/MathMLMathElement.h"
#include "core/layout/mathml/layout_ng_mathml_math.h"

namespace blink {

MathMLMathElement::MathMLMathElement(Document& doc)
  : MathMLElement(MathMLNames::mathTag, doc) {
}

DEFINE_NODE_FACTORY(MathMLMathElement)

LayoutObject* MathMLMathElement::createLayoutObject(const ComputedStyle& style) {
  return new LayoutNGMathMLMath(this);
}

}  // namespace blink
