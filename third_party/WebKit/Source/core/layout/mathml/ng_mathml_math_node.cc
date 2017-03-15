// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ng_mathml_math_node.h"

#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"

namespace blink {

RefPtr<NGLayoutResult> NGMathMLMathNode::Layout(
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token) {
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox, this);
  NGConstraintSpaceBuilder child_constraints(constraint_space);

  NGLogicalSize available_size = constraint_space->AvailableSize();
  NGBoxStrut border_padding = ComputeBorders(*constraint_space, Style()) +
                              ComputePadding(*constraint_space, Style());

  LayoutUnit adjusted_inline_size =
      available_size.inline_size - border_padding.InlineSum();

  LayoutUnit block_size =
      ComputeBlockSizeForFragment(*constraint_space, Style(), NGSizeIndefinite);
  LayoutUnit adjusted_block_size = block_size == NGSizeIndefinite
                                       ? block_size
                                       : block_size - border_padding.BlockSum();

  NGLogicalSize available_size_for_children(adjusted_inline_size,
                                            adjusted_block_size);

  child_constraints.SetAvailableSize(available_size_for_children)
      .SetPercentageResolutionSize(available_size_for_children);

  NGLogicalOffset offset(border_padding.inline_start,
                         border_padding.block_start);
  LayoutUnit max_child_inline_size;
  for (NGBlockNode* child = toNGBlockNode(FirstChild()); child;
       child = toNGBlockNode(child->NextSibling())) {
    RefPtr<NGConstraintSpace> child_constraint_space =
        child_constraints.ToConstraintSpace(
            FromPlatformWritingMode(child->Style().getWritingMode()));

    // TODO(emilio): Need to understand all the break_token business, seems to
    // be used only for fragmentation so we should be fine.
    RefPtr<NGLayoutResult> result = child->Layout(child_constraint_space.get());

    // TODO(emilio): does this handle writing-mode properly? I'd say nope.
    //
    // Also, we should account for child margins I guess?
    NGBoxFragment fragment(
        constraint_space->WritingMode(),
        toNGPhysicalBoxFragment(result->PhysicalFragment().get()));

    LayoutUnit child_used_block_space = fragment.BlockSize();
    max_child_inline_size =
        std::max(max_child_inline_size, fragment.InlineSize());

    builder.AddChild(std::move(result), offset);
    offset.block_offset += child_used_block_space;

    // Assume all children are block-level, so keep the inline offset to zero
    // while growing the block offset...
    if (available_size_for_children.block_size != NGSizeIndefinite)
      available_size_for_children.block_size -= child_used_block_space;
    child_constraints.SetAvailableSize(available_size_for_children);
  }

  // When we render as a block, we always take all the available inline space.
  LayoutUnit final_inline_size =
      Style().isDisplayInlineType()
          ? max_child_inline_size + border_padding.InlineSum()
          : available_size.inline_size;

  RefPtr<NGLayoutResult> result =
      builder.SetInlineSize(final_inline_size)
          .SetBlockSize(offset.block_offset + border_padding.block_end)
          .ToBoxFragment();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
