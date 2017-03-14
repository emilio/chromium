// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMathMLMath_h
#define LayoutNGMathMLMath_h

#include "core/layout/LayoutReplaced.h"
#include "core/mathml/MathMLElement.h"
#include "ng_mathml_math_node.h"

namespace blink {

class MathMLMathElement;

class LayoutNGMathMLMath final : public LayoutReplaced {
 public:
  explicit LayoutNGMathMLMath(MathMLMathElement*);
  ~LayoutNGMathMLMath() override = default;

  const char* name() const final { return "LayoutNGMathMLMath"; }

 private:
  NGMathMLMathNode* toNGLayoutInputNode(const ComputedStyle&) override;
  void computeIntrinsicSizingInfo(IntrinsicSizingInfo&) const override;
  void layout() override;
  void paintReplaced(const PaintInfo&, const LayoutPoint&) const override;


  bool canHaveChildren() const override { return true; }
  const LayoutObjectChildList* children() const { return &m_children; }
  LayoutObjectChildList* children() { return &m_children; }
  LayoutObjectChildList* virtualChildren() override { return children(); }
  const LayoutObjectChildList* virtualChildren() const override { return children(); }
  bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  bool isOfType(LayoutObjectType) const override;

  LayoutObjectChildList m_children;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGMathMLMath, isMathMLMath());

}  // namespace blink

#endif  // LayoutNGMathMLMath_h
