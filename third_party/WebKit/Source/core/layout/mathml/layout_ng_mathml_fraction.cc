// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout_ng_mathml_fraction.h"

#include "core/mathml/MathMLFractionElement.h"

namespace blink {

LayoutNGMathMLFraction::LayoutNGMathMLFraction(MathMLFractionElement* element)
    : LayoutNGMathMLBlock(element) {
  DCHECK(element);
}

void LayoutNGMathMLFraction::layoutBlock(bool relayoutChildren) {
  ASSERT_NOT_REACHED();  // Should use LayoutNG instead
  clearNeedsLayout();
}

NGMathMLFractionNode* LayoutNGMathMLFraction::toNGLayoutInputNode(
    const ComputedStyle& style) {
  MathMLFractionElement* fractionElement = toMathMLFractionElement(node());
  return new NGMathMLFractionNode(this, fractionElement->linethickness());
}

}  // namespace blink
