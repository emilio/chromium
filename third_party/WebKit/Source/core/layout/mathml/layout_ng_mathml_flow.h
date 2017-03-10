// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMathMLFlow_h
#define LayoutNGMathMLFlow_h

#include "core/layout/LayoutReplaced.h"
#include "core/mathml/MathMLElement.h"
#include "ng_mathml_root_node.h"

namespace blink {

class MathMLElement;

class LayoutNGMathMLFlow final : public LayoutReplaced {
 public:
  explicit LayoutNGMathMLFlow(MathMLElement*);
  ~LayoutNGMathMLFlow() override = default;

  const char* name() const final {
    return "LayoutNGMathMLFlow";
  }

  void computeIntrinsicSizingInfo(IntrinsicSizingInfo&) const override;

  NGMathMLRootNode* toNGLayoutInputNode(const ComputedStyle&) override;

 private:
  bool isOfType(LayoutObjectType) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGMathMLFlow, isMathMLRoot());

}  // namespace blink

#endif  // LayoutNGMathMLFlow_h
