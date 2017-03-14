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

  RefPtr<NGLayoutResult> result = builder.SetBlockSize(LayoutUnit(20))
                                      .SetInlineSize(LayoutUnit(20))
                                      .ToBoxFragment();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
