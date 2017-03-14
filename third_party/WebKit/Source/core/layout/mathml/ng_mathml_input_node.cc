// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ng_mathml_input_node.h"

namespace blink {

RefPtr<NGLayoutResult> NGMathMLInputNode::Layout(
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token) {
  ASSERT_NOT_REACHED();  // Should be subclassed.
  return nullptr;
}

}  // namespace blink
