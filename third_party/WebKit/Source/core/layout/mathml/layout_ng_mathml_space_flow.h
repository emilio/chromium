// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMathMLSpaceFlow_h
#define LayoutNGMathMLSpaceFlow_h

#include "core/layout/LayoutBlock.h"
#include "core/layout/mathml/ng_mathml_space_node.h"

namespace blink {

class MathMLSpaceElement;

// TODO(emilio): Is LayoutBlock the right thing to inherit from? Probably ok for
// a quick test.
//
// Also the "Flow" name may very well be misleading.
class LayoutNGMathMLSpaceFlow final : public LayoutBlock {
 public:
  explicit LayoutNGMathMLSpaceFlow(MathMLSpaceElement*);
  ~LayoutNGMathMLSpaceFlow() override = default;
  bool isOfType(LayoutObjectType) const override;
  void layoutBlock(bool relayoutChildren) override;
  const char* name() const override { return "LayoutNGMathMLSpaceFlow"; }

  NGMathMLSpaceNode* toNGLayoutInputNode(const ComputedStyle&) override;
};

}

#endif  // LayoutNGMathMLSpaceFlow_h
