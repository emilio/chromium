// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxStrut_h
#define NGBoxStrut_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/text/TextDirection.h"

namespace blink {

// This struct is used for storing margins, borders or padding of a box on all
// four edges.
struct CORE_EXPORT NGBoxStrut {
  NGBoxStrut() {}
  NGBoxStrut(LayoutUnit inline_start,
             LayoutUnit inline_end,
             LayoutUnit block_start,
             LayoutUnit block_end)
      : inline_start(inline_start),
        inline_end(inline_end),
        block_start(block_start),
        block_end(block_end) {}

  LayoutUnit InlineSum() const { return inline_start + inline_end; }
  LayoutUnit BlockSum() const { return block_start + block_end; }

  bool IsEmpty() const;

  // The following two operators exist primarily to have an easy way to access
  // the sum of border and padding.
  NGBoxStrut& operator+=(const NGBoxStrut& other) {
    inline_start += other.inline_start;
    inline_end += other.inline_end;
    block_start += other.block_start;
    block_end += other.block_end;
    return *this;
  }

  NGBoxStrut operator+(const NGBoxStrut& other) {
    NGBoxStrut result(*this);
    result += other;
    return result;
  }

  bool operator==(const NGBoxStrut& other) const;

  String ToString() const;

  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit block_start;
  LayoutUnit block_end;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGBoxStrut&);

// Struct to store physical dimensions, independent of writing mode and
// direction.
// See https://drafts.csswg.org/css-writing-modes-3/#abstract-box
struct CORE_EXPORT NGPhysicalBoxStrut {
  LayoutUnit left;
  LayoutUnit right;
  LayoutUnit top;
  LayoutUnit bottom;
  NGBoxStrut ConvertToLogical(NGWritingMode, TextDirection) const;
};

}  // namespace blink

#endif  // NGBoxStrut_h
