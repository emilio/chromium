/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MathMLFractionElement_h
#define MathMLFractionElement_h

#include "core/MathMLNames.h"
#include "core/mathml/MathMLElement.h"

namespace blink {

class LayoutObject;
class ComputedStyle;

class MathMLFractionElement final : public MathMLElement {
 public:
  DECLARE_NODE_FACTORY(MathMLFractionElement);

  const Length& linethickness();

 protected:
  explicit MathMLFractionElement(Document&);

 private:
  LayoutObject* createLayoutObject(const ComputedStyle&) override;
  virtual void parseAttribute(const AttributeModificationParams&);

  Length m_linethickness;
};

}  // namespace blink

#endif  // MathMLFractionElement
