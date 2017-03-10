#include "layout_ng_mathml_flow.h"

namespace blink {

LayoutNGMathMLFlow::LayoutNGMathMLFlow(MathMLElement* element)
  : LayoutReplaced(element) {
  DCHECK(element);
}

bool LayoutNGMathMLFlow::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathMLRoot || type == LayoutObjectMathML ||
         LayoutReplaced::isOfType(type);
}

NGMathMLRootNode* LayoutNGMathMLFlow::toNGLayoutInputNode(
    const ComputedStyle& style) {
  return new NGMathMLRootNode(this);

}

void LayoutNGMathMLFlow::computeIntrinsicSizingInfo(
    IntrinsicSizingInfo& info) const {
  // TODO(emilio).
}

}  // namespace blink
