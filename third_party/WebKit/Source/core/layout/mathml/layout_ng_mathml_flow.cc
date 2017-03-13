#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/mathml/layout_ng_mathml_flow.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/mathml/MathMLMathElement.h"

namespace blink {

LayoutNGMathMLFlow::LayoutNGMathMLFlow(MathMLMathElement* element)
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

bool LayoutNGMathMLFlow::isChildAllowed(LayoutObject* child,
                                        const ComputedStyle&) const {
  return child->isMathML();
}

void LayoutNGMathMLFlow::layout() {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  // XXX NGLayoutInputNode is GC'd, is this right?
  Persistent<NGLayoutInputNode> input = toNGLayoutInputNode(*style());
  RefPtr<NGLayoutResult> result = input->Layout(constraint_space.get(),
                                                /* break_token = */ nullptr);

  clearNeedsLayout();
}

void LayoutNGMathMLFlow::computeIntrinsicSizingInfo(
    IntrinsicSizingInfo& info) const {
  // TODO(emilio).
  info.hasWidth = true;
  info.hasHeight = true;
  info.size = FloatSize(100.0f, 50.0f);
  info.aspectRatio = info.size;
}

}  // namespace blink
