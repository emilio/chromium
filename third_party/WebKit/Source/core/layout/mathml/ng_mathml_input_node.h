// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGMathMLInputNode_h
#define NGMathMLInputNode_h

#include "core/layout/ng/ng_block_node.h"

#include "core/mathml/MathMLElement.h"

namespace blink {

class NGMathMLInputNode : public NGBlockNode {
 public:
  explicit NGMathMLInputNode(LayoutObject* flow) : NGBlockNode(flow) {}

  RefPtr<NGLayoutResult> Layout(NGConstraintSpace* constraint_space,
                                NGBreakToken* break_token) override;

 protected:
  LayoutUnit toUserUnits(const MathMLElement::Length&,
                         const LayoutUnit& referenceValue) const;
  LayoutUnit ruleThicknessFallback() const {
    // This function returns a value for the default rule thickness (TeX's
    // \xi_8) to be used as a fallback when we lack a MATH table.
    return LayoutUnit(0.05f * Style().fontSize());
  }
  bool hasMathData() const {
    return Style().font().primaryFont()->platformData().hasMathData();
  }
  LayoutUnit mathConstant(FontPlatformData::MathConstant constant) const {
    return LayoutUnit(
        Style().font().primaryFont()->platformData().mathConstant(constant) /
        65536.0);
  }
  LayoutUnit mathAxisHeight() const {
    return hasMathData()
               ? mathConstant(FontPlatformData::AxisHeight)
               : LayoutUnit(
                     Style().font().primaryFont()->getFontMetrics().xHeight() /
                     2);
  }
};

}  // namespace blink

#endif
