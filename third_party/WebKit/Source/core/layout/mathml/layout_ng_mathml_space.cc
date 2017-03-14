// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout_ng_mathml_space.h"

#include "core/mathml/MathMLSpaceElement.h"

namespace blink {

LayoutNGMathMLSpace::LayoutNGMathMLSpace(MathMLSpaceElement* element)
    : LayoutNGMathMLBlock(element) {
  DCHECK(element);
}

void LayoutNGMathMLSpace::layoutBlock(bool relayoutChildren) {
  ASSERT_NOT_REACHED();  // Should use LayoutNG instead
  clearNeedsLayout();
}

bool LayoutNGMathMLSpace::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathML || LayoutNGMathMLBlock::isOfType(type);
}

NGMathMLSpaceNode* LayoutNGMathMLSpace::toNGLayoutInputNode(
    const ComputedStyle& style) {
  return new NGMathMLSpaceNode(this);
}

}  // namespace blink
