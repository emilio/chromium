/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2008, 2011, 2012, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
 *
 */

#include "core/html/HTMLCollection.h"

#include "core/HTMLNames.h"
#include "core/dom/ClassCollection.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeRareData.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLDataListOptionsCollection.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLOptionsCollection.h"
#include "core/html/HTMLTagCollection.h"
#include "core/html/WindowNameCollection.h"
#include "wtf/HashSet.h"

namespace blink {

using namespace HTMLNames;

static bool shouldTypeOnlyIncludeDirectChildren(CollectionType type) {
  switch (type) {
    case ClassCollectionType:
    case TagCollectionType:
    case HTMLTagCollectionType:
    case DocAll:
    case DocAnchors:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocImages:
    case DocLinks:
    case DocScripts:
    case DocumentNamedItems:
    case MapAreas:
    case TableRows:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case WindowNamedItems:
    case FormControls:
      return false;
    case NodeChildren:
    case TRCells:
    case TSectionRows:
    case TableTBodies:
      return true;
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
      break;
  }
  NOTREACHED();
  return false;
}

static NodeListRootType rootTypeFromCollectionType(const ContainerNode& owner,
                                                   CollectionType type) {
  switch (type) {
    case DocImages:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocLinks:
    case DocAnchors:
    case DocScripts:
    case DocAll:
    case WindowNamedItems:
    case DocumentNamedItems:
      return NodeListRootType::TreeScope;
    case ClassCollectionType:
    case TagCollectionType:
    case HTMLTagCollectionType:
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case MapAreas:
      return NodeListRootType::Node;
    case FormControls:
      if (isHTMLFieldSetElement(owner))
        return NodeListRootType::Node;
      DCHECK(isHTMLFormElement(owner));
      return NodeListRootType::TreeScope;
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
      break;
  }
  NOTREACHED();
  return NodeListRootType::Node;
}

static NodeListInvalidationType invalidationTypeExcludingIdAndNameAttributes(
    CollectionType type) {
  switch (type) {
    case TagCollectionType:
    case HTMLTagCollectionType:
    case DocImages:
    case DocEmbeds:
    case DocForms:
    case DocScripts:
    case DocAll:
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case MapAreas:
      return DoNotInvalidateOnAttributeChanges;
    case DocApplets:
    case SelectedOptions:
    case DataListOptions:
      // FIXME: We can do better some day.
      return InvalidateOnAnyAttrChange;
    case DocAnchors:
      return InvalidateOnNameAttrChange;
    case DocLinks:
      return InvalidateOnHRefAttrChange;
    case WindowNamedItems:
      return InvalidateOnIdNameAttrChange;
    case DocumentNamedItems:
      return InvalidateOnIdNameAttrChange;
    case FormControls:
      return InvalidateForFormControls;
    case ClassCollectionType:
      return InvalidateOnClassAttrChange;
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
      break;
  }
  NOTREACHED();
  return DoNotInvalidateOnAttributeChanges;
}

HTMLCollection::HTMLCollection(ContainerNode& ownerNode,
                               CollectionType type,
                               ItemAfterOverrideType itemAfterOverrideType)
    : LiveNodeListBase(ownerNode,
                       rootTypeFromCollectionType(ownerNode, type),
                       invalidationTypeExcludingIdAndNameAttributes(type),
                       type),
      m_overridesItemAfter(itemAfterOverrideType == OverridesItemAfter),
      m_shouldOnlyIncludeDirectChildren(
          shouldTypeOnlyIncludeDirectChildren(type)) {
  // Keep this in the child class because |registerNodeList| requires wrapper
  // tracing and potentially calls virtual methods which is not allowed in a
  // base class constructor.
  document().registerNodeList(this);
}

HTMLCollection* HTMLCollection::create(ContainerNode& base,
                                       CollectionType type) {
  return new HTMLCollection(base, type, DoesNotOverrideItemAfter);
}

HTMLCollection::~HTMLCollection() {}

void HTMLCollection::invalidateCache(Document* oldDocument) const {
  m_collectionItemsCache.invalidate();
  invalidateIdNameCacheMaps(oldDocument);
}

unsigned HTMLCollection::length() const {
  return m_collectionItemsCache.nodeCount(*this);
}

Element* HTMLCollection::item(unsigned offset) const {
  return m_collectionItemsCache.nodeAt(*this, offset);
}

static inline bool isMatchingHTMLElement(const HTMLCollection& htmlCollection,
                                         const HTMLElement& element) {
  switch (htmlCollection.type()) {
    case DocImages:
      return element.hasTagName(imgTag);
    case DocScripts:
      return element.hasTagName(scriptTag);
    case DocForms:
      return element.hasTagName(formTag);
    case DocumentNamedItems:
      return toDocumentNameCollection(htmlCollection).elementMatches(element);
    case TableTBodies:
      return element.hasTagName(tbodyTag);
    case TRCells:
      return element.hasTagName(tdTag) || element.hasTagName(thTag);
    case TSectionRows:
      return element.hasTagName(trTag);
    case SelectOptions:
      return toHTMLOptionsCollection(htmlCollection).elementMatches(element);
    case SelectedOptions:
      return isHTMLOptionElement(element) &&
             toHTMLOptionElement(element).selected();
    case DataListOptions:
      return toHTMLDataListOptionsCollection(htmlCollection)
          .elementMatches(element);
    case MapAreas:
      return element.hasTagName(areaTag);
    case DocApplets:
      return isHTMLObjectElement(element) &&
             toHTMLObjectElement(element).containsJavaApplet();
    case DocEmbeds:
      return element.hasTagName(embedTag);
    case DocLinks:
      return (element.hasTagName(aTag) || element.hasTagName(areaTag)) &&
             element.fastHasAttribute(hrefAttr);
    case DocAnchors:
      return element.hasTagName(aTag) && element.fastHasAttribute(nameAttr);
    case FormControls:
      DCHECK(isHTMLFieldSetElement(htmlCollection.ownerNode()));
      return isHTMLObjectElement(element) || isHTMLFormControlElement(element);
    case ClassCollectionType:
    case TagCollectionType:
    case HTMLTagCollectionType:
    case DocAll:
    case NodeChildren:
    case TableRows:
    case WindowNamedItems:
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
      NOTREACHED();
  }
  return false;
}

inline bool HTMLCollection::elementMatches(const Element& element) const {
  // These collections apply to any kind of Elements, not just HTMLElements.
  switch (type()) {
    case DocAll:
    case NodeChildren:
      return true;
    case ClassCollectionType:
      return toClassCollection(*this).elementMatches(element);
    case TagCollectionType:
      return toTagCollection(*this).elementMatches(element);
    case HTMLTagCollectionType:
      return toHTMLTagCollection(*this).elementMatches(element);
    case WindowNamedItems:
      return toWindowNameCollection(*this).elementMatches(element);
    default:
      break;
  }

  // The following only applies to HTMLElements.
  return element.isHTMLElement() &&
         isMatchingHTMLElement(*this, toHTMLElement(element));
}

namespace {

template <class HTMLCollectionType>
class IsMatch {
  STACK_ALLOCATED();

 public:
  IsMatch(const HTMLCollectionType& list) : m_list(&list) {}

  bool operator()(const Element& element) const {
    return m_list->elementMatches(element);
  }

 private:
  Member<const HTMLCollectionType> m_list;
};

}  // namespace

template <class HTMLCollectionType>
static inline IsMatch<HTMLCollectionType> makeIsMatch(
    const HTMLCollectionType& list) {
  return IsMatch<HTMLCollectionType>(list);
}

Element* HTMLCollection::virtualItemAfter(Element*) const {
  NOTREACHED();
  return nullptr;
}

// https://html.spec.whatwg.org/multipage/infrastructure.html#all-named-elements
// The document.all collection returns only certain types of elements by name,
// although it returns any type of element by id.
static inline bool nameShouldBeVisibleInDocumentAll(
    const HTMLElement& element) {
  return element.hasTagName(aTag) || element.hasTagName(appletTag) ||
         element.hasTagName(buttonTag) || element.hasTagName(embedTag) ||
         element.hasTagName(formTag) || element.hasTagName(frameTag) ||
         element.hasTagName(framesetTag) || element.hasTagName(iframeTag) ||
         element.hasTagName(imgTag) || element.hasTagName(inputTag) ||
         element.hasTagName(mapTag) || element.hasTagName(metaTag) ||
         element.hasTagName(objectTag) || element.hasTagName(selectTag) ||
         element.hasTagName(textareaTag);
}

Element* HTMLCollection::traverseToFirst() const {
  switch (type()) {
    case HTMLTagCollectionType:
      return ElementTraversal::firstWithin(
          rootNode(), makeIsMatch(toHTMLTagCollection(*this)));
    case ClassCollectionType:
      return ElementTraversal::firstWithin(
          rootNode(), makeIsMatch(toClassCollection(*this)));
    default:
      if (overridesItemAfter())
        return virtualItemAfter(0);
      if (shouldOnlyIncludeDirectChildren())
        return ElementTraversal::firstChild(rootNode(), makeIsMatch(*this));
      return ElementTraversal::firstWithin(rootNode(), makeIsMatch(*this));
  }
}

Element* HTMLCollection::traverseToLast() const {
  DCHECK(canTraverseBackward());
  if (shouldOnlyIncludeDirectChildren())
    return ElementTraversal::lastChild(rootNode(), makeIsMatch(*this));
  return ElementTraversal::lastWithin(rootNode(), makeIsMatch(*this));
}

Element* HTMLCollection::traverseForwardToOffset(
    unsigned offset,
    Element& currentElement,
    unsigned& currentOffset) const {
  DCHECK_LT(currentOffset, offset);
  switch (type()) {
    case HTMLTagCollectionType:
      return traverseMatchingElementsForwardToOffset(
          currentElement, &rootNode(), offset, currentOffset,
          makeIsMatch(toHTMLTagCollection(*this)));
    case ClassCollectionType:
      return traverseMatchingElementsForwardToOffset(
          currentElement, &rootNode(), offset, currentOffset,
          makeIsMatch(toClassCollection(*this)));
    default:
      if (overridesItemAfter()) {
        for (Element* next = virtualItemAfter(&currentElement); next;
             next = virtualItemAfter(next)) {
          if (++currentOffset == offset)
            return next;
        }
        return nullptr;
      }
      if (shouldOnlyIncludeDirectChildren()) {
        IsMatch<HTMLCollection> isMatch(*this);
        for (Element* next =
                 ElementTraversal::nextSibling(currentElement, isMatch);
             next; next = ElementTraversal::nextSibling(*next, isMatch)) {
          if (++currentOffset == offset)
            return next;
        }
        return nullptr;
      }
      return traverseMatchingElementsForwardToOffset(
          currentElement, &rootNode(), offset, currentOffset,
          makeIsMatch(*this));
  }
}

Element* HTMLCollection::traverseBackwardToOffset(
    unsigned offset,
    Element& currentElement,
    unsigned& currentOffset) const {
  DCHECK_GT(currentOffset, offset);
  DCHECK(canTraverseBackward());
  if (shouldOnlyIncludeDirectChildren()) {
    IsMatch<HTMLCollection> isMatch(*this);
    for (Element* previous =
             ElementTraversal::previousSibling(currentElement, isMatch);
         previous;
         previous = ElementTraversal::previousSibling(*previous, isMatch)) {
      if (--currentOffset == offset)
        return previous;
    }
    return nullptr;
  }
  return traverseMatchingElementsBackwardToOffset(
      currentElement, &rootNode(), offset, currentOffset, makeIsMatch(*this));
}

Element* HTMLCollection::namedItem(const AtomicString& name) const {
  // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
  // This method first searches for an object with a matching id
  // attribute. If a match is not found, the method then searches for an
  // object with a matching name attribute, but only on those elements
  // that are allowed a name attribute.
  updateIdNameCache();

  const NamedItemCache& cache = namedItemCache();
  HeapVector<Member<Element>>* idResults = cache.getElementsById(name);
  if (idResults && !idResults->isEmpty())
    return idResults->front();

  HeapVector<Member<Element>>* nameResults = cache.getElementsByName(name);
  if (nameResults && !nameResults->isEmpty())
    return nameResults->front();

  return nullptr;
}

bool HTMLCollection::namedPropertyQuery(const AtomicString& name,
                                        ExceptionState&) {
  return namedItem(name);
}

void HTMLCollection::supportedPropertyNames(Vector<String>& names) {
  // As per the specification (https://dom.spec.whatwg.org/#htmlcollection):
  // The supported property names are the values from the list returned by these
  // steps:
  // 1. Let result be an empty list.
  // 2. For each element represented by the collection, in tree order, run these
  //    substeps:
  //   1. If element has an ID which is neither the empty string nor is in
  //      result, append element's ID to result.
  //   2. If element is in the HTML namespace and has a name attribute whose
  //      value is neither the empty string nor is in result, append element's
  //      name attribute value to result.
  // 3. Return result.
  HashSet<AtomicString> existingNames;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    Element* element = item(i);
    const AtomicString& idAttribute = element->getIdAttribute();
    if (!idAttribute.isEmpty()) {
      HashSet<AtomicString>::AddResult addResult =
          existingNames.insert(idAttribute);
      if (addResult.isNewEntry)
        names.push_back(idAttribute);
    }
    if (!element->isHTMLElement())
      continue;
    const AtomicString& nameAttribute = element->getNameAttribute();
    if (!nameAttribute.isEmpty() &&
        (type() != DocAll ||
         nameShouldBeVisibleInDocumentAll(toHTMLElement(*element)))) {
      HashSet<AtomicString>::AddResult addResult =
          existingNames.insert(nameAttribute);
      if (addResult.isNewEntry)
        names.push_back(nameAttribute);
    }
  }
}

void HTMLCollection::namedPropertyEnumerator(Vector<String>& names,
                                             ExceptionState&) {
  supportedPropertyNames(names);
}

void HTMLCollection::updateIdNameCache() const {
  if (hasValidIdNameCache())
    return;

  NamedItemCache* cache = NamedItemCache::create();
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    Element* element = item(i);
    const AtomicString& idAttrVal = element->getIdAttribute();
    if (!idAttrVal.isEmpty())
      cache->addElementWithId(idAttrVal, element);
    if (!element->isHTMLElement())
      continue;
    const AtomicString& nameAttrVal = element->getNameAttribute();
    if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal &&
        (type() != DocAll ||
         nameShouldBeVisibleInDocumentAll(toHTMLElement(*element))))
      cache->addElementWithName(nameAttrVal, element);
  }
  // Set the named item cache last as traversing the tree may cause cache
  // invalidation.
  setNamedItemCache(cache);
}

void HTMLCollection::namedItems(const AtomicString& name,
                                HeapVector<Member<Element>>& result) const {
  DCHECK(result.isEmpty());
  if (name.isEmpty())
    return;

  updateIdNameCache();

  const NamedItemCache& cache = namedItemCache();
  if (HeapVector<Member<Element>>* idResults = cache.getElementsById(name)) {
    for (const auto& element : *idResults)
      result.push_back(element);
  }
  if (HeapVector<Member<Element>>* nameResults =
          cache.getElementsByName(name)) {
    for (const auto& element : *nameResults)
      result.push_back(element);
  }
}

HTMLCollection::NamedItemCache::NamedItemCache() {}

DEFINE_TRACE(HTMLCollection) {
  visitor->trace(m_namedItemCache);
  visitor->trace(m_collectionItemsCache);
  LiveNodeListBase::trace(visitor);
}

}  // namespace blink
