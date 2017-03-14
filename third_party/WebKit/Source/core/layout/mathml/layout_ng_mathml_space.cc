#include "layout_ng_mathml_space.h"
#include "core/mathml/MathMLSpaceElement.h"

namespace blink {

LayoutNGMathMLSpace::LayoutNGMathMLSpace(MathMLSpaceElement* element)
  : LayoutBlock(element) {
  DCHECK(element);
}

void LayoutNGMathMLSpace::layoutBlock(bool relayoutChildren) {
  ASSERT_NOT_REACHED(); // Should use LayoutNG instead
  clearNeedsLayout();
}

bool LayoutNGMathMLSpace::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathML || LayoutBlock::isOfType(type);
}

NGMathMLSpaceNode* LayoutNGMathMLSpace::toNGLayoutInputNode(
    const ComputedStyle& style) {
  return new NGMathMLSpaceNode(this);
}

}  // namespace blink
