// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/mathml/ng_mathml_fraction_node.h"

#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

RefPtr<NGLayoutResult> NGMathMLFractionNode::Layout(
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token) {

  LayoutUnit default_linethickness =
      hasMathData() ? mathConstant(FontPlatformData::FractionRuleThickness)
      : ruleThicknessFallback();
  LayoutUnit linethickness =
      toUserUnits(m_linethickness, default_linethickness);

  // TODO: Do the actual fraction layout.
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox, this);
  NGConstraintSpaceBuilder child_constraints(constraint_space);
  for (NGBlockNode* child = toNGBlockNode(FirstChild()); child;
       child = toNGBlockNode(child->NextSibling())) {
    RefPtr<NGConstraintSpace> child_constraint_space =
        child_constraints.ToConstraintSpace(
            FromPlatformWritingMode(child->Style().getWritingMode()));
    RefPtr<NGLayoutResult> result =
        child->Layout(child_constraint_space.get());
    builder.AddChild(std::move(result), NGLogicalOffset());
  }

  RefPtr<NGLayoutResult> result =
      builder.SetBlockSize(linethickness)
             .SetInlineSize(LayoutUnit(100))
             .ToBoxFragment();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
