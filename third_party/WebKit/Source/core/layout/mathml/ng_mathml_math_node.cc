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
  LayoutUnit adjusted_block_size =
    available_size.block_size == NGSizeIndefinite
      ? available_size.block_size
      : available_size.block_size - border_padding.BlockSum();

  NGLogicalSize available_size_for_children(adjusted_inline_size,
                                            adjusted_block_size);

  child_constraints.SetAvailableSize(available_size_for_children)
      .SetPercentageResolutionSize(available_size_for_children);

  NGLogicalOffset offset(border_padding.inline_start,
                         border_padding.block_start);
  LayoutUnit max_row_inline_size;
  LayoutUnit max_row_block_size;

  for (NGBlockNode* child = toNGBlockNode(FirstChild()); child;
       child = toNGBlockNode(child->NextSibling())) {
    RefPtr<NGConstraintSpace> child_constraint_space =
        child_constraints.ToConstraintSpace(
            FromPlatformWritingMode(child->Style().getWritingMode()));

    // TODO(emilio): Need to understand all the break_token business, seems to
    // be used only for fragmentation so we should be fine.
    RefPtr<NGLayoutResult> result = child->Layout(child_constraint_space.get());

    // Also, we should account for child margins I guess?
    NGBoxFragment fragment(
        constraint_space->WritingMode(),
        toNGPhysicalBoxFragment(result->PhysicalFragment().get()));

    LayoutUnit child_used_block_space = fragment.BlockSize();
    LayoutUnit child_used_inline_space = fragment.InlineSize();

    builder.AddChild(std::move(result), offset);

    if (child_used_inline_space >
        available_size_for_children.inline_size &&
        offset.inline_offset != LayoutUnit()) {
      // Flush the line.
      offset.inline_offset = LayoutUnit();
      offset.block_offset += max_row_block_size;
      available_size_for_children = NGLogicalSize(adjusted_inline_size,
          available_size_for_children.block_size);
      if (available_size_for_children.block_size != NGSizeIndefinite)
        available_size_for_children.block_size -= max_row_block_size;
      max_row_block_size = LayoutUnit();
    } else {
      offset.inline_offset += child_used_inline_space;
      available_size_for_children.inline_size -= child_used_inline_space;
    }

    max_row_block_size = std::max(max_row_block_size, child_used_block_space);
    max_row_inline_size = std::max(max_row_inline_size, offset.inline_offset);

    // Assume all children are block-level, so keep the inline offset to zero
    // while growing the block offset...
    child_constraints.SetAvailableSize(available_size_for_children);
  }

  // When we render as a block, we always take all the available inline space.
  LayoutUnit final_inline_size =
      Style().isDisplayInlineType()
          ? max_row_inline_size + border_padding.inline_end
          : available_size.inline_size;

  RefPtr<NGLayoutResult> result =
      builder.SetInlineSize(final_inline_size)
          .SetBlockSize(offset.block_offset + max_row_block_size + border_padding.block_end)
          .ToBoxFragment();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
