/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Computer, Inc.
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
#include "core/css/CSSShadowValue.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Used for text-shadow and box-shadow
CSSShadowValue::CSSShadowValue(CSSPrimitiveValue* x,
                               CSSPrimitiveValue* y,
                               CSSPrimitiveValue* blur,
                               CSSPrimitiveValue* spread,
                               CSSIdentifierValue* style,
                               CSSValue* color)
    : CSSValue(ShadowClass),
      x(x),
      y(y),
      blur(blur),
      spread(spread),
      style(style),
      color(color) {}

String CSSShadowValue::customCSSText() const {
  StringBuilder text;

  if (color)
    text.append(color->cssText());
  if (x) {
    if (!text.isEmpty())
      text.append(' ');
    text.append(x->cssText());
  }
  if (y) {
    if (!text.isEmpty())
      text.append(' ');
    text.append(y->cssText());
  }
  if (blur) {
    if (!text.isEmpty())
      text.append(' ');
    text.append(blur->cssText());
  }
  if (spread) {
    if (!text.isEmpty())
      text.append(' ');
    text.append(spread->cssText());
  }
  if (style) {
    if (!text.isEmpty())
      text.append(' ');
    text.append(style->cssText());
  }

  return text.toString();
}

bool CSSShadowValue::equals(const CSSShadowValue& other) const {
  return dataEquivalent(color, other.color) && dataEquivalent(x, other.x) &&
         dataEquivalent(y, other.y) && dataEquivalent(blur, other.blur) &&
         dataEquivalent(spread, other.spread) &&
         dataEquivalent(style, other.style);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSShadowValue) {
  visitor->trace(x);
  visitor->trace(y);
  visitor->trace(blur);
  visitor->trace(spread);
  visitor->trace(style);
  visitor->trace(color);
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
