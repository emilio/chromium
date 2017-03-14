/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MathMLMSpaceElement_h
#define MathMLMSpaceElement_h

#include "core/MathMLNames.h"
#include "core/mathml/MathMLElement.h"

namespace blink {

class LayoutObject;
class ComputedStyle;

class MathMLSpaceElement final : public MathMLElement {
 public:
  DECLARE_NODE_FACTORY(MathMLSpaceElement);

  const Length& width() {
    return cachedMathMLLength(MathMLNames::widthAttr, m_width);
  }
  const Length& height() {
    return cachedMathMLLength(MathMLNames::heightAttr, m_height);
  }
  const Length& depth() {
    return cachedMathMLLength(MathMLNames::depthAttr, m_depth);
  }

 protected:
  explicit MathMLSpaceElement(Document&);

 private:
  LayoutObject* createLayoutObject(const ComputedStyle&) override;
  virtual void parseAttribute(const AttributeModificationParams&);

  Length m_width;
  Length m_height;
  Length m_depth;
};

}  // namespace blink

#endif  // MathMLMSpaceElement
