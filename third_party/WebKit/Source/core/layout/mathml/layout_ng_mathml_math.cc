#include "core/layout/mathml/layout_ng_mathml_math.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/mathml/MathMLMathElement.h"

namespace blink {

LayoutNGMathMLMath::LayoutNGMathMLMath(MathMLMathElement* element)
    : LayoutNGMathMLBlock(element) {
  DCHECK(element);
}

bool LayoutNGMathMLMath::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathMLMath || LayoutNGMathMLBlock::isOfType(type);
}

NGMathMLMathNode* LayoutNGMathMLMath::toNGLayoutInputNode(
    const ComputedStyle& style) {
  return new NGMathMLMathNode(this);
}

}  // namespace blink
