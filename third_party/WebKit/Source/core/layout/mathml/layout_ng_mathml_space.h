// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMathMLSpace_h
#define LayoutNGMathMLSpace_h

#include "core/layout/mathml/ng_mathml_space_node.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/mathml/layout_ng_mathml_block.h"

namespace blink {

class MathMLSpaceElement;

class LayoutNGMathMLSpace final : public LayoutNGMathMLBlock {
 public:
  explicit LayoutNGMathMLSpace(MathMLSpaceElement*);
  ~LayoutNGMathMLSpace() override = default;
  void layoutBlock(bool relayoutChildren) override;
  const char* name() const override { return "LayoutNGMathMLSpace"; }

  NGMathMLSpaceNode* toNGLayoutInputNode(const ComputedStyle&) override;
};
}

#endif  // LayoutNGMathMLSpace_h
