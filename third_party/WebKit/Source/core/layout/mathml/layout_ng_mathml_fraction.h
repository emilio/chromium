// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMathMLFraction_h
#define LayoutNGMathMLFraction_h

#include "core/layout/mathml/ng_mathml_fraction_node.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/mathml/layout_ng_mathml_block.h"

namespace blink {

class MathMLFractionElement;

class LayoutNGMathMLFraction final : public LayoutNGMathMLBlock {
 public:
  explicit LayoutNGMathMLFraction(MathMLFractionElement*);
  ~LayoutNGMathMLFraction() override = default;
  void layoutBlock(bool relayoutChildren) override;
  const char* name() const override { return "LayoutNGMathMLFraction"; }

  NGMathMLFractionNode* toNGLayoutInputNode(const ComputedStyle&) override;
};
}

#endif  // LayoutNGMathMLFraction_h
