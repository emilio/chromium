/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "core/html/parser/HTMLParserIdioms.h"
#include "core/mathml/MathMLElement.h"

namespace blink {

MathMLElement::MathMLElement(const QualifiedName& tagName,
                             Document& document,
                             ConstructionType constructionType)
    : Element(tagName, &document, constructionType) {}

MathMLElement::~MathMLElement() {}

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(MathMLElement)

static inline unsigned parseNamedSpace(const String& string) {
  if (string == "veryverythinmathspace")
    return 1;
  if (string == "verythinmathspace")
    return 2;
  if (string == "thinmathspace")
    return 3;
  if (string == "mediummathspace")
    return 4;
  if (string == "thickmathspace")
    return 5;
  if (string == "verythickmathspace")
    return 6;
  if (string == "veryverythickmathspace")
    return 7;
  return 0;
}

MathMLElement::Length MathMLElement::parseMathMLLength(const String& string) {
  Length length;

  // The regular expression from the MathML Relax NG schema is as follows:
  //
  //   pattern =
  //   '\s*((-?[0-9]*([0-9]\.?|\.[0-9])[0-9]*(e[mx]|in|cm|mm|p[xtc]|%)?)|(negative)?((very){0,2}thi(n|ck)|medium)mathspace)\s*'
  //
  // We do not perform a strict verification of the syntax of whitespaces and
  // number.
  // Instead, we just use isHTMLSpace and toFloat to parse these parts.

  // We first skip whitespace from both ends of the string.
  unsigned start = 0, stringLength = string.length();
  while (stringLength > 0 && isHTMLSpace(string[start])) {
    start++;
    stringLength--;
  }
  while (stringLength > 0 && isHTMLSpace(string[start + stringLength - 1]))
    stringLength--;
  if (!stringLength)
    return length;

  // We consider the most typical case: a number followed by an optional unit.
  UChar firstChar = string[start];
  if (isASCIIDigit(firstChar) || firstChar == '-' || firstChar == '.') {
    LengthType lengthType = LengthType::UnitLess;
    UChar lastChar = string[start + stringLength - 1];
    if (lastChar == '%') {
      lengthType = LengthType::Percentage;
      stringLength--;
    } else if (stringLength >= 2) {
      UChar penultimateChar = string[start + stringLength - 2];
      if (penultimateChar == 'c' && lastChar == 'm')
        lengthType = LengthType::Cm;
      if (penultimateChar == 'e' && lastChar == 'm')
        lengthType = LengthType::Em;
      else if (penultimateChar == 'e' && lastChar == 'x')
        lengthType = LengthType::Ex;
      else if (penultimateChar == 'i' && lastChar == 'n')
        lengthType = LengthType::In;
      else if (penultimateChar == 'm' && lastChar == 'm')
        lengthType = LengthType::Mm;
      else if (penultimateChar == 'p' && lastChar == 'c')
        lengthType = LengthType::Pc;
      else if (penultimateChar == 'p' && lastChar == 't')
        lengthType = LengthType::Pt;
      else if (penultimateChar == 'p' && lastChar == 'x')
        lengthType = LengthType::Px;

      if (lengthType != LengthType::UnitLess)
        stringLength -= 2;
    }
    bool ok;
    float lengthValue = string.substring(start, stringLength).toFloat(&ok);
    if (ok) {
      length.type = lengthType;
      length.value = lengthValue;
    }
    return length;
  }

  // Otherwise, we try and parse a named space.
  bool negative = string.startsWith("negative");
  if (negative) {
    start += 8;
    stringLength -= 8;
  }
  unsigned namedSpaceValue =
      parseNamedSpace(string.substring(start, stringLength));
  if (namedSpaceValue) {
    length.type = LengthType::MathUnit;
    length.value = namedSpaceValue * (negative ? -1 : 1);
  }
  return length;
}

const MathMLElement::Length& MathMLElement::cachedMathMLLength(
    const QualifiedName& name,
    Length& length) {
  if (length.dirty) {
    length = parseMathMLLength(fastGetAttribute(name));
    length.dirty = false;
  }
  return length;
}

} // namespace blink
