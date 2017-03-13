// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ng_mathml_math_node.h"
#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

RefPtr<NGLayoutResult>
NGMathMLMathNode::Layout(NGConstraintSpace* constraint_space,
                         NGBreakToken* break_token) {
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox,
                            this);

  for (NGLayoutInputNode* child = FirstChild(); child;
       child = child->NextSibling()) {
    // FIXME(emilio): Pretty sure I need to subdivide the available space
    // somehow.
    RefPtr<NGLayoutResult> child_fragment =
      child->Layout(constraint_space, break_token);

    // FIXME(emilio): Calculate offset! (presumably easy since we don't have to
    // care about oof-positioned stuff?).
    builder.AddChild(std::move(child_fragment), NGLogicalOffset());
  }

  builder.SetInlineSize(LayoutUnit(40))
    .SetBlockSize(LayoutUnit(40));

  RefPtr<NGLayoutResult> result = builder.ToBoxFragment();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
