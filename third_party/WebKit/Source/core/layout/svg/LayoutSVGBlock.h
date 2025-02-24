/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef LayoutSVGBlock_h
#define LayoutSVGBlock_h

#include "core/layout/LayoutBlockFlow.h"

namespace blink {

class SVGElement;

// A common class of SVG objects that delegate layout, paint, etc. tasks to
// LayoutBlockFlow. It has two coordinate spaces:
// - local SVG coordinate space: similar to LayoutSVGModelObject, the space
//   that localSVGTransform() applies.
// - local HTML coordinate space: defined by frameRect() as if the local SVG
//   coordinate space created a containing block. Like other LayoutBlockFlow
//   objects, LayoutSVGBlock's frameRect() is also in physical coordinates with
//   flipped blocks direction in the "containing block".
class LayoutSVGBlock : public LayoutBlockFlow {
 public:
  explicit LayoutSVGBlock(SVGElement*);

  // These mapping functions map coordinates in HTML spaces.
  void mapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags = ApplyContainerFlip) const final;
  void mapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags = ApplyContainerFlip) const final;
  const LayoutObject* pushMappingToContainer(
      const LayoutBoxModelObject* ancestorToStopAt,
      LayoutGeometryMap&) const final;
  bool mapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = DefaultVisualRectFlags) const final;

  AffineTransform localSVGTransform() const final { return m_localTransform; }

  PaintLayerType layerTypeRequired() const final { return NoPaintLayer; }

 protected:
  void willBeDestroyed() override;

  AffineTransform m_localTransform;

  bool isOfType(LayoutObjectType type) const override {
    return type == LayoutObjectSVG || LayoutBlockFlow::isOfType(type);
  }

 private:
  LayoutRect absoluteVisualRect() const final;

  bool allowsOverflowClip() const final;

  void absoluteRects(Vector<IntRect>&,
                     const LayoutPoint& accumulatedOffset) const final;

  void updateFromStyle() final;
  void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) final;

  bool nodeAtPoint(HitTestResult&,
                   const HitTestLocation& locationInContainer,
                   const LayoutPoint& accumulatedOffset,
                   HitTestAction) override;

  // The inherited version doesn't check for SVG effects.
  bool paintedOutputOfObjectHasNoEffectRegardlessOfSize() const override {
    return false;
  }
};

}  // namespace blink
#endif  // LayoutSVGBlock_h
