// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGMathMLInputNode_h
#define NGMathMLInputNode_h

#include "core/layout/ng/ng_block_node.h"

namespace blink {

class NGMathMLInputNode : public NGBlockNode {
 public:
  explicit NGMathMLInputNode(LayoutObject* flow) : NGBlockNode(flow) {}

  RefPtr<NGLayoutResult> Layout(NGConstraintSpace* constraint_space,
                                NGBreakToken* break_token) override;
};

}  // namespace blink

#endif
