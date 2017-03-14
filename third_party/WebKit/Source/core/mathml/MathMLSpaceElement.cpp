/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "core/mathml/MathMLSpaceElement.h"

#include "core/layout/mathml/layout_ng_mathml_space.h"

namespace blink {

using namespace MathMLNames;

MathMLSpaceElement::MathMLSpaceElement(Document& doc)
    : MathMLElement(MathMLNames::mspaceTag, doc) {}

DEFINE_NODE_FACTORY(MathMLSpaceElement)

void MathMLSpaceElement::parseAttribute(const AttributeModificationParams& param) {
  if (param.name == widthAttr)
    m_width.dirty = true;
  else if (param.name == heightAttr)
    m_height.dirty = true;
  else if (param.name == depthAttr)
    m_depth.dirty = true;

  MathMLElement::parseAttribute(param);
}

/*
 * TODO(emilio): We could consider writing our custom paint code for MathML
 * stuff, and then we'd only need to create LayoutNGNodes.
 *
 * But meanwhile...
 */
LayoutObject* MathMLSpaceElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutNGMathMLSpace(this);
}

}  // namespace blink
