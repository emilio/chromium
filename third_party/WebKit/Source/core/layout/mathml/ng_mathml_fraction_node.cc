// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/mathml/ng_mathml_fraction_node.h"

#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"

namespace blink {

RefPtr<NGLayoutResult> NGMathMLFractionNode::Layout(
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token) {

  LayoutUnit default_linethickness =
      hasMathData() ? mathConstant(FontPlatformData::FractionRuleThickness)
      : ruleThicknessFallback();
  LayoutUnit linethickness =
      toUserUnits(m_linethickness, default_linethickness);

  NGBlockNode* numerator = toNGBlockNode(FirstChild());
  NGBlockNode* denominator =
    numerator ? toNGBlockNode(numerator->NextSibling()) : nullptr;

  // Invalid markup: display nothing. We could create an anonymous
  // TextLayoutObject or something, even in createLayoutObject instead of here.
  //
  // Anyway, this matches WebKit's behavior for now.
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox, this);

  if (!numerator || !denominator || denominator->NextSibling()) {
    RefPtr<NGLayoutResult> result =
      builder.SetBlockSize(LayoutUnit())
        .SetInlineSize(LayoutUnit())
        .ToBoxFragment();

    CopyFragmentDataToLayoutBox(*constraint_space, result.get());

    return result;
  }

  // FIXME(emilio): This is kinda boilerplate, right? Just make it shared with
  // other code.
  NGLogicalSize available_size = constraint_space->AvailableSize();
  NGBoxStrut border_padding = ComputeBorders(*constraint_space, Style()) +
                              ComputePadding(*constraint_space, Style());

  NGLogicalSize adjusted_size(
      available_size.inline_size - border_padding.InlineSum(),
      available_size.block_size == NGSizeIndefinite
        ? NGSizeIndefinite : available_size.block_size - border_padding.BlockSum());

  NGConstraintSpaceBuilder child_constraints(constraint_space);
  child_constraints.SetAvailableSize(adjusted_size)
    .SetPercentageResolutionSize(adjusted_size);

  // TODO(emilio): It's not clear what to do if they don't fit in the constraint
  // space...
  RefPtr<NGConstraintSpace> numerator_constraint_space =
      child_constraints.ToConstraintSpace(
          FromPlatformWritingMode(numerator->Style().getWritingMode()));
  RefPtr<NGLayoutResult> numerator_layout_result =
      numerator->Layout(numerator_constraint_space.get());

  RefPtr<NGConstraintSpace> denominator_constraint_space =
      child_constraints.ToConstraintSpace(
          FromPlatformWritingMode(denominator->Style().getWritingMode()));
  RefPtr<NGLayoutResult> denominator_layout_result =
      denominator->Layout(denominator_constraint_space.get());

  NGLogicalOffset numerator_offset;
  NGLogicalOffset denominator_offset;

  NGBoxFragment numerator_fragment(
      FromPlatformWritingMode(numerator->Style().getWritingMode()),
      toNGPhysicalBoxFragment(numerator_layout_result->PhysicalFragment().get()));
  NGBoxFragment denominator_fragment(
      FromPlatformWritingMode(denominator->Style().getWritingMode()),
      toNGPhysicalBoxFragment(denominator_layout_result->PhysicalFragment().get()));

  // Center the numerator and denominator.
  LayoutUnit total_inline_size = std::max(numerator_fragment.InlineSize(),
                                          denominator_fragment.InlineSize());
  numerator_offset.inline_offset =
      (total_inline_size - numerator_fragment.InlineSize()) / 2;
  denominator_offset.inline_offset =
      (total_inline_size - denominator_fragment.InlineSize()) / 2;

  // Read the gaps from the MATH table or use suggested values.
  LayoutUnit min_numerator_gap, min_denominator_gap;
  if (hasMathData()) {
    min_numerator_gap = mathConstant(
        FontPlatformData::FractionNumeratorGapMin);
    min_denominator_gap = mathConstant(
        FontPlatformData::FractionDenominatorGapMin);
  } else {
    min_numerator_gap = ruleThicknessFallback();
    min_denominator_gap = ruleThicknessFallback();
  }

  denominator_offset.block_offset =
    numerator_fragment.BlockSize() + min_numerator_gap + linethickness +
    min_denominator_gap;

  builder.AddChild(std::move(numerator_layout_result), numerator_offset);
  builder.AddChild(std::move(denominator_layout_result), denominator_offset);

  LayoutUnit total_block_size =
    denominator_offset.block_offset + denominator_fragment.BlockSize();

  RefPtr<NGLayoutResult> result =
      builder.SetBlockSize(total_block_size)
             .SetInlineSize(total_inline_size)
             .ToBoxFragment();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
