/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MathMLElement_h
#define MathMLElement_h

#include "core/dom/Element.h"
#include "core/CoreExport.h"
#include "core/MathMLNames.h"

namespace blink {

class ComputedStyle;

class CORE_EXPORT MathMLElement : public Element {
  // DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_ELEMENT_FACTORY_WITH_TAGNAME(MathMLElement);

  ~MathMLElement() override;

  bool hasTagName(const MathMLQualifiedName& name) const {
    return hasLocalName(name.localName());
  }

 protected:
  MathMLElement(const QualifiedName& tagName,
                Document& document,
                ConstructionType constructionType = CreateMathMLElement);

 private:
  bool isMathMLElement() const =
      delete;  // This will catch anyone doing an unnecessary check.
};

DEFINE_ELEMENT_TYPE_CASTS(MathMLElement, isMathMLElement());

template <typename T>
bool isElementOfType(const MathMLElement&);
template <>
inline bool isElementOfType<const MathMLElement>(const MathMLElement&) {
  return true;
}

inline bool Node::hasTagName(const MathMLQualifiedName& name) const {
  return isMathMLElement() && toMathMLElement(*this).hasTagName(name);
}

}  // namespace blink

#include "core/MathMLElementTypeHelpers.h"

#endif  // MathMLElement_h
