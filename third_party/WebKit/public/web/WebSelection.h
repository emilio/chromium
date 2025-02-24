// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSelection_h
#define WebSelection_h

#include "../platform/WebCommon.h"
#include "../platform/WebSelectionBound.h"

namespace blink {

struct CompositedSelection;

// The active selection region, containing compositing data for the selection
// end points as well as metadata for the selection region.
class BLINK_EXPORT WebSelection {
 public:
  enum SelectionType { NoSelection, CaretSelection, RangeSelection };

#if INSIDE_BLINK
  explicit WebSelection(const CompositedSelection&);
#endif
  WebSelection(const WebSelection&);

  const WebSelectionBound& start() const { return m_start; }
  const WebSelectionBound& end() const { return m_end; }

  bool isNone() const { return selectionType() == NoSelection; }
  bool isCaret() const { return selectionType() == CaretSelection; }
  bool isRange() const { return selectionType() == RangeSelection; }

 private:
  SelectionType selectionType() const { return m_selectionType; }

  SelectionType m_selectionType;

  WebSelectionBound m_start;
  WebSelectionBound m_end;
};

}  // namespace blink

#endif  // WebSelection_h
