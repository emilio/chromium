#include "layout_ng_mathml_flow.h"

namespace blink {

LayoutNGMathMLFlow::LayoutNGMathMLFlow(MathMLElement* element)
  : LayoutReplaced(element) {
  DCHECK(element);
}

bool LayoutNGMathMLFlow::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathMLMath || type == LayoutObjectMathML ||
         LayoutReplaced::isOfType(type);
}

NGMathMLMathNode* LayoutNGMathMLFlow::toNGLayoutInputNode(
    const ComputedStyle& style) {
  return new NGMathMLMathNode(this);

}

void LayoutNGMathMLFlow::computeIntrinsicSizingInfo(
    IntrinsicSizingInfo& info) const {
  // TODO(emilio).
}

}  // namespace blink
