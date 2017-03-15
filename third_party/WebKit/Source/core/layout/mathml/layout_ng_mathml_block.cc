// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/mathml/layout_ng_mathml_block.h"

#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space.h"

namespace blink {

LayoutNGMathMLBlock::LayoutNGMathMLBlock(MathMLElement* element)
    : LayoutBlock(element) {
  DCHECK(element);
}

bool LayoutNGMathMLBlock::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathML || type == LayoutObjectMathMLBlock ||
         LayoutBlock::isOfType(type);
}

bool LayoutNGMathMLBlock::isChildAllowed(LayoutObject* child,
                                         const ComputedStyle&) const {
  return child->isMathML();
}

void LayoutNGMathMLBlock::layoutBlock(bool) {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  // XXX NGLayoutInputNode is GC'd, is this right?
  Persistent<NGLayoutInputNode> input = toNGLayoutInputNode(*style());
  RefPtr<NGLayoutResult> result = input->Layout(constraint_space.get(),
                                                /* break_token = */ nullptr);

  clearNeedsLayout();
}

}  // namespace blink
