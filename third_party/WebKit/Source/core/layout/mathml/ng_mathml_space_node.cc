// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/mathml/ng_mathml_space_node.h"

#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

RefPtr<NGLayoutResult> NGMathMLSpaceNode::Layout(
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token) {
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox, this);

  // FIXME: We should use height & depth to set NGFragment's alignmentBaseline.
  LayoutUnit inlineSize = toUserUnits(m_height, LayoutUnit(0)) +
                          toUserUnits(m_depth, LayoutUnit(0));

  RefPtr<NGLayoutResult> result =
      builder.SetBlockSize(toUserUnits(m_width, LayoutUnit(0)))
          .SetInlineSize(inlineSize)
          .ToBoxFragment();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
