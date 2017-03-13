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
