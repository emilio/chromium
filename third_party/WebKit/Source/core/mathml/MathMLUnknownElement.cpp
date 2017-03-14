/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "core/mathml/MathMLUnknownElement.h"

namespace blink {

MathMLUnknownElement::MathMLUnknownElement(const QualifiedName& tagName,
                                           Document& document)
    : MathMLElement(tagName, document) {}

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(MathMLUnknownElement);

}  // namespace blink
