// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMathMLBlock_h
#define LayoutNGMathMLBlock_h

#include "core/layout/LayoutBlockFlow.h"
#include "core/mathml/MathMLElement.h"

namespace blink {

class MathMLMathElement;
class NGBlockNode;

class LayoutNGMathMLBlock : public LayoutBlockFlow {
 public:
  explicit LayoutNGMathMLBlock(MathMLElement*);
  ~LayoutNGMathMLBlock() override = default;

  const char* name() const override {
    ASSERT_NOT_REACHED();  // Should be subclassed.
    return "LayoutNGMathMLBlock";
  }

  bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  bool isOfType(LayoutObjectType) const override;

 private:
  void layoutBlock(bool) override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGMathMLBlock, isMathMLBlock());

}  // namespace blink

#endif  // LayoutNGMathMLMath_h
