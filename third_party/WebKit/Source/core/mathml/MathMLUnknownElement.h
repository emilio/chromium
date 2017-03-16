/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MathMLUnknownElement_h
#define MathMLUnknownElement_h

#include "core/mathml/MathMLElement.h"

namespace blink {

class MathMLUnknownElement final : public MathMLElement {
 public:
  DECLARE_ELEMENT_FACTORY_WITH_TAGNAME(MathMLUnknownElement);

 private:
  MathMLUnknownElement(const QualifiedName&, Document&);

  bool layoutObjectIsNeeded(const ComputedStyle&) override { return false; }
};

}  // namespace blink

#endif  // MathMLUnknownElement_h
