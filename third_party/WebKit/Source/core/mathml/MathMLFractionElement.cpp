/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "core/mathml/MathMLFractionElement.h"

#include "core/layout/mathml/layout_ng_mathml_fraction.h"

namespace blink {

using namespace MathMLNames;

MathMLFractionElement::MathMLFractionElement(Document& doc)
    : MathMLElement(MathMLNames::mfracTag, doc) {}

DEFINE_NODE_FACTORY(MathMLFractionElement)

void MathMLFractionElement::parseAttribute(
    const AttributeModificationParams& param) {
  if (param.name == linethicknessAttr)
    m_linethickness.dirty = true;

  MathMLElement::parseAttribute(param);
}

const MathMLElement::Length& MathMLFractionElement::linethickness() {
  if (m_linethickness.dirty) {
    // The MathML specification does not provide accurate values for "thin",
    // "medium" or "thick".
    // We use the suggestions from the MathML in HTML5 implementation note.
    const AtomicString& thickness = fastGetAttribute(linethicknessAttr);
    if (thickness == "thin") {
      m_linethickness.type = LengthType::UnitLess;
      m_linethickness.value = .5;
    } else if (thickness == "medium") {
      m_linethickness.type = LengthType::UnitLess;

      m_linethickness.value = 1;
    } else if (thickness == "thick") {
      m_linethickness.type = LengthType::UnitLess;
      m_linethickness.value = 2;
    } else
      m_linethickness = parseMathMLLength(thickness);
    m_linethickness.dirty = false;
  }
  return m_linethickness;
}

/*
 * TODO(emilio): We could consider writing our custom paint code for MathML
 * stuff, and then we'd only need to create LayoutNGNodes.
 *
 * But meanwhile...
 */
LayoutObject* MathMLFractionElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutNGMathMLFraction(this);
}

}  // namespace blink
