#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/mathml/layout_ng_mathml_math.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/mathml/MathMLMathElement.h"
#include "core/paint/BoxPainter.h"

namespace blink {

LayoutNGMathMLMath::LayoutNGMathMLMath(MathMLMathElement* element)
  : LayoutReplaced(element) {
  DCHECK(element);
}

bool LayoutNGMathMLMath::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectMathMLMath || type == LayoutObjectMathML ||
         LayoutReplaced::isOfType(type);
}

NGMathMLMathNode* LayoutNGMathMLMath::toNGLayoutInputNode(
    const ComputedStyle& style) {
  return new NGMathMLMathNode(this);

}

bool LayoutNGMathMLMath::isChildAllowed(LayoutObject* child,
                                        const ComputedStyle&) const {
  return child->isMathML();
}

void LayoutNGMathMLMath::layout() {
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

void LayoutNGMathMLMath::computeIntrinsicSizingInfo(
    IntrinsicSizingInfo& info) const {
  // TODO(emilio).
  info.hasWidth = true;
  info.hasHeight = true;
  info.size = FloatSize(100.0f, 50.0f);
  info.aspectRatio = info.size;
}

void LayoutNGMathMLMath::paintReplaced(const PaintInfo& paintInfo,
                                       const LayoutPoint& paintOffset) const {
  BoxPainter(*this).paintChildren(paintInfo, paintOffset);
}
}  // namespace blink
