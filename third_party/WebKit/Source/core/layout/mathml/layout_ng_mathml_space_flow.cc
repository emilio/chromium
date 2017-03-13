#include "layout_ng_mathml_space_flow.h"
#include "core/mathml/MathMLSpaceElement.h"

namespace blink {

LayoutNGMathMLSpaceFlow::LayoutNGMathMLSpaceFlow(MathMLSpaceElement* element)
  : LayoutBlock(element) {
  DCHECK(element);
}

void LayoutNGMathMLSpaceFlow::layoutBlock(bool relayoutChildren) {
  ASSERT_NOT_REACHED(); // Should use LayoutNG instead
  clearNeedsLayout();
}

bool LayoutNGMathMLSpaceFlow::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathML || LayoutBlock::isOfType(type);
}

NGMathMLSpaceNode* LayoutNGMathMLSpaceFlow::toNGLayoutInputNode(
    const ComputedStyle& style) {
  return new NGMathMLSpaceNode(this);
}

}  // namespace blink
