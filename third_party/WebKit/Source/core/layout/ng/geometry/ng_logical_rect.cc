// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_logical_rect.h"

#include "wtf/text/WTFString.h"

namespace blink {

bool NGLogicalRect::IsEmpty() const {
  return size.IsEmpty() && offset.inline_offset == LayoutUnit() &&
         offset.block_offset == LayoutUnit();
}

bool NGLogicalRect::IsContained(const NGLogicalRect& other) const {
  return !(InlineEndOffset() <= other.InlineStartOffset() ||
           BlockEndOffset() <= other.BlockStartOffset() ||
           InlineStartOffset() >= other.InlineEndOffset() ||
           BlockStartOffset() >= other.BlockEndOffset());
}

bool NGLogicalRect::operator==(const NGLogicalRect& other) const {
  return std::tie(other.offset, other.size) == std::tie(offset, size);
}

String NGLogicalRect::ToString() const {
  return IsEmpty()
             ? "(empty)"
             : String::format("%sx%s at (%s,%s)",
                              size.inline_size.toString().ascii().data(),
                              size.block_size.toString().ascii().data(),
                              offset.inline_offset.toString().ascii().data(),
                              offset.block_offset.toString().ascii().data());
}

std::ostream& operator<<(std::ostream& os, const NGLogicalRect& value) {
  return os << value.ToString();
}

}  // namespace blink
