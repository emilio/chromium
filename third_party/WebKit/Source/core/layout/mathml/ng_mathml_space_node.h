// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGMathMLSpaceNode_h
#define NGMathMLSpaceNode_h

#include "core/layout/mathml/ng_mathml_input_node.h"

namespace blink {

// Let's assume we're a block so our parent can layout us as such, but override
// our layout algorithm so new we layout the children using a simplified
// algorithm for MathML.
//
// FIXME(emilio): This might need more care, but right now LayoutNG only does
// Block and a simplified version of inline layout, so making it quack like a
// block is probably ok.
class NGMathMLSpaceNode final : public NGMathMLInputNode {
 public:
  explicit NGMathMLSpaceNode(LayoutObject* flow) : NGMathMLInputNode(flow) {}

  RefPtr<NGLayoutResult> Layout(NGConstraintSpace* constraint_space,
                                NGBreakToken* break_token) final;
};

}  // namespace blink

#endif
