/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "core/mathml/MathMLElement.h"

namespace blink {

MathMLElement::MathMLElement(const QualifiedName& tagName,
                             Document& document,
                             ConstructionType constructionType)
    : Element(tagName, &document, constructionType) {}

MathMLElement::~MathMLElement() {}

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(MathMLElement)

} // namespace blink
