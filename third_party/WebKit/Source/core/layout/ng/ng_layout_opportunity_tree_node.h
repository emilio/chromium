// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunityTreeNode_h
#define NGLayoutOpportunityTreeNode_h

#include "core/layout/ng/geometry/ng_edge.h"
#include "core/layout/ng/geometry/ng_logical_rect.h"
#include "core/layout/ng/ng_exclusion.h"
#include "platform/heap/Handle.h"

namespace blink {

// 3 node R-Tree that represents available space(left, bottom, right) or
// layout opportunity after the parent spatial rectangle is split by the
// exclusion rectangle.
struct CORE_EXPORT NGLayoutOpportunityTreeNode
    : public GarbageCollectedFinalized<NGLayoutOpportunityTreeNode> {
 public:
  // Default constructor.
  // Creates a Layout Opportunity tree node that is limited by it's own edge
  // from above.
  // @param opportunity The layout opportunity for this node.
  NGLayoutOpportunityTreeNode(const NGLogicalRect opportunity);

  // Constructor that creates a node with explicitly set exclusion edge.
  // @param opportunity The layout opportunity for this node.
  // @param exclusion_edge Edge that limits this node's opportunity from above.
  NGLayoutOpportunityTreeNode(const NGLogicalRect opportunity,
                              NGEdge exclusion_edge);

  // Children of the node.
  Member<NGLayoutOpportunityTreeNode> left;
  Member<NGLayoutOpportunityTreeNode> bottom;
  Member<NGLayoutOpportunityTreeNode> right;

  // The top layout opportunity associated with this node.
  NGLogicalRect opportunity;

  // Edge that limits this layout opportunity from above.
  NGEdge exclusion_edge;

  // Exclusions that splits apart this layout opportunity.
  Vector<const NGExclusion*> exclusions;  // Not owned.

  // Exclusion that represent all combined exclusions that
  // split this node.
  std::unique_ptr<NGExclusion> combined_exclusion;

  // Whether this node is a leaf node.
  // The node is a leaf if it doesn't have exclusions that split it apart.
  bool IsLeafNode() const { return exclusions.isEmpty(); }

  String ToString() const;

  DECLARE_TRACE();
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGLayoutOpportunityTreeNode& value) {
  return stream << value.ToString();
}

inline std::ostream& operator<<(std::ostream& out,
                                const NGLayoutOpportunityTreeNode* value) {
  return out << (value ? value->ToString() : "(null)");
}

}  // namespace blink
#endif  // NGLayoutOpportunityTreeNode_h
