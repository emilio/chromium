// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMathMLMath_h
#define LayoutNGMathMLMath_h

#include "core/layout/LayoutReplaced.h"
#include "core/mathml/MathMLElement.h"
#include "ng_mathml_math_node.h"
#include "layout_ng_mathml_block.h"

namespace blink {

class MathMLMathElement;

class LayoutNGMathMLMath final : public LayoutNGMathMLBlock {
 public:
  explicit LayoutNGMathMLMath(MathMLMathElement*);
  ~LayoutNGMathMLMath() override = default;

  const char* name() const final { return "LayoutNGMathMLMath"; }

 private:
  NGMathMLMathNode* toNGLayoutInputNode(const ComputedStyle&) override;
  bool isOfType(LayoutObjectType) const override;

  LayoutObjectChildList m_children;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGMathMLMath, isMathMLMath());

}  // namespace blink

#endif  // LayoutNGMathMLMath_h
