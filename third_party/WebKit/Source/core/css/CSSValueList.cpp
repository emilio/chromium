/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "core/css/CSSValueList.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSParser.h"
#include "wtf/SizeAssertions.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

struct SameSizeAsCSSValueList : CSSValue {
  Vector<Member<CSSValue>, 4> list_values;
};
ASSERT_SIZE(CSSValueList, SameSizeAsCSSValueList);

CSSValueList::CSSValueList(ClassType classType,
                           ValueListSeparator listSeparator)
    : CSSValue(classType) {
  m_valueListSeparator = listSeparator;
}

CSSValueList::CSSValueList(ValueListSeparator listSeparator)
    : CSSValue(ValueListClass) {
  m_valueListSeparator = listSeparator;
}

bool CSSValueList::removeAll(const CSSValue& val) {
  bool found = false;
  for (int index = m_values.size() - 1; index >= 0; --index) {
    Member<const CSSValue>& value = m_values.at(index);
    if (value && *value == val) {
      m_values.remove(index);
      found = true;
    }
  }

  return found;
}

bool CSSValueList::hasValue(const CSSValue& val) const {
  for (size_t index = 0; index < m_values.size(); index++) {
    const Member<const CSSValue>& value = m_values.at(index);
    if (value && *value == val) {
      return true;
    }
  }
  return false;
}

CSSValueList* CSSValueList::copy() const {
  CSSValueList* newList = nullptr;
  switch (m_valueListSeparator) {
    case SpaceSeparator:
      newList = createSpaceSeparated();
      break;
    case CommaSeparator:
      newList = createCommaSeparated();
      break;
    case SlashSeparator:
      newList = createSlashSeparated();
      break;
    default:
      ASSERT_NOT_REACHED();
  }
  newList->m_values = m_values;
  return newList;
}

String CSSValueList::customCSSText() const {
  StringBuilder result;
  String separator;
  switch (m_valueListSeparator) {
    case SpaceSeparator:
      separator = " ";
      break;
    case CommaSeparator:
      separator = ", ";
      break;
    case SlashSeparator:
      separator = " / ";
      break;
    default:
      ASSERT_NOT_REACHED();
  }

  unsigned size = m_values.size();
  for (unsigned i = 0; i < size; i++) {
    if (!result.isEmpty())
      result.append(separator);
    result.append(m_values[i]->cssText());
  }

  return result.toString();
}

bool CSSValueList::equals(const CSSValueList& other) const {
  return m_valueListSeparator == other.m_valueListSeparator &&
         compareCSSValueVector(m_values, other.m_values);
}

bool CSSValueList::hasFailedOrCanceledSubresources() const {
  for (unsigned i = 0; i < m_values.size(); ++i) {
    if (m_values[i]->hasFailedOrCanceledSubresources())
      return true;
  }
  return false;
}

bool CSSValueList::mayContainUrl() const {
  for (const auto& value : m_values) {
    if (value->mayContainUrl())
      return true;
  }
  return false;
}

void CSSValueList::reResolveUrl(const Document& document) const {
  for (const auto& value : m_values)
    value->reResolveUrl(document);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSValueList) {
  visitor->trace(m_values);
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
