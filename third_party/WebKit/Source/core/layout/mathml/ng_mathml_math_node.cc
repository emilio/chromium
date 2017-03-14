// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ng_mathml_math_node.h"

#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

RefPtr<NGLayoutResult> NGMathMLMathNode::Layout(
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token) {
  RefPtr<NGLayoutResult> result =
      NGBlockLayoutAlgorithm(this, constraint_space,
                             toNGBlockBreakToken(break_token))
          .Layout();

  CopyFragmentDataToLayoutBox(*constraint_space, result.get());

  return result;
}

}  // namespace blink
