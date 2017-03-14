// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/mathml/layout_ng_mathml_block.h"

#include "core/css/CSSHelper.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space.h"

namespace blink {

LayoutNGMathMLBlock::LayoutNGMathMLBlock(MathMLElement* element)
    : LayoutBlock(element) {
  DCHECK(element);
}

bool LayoutNGMathMLBlock::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathML || type == LayoutObjectMathMLBlock ||
         LayoutBlock::isOfType(type);
}

bool LayoutNGMathMLBlock::isChildAllowed(LayoutObject* child,
                                         const ComputedStyle&) const {
  return child->isMathML();
}

void LayoutNGMathMLBlock::layoutBlock(bool) {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  // XXX NGLayoutInputNode is GC'd, is this right?
  Persistent<NGLayoutInputNode> input = toNGLayoutInputNode(*style());
  RefPtr<NGLayoutResult> result = input->Layout(constraint_space.get(),
                                                /* break_token = */ nullptr);

  clearNeedsLayout();
}

LayoutUnit LayoutNGMathMLBlock::toUserUnits(
    const MathMLElement::Length& length,
    const LayoutUnit& referenceValue) const {
  switch (length.type) {
    case MathMLElement::LengthType::Cm:
      return LayoutUnit(length.value * cssPixelsPerInch / 2.54f);
    case MathMLElement::LengthType::Em:
      return LayoutUnit(length.value * style()->fontSize());
    case MathMLElement::LengthType::Ex:
      return LayoutUnit(
          length.value *
          style()->font().primaryFont()->getFontMetrics().xHeight());
    case MathMLElement::LengthType::In:
      return LayoutUnit(length.value * cssPixelsPerInch);
    case MathMLElement::LengthType::MathUnit:
      return LayoutUnit(length.value * style()->fontSize() / 18);
    case MathMLElement::LengthType::Mm:
      return LayoutUnit(length.value * cssPixelsPerInch / 25.4f);
    case MathMLElement::LengthType::Pc:
      return LayoutUnit(length.value * cssPixelsPerInch / 6);
    case MathMLElement::LengthType::Percentage:
      return LayoutUnit(referenceValue * length.value / 100);
    case MathMLElement::LengthType::Pt:
      return LayoutUnit(length.value * cssPixelsPerInch / 72);
    case MathMLElement::LengthType::Px:
      return LayoutUnit(length.value);
    case MathMLElement::LengthType::UnitLess:
      return LayoutUnit(referenceValue * length.value);
    case MathMLElement::LengthType::ParsingFailed:
      return referenceValue;
  }
}

}  // namespace blink
