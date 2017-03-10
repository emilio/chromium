/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MathMLMSpaceElement_h
#define MathMLMSpaceElement_h

#include "core/mathml/MathMLElement.h"

namespace blink {

class LayoutObject;
class ComputedStyle;

class MathMLMSpaceElement final : public MathMLElement {
public:
  LayoutObject* createLayoutObject(const ComputedStyle&) override;
};

}  // namespace blink

#endif  // MathMLMSpaceElement
