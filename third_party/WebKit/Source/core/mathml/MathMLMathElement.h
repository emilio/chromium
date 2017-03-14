/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MathMLMathElement_h
#define MathMLMathElement_h

#include "core/mathml/MathMLElement.h"

namespace blink {

class ComputedStyle;
class Document;

class CORE_EXPORT MathMLMathElement final : public MathMLElement {
 public:
  DECLARE_NODE_FACTORY(MathMLMathElement);
  explicit MathMLMathElement(Document&);

  LayoutObject* createLayoutObject(const ComputedStyle&) override;
};
}

#endif
