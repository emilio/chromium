// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ng_mathml_input_node.h"

#include "core/css/CSSHelper.h"

namespace blink {

RefPtr<NGLayoutResult> NGMathMLInputNode::Layout(
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token) {
  ASSERT_NOT_REACHED();  // Should be subclassed.
  return nullptr;
}

LayoutUnit NGMathMLInputNode::toUserUnits(
    const MathMLElement::Length& length,
    const LayoutUnit& referenceValue) const {
  switch (length.type) {
    case MathMLElement::LengthType::Cm:
      return LayoutUnit(length.value * cssPixelsPerInch / 2.54f);
    case MathMLElement::LengthType::Em:
      return LayoutUnit(length.value * Style().fontSize());
    case MathMLElement::LengthType::Ex:
      return LayoutUnit(
          length.value *
          Style().font().primaryFont()->getFontMetrics().xHeight());
    case MathMLElement::LengthType::In:
      return LayoutUnit(length.value * cssPixelsPerInch);
    case MathMLElement::LengthType::MathUnit:
      return LayoutUnit(length.value * Style().fontSize() / 18);
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
