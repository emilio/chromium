/*
 * Copyright (C) 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
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

#include "core/html/forms/FormController.h"

#include "core/events/ScopedEventQueue.h"
#include "core/html/HTMLFormControlElementWithState.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "platform/FileChooser.h"
#include "wtf/Deque.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

using namespace HTMLNames;

static inline HTMLFormElement* ownerFormForState(
    const HTMLFormControlElementWithState& control) {
  // Assume controls with form attribute have no owners because we restore
  // state during parsing and form owners of such controls might be
  // indeterminate.
  return control.fastHasAttribute(formAttr) ? 0 : control.form();
}

// ----------------------------------------------------------------------------

// Serilized form of FormControlState:
//  (',' means strings around it are separated in stateVector.)
//
// SerializedControlState ::= SkipState | RestoreState
// SkipState ::= '0'
// RestoreState ::= UnsignedNumber, ControlValue+
// UnsignedNumber ::= [0-9]+
// ControlValue ::= arbitrary string
//
// RestoreState has a sequence of ControlValues. The length of the
// sequence is represented by UnsignedNumber.

void FormControlState::serializeTo(Vector<String>& stateVector) const {
  DCHECK(!isFailure());
  stateVector.push_back(String::number(m_values.size()));
  for (const auto& value : m_values)
    stateVector.push_back(value.isNull() ? emptyString : value);
}

FormControlState FormControlState::deserialize(
    const Vector<String>& stateVector,
    size_t& index) {
  if (index >= stateVector.size())
    return FormControlState(TypeFailure);
  size_t valueSize = stateVector[index++].toUInt();
  if (!valueSize)
    return FormControlState();
  if (index + valueSize > stateVector.size())
    return FormControlState(TypeFailure);
  FormControlState state;
  state.m_values.reserveCapacity(valueSize);
  for (size_t i = 0; i < valueSize; ++i)
    state.append(stateVector[index++]);
  return state;
}

// ----------------------------------------------------------------------------

class FormElementKey {
 public:
  FormElementKey(StringImpl* = 0, StringImpl* = 0);
  ~FormElementKey();
  FormElementKey(const FormElementKey&);
  FormElementKey& operator=(const FormElementKey&);

  StringImpl* name() const { return m_name; }
  StringImpl* type() const { return m_type; }

  // Hash table deleted values, which are only constructed and never copied or
  // destroyed.
  FormElementKey(WTF::HashTableDeletedValueType)
      : m_name(hashTableDeletedValue()) {}
  bool isHashTableDeletedValue() const {
    return m_name == hashTableDeletedValue();
  }

 private:
  void ref() const;
  void deref() const;

  static StringImpl* hashTableDeletedValue() {
    return reinterpret_cast<StringImpl*>(-1);
  }

  StringImpl* m_name;
  StringImpl* m_type;
};

FormElementKey::FormElementKey(StringImpl* name, StringImpl* type)
    : m_name(name), m_type(type) {
  ref();
}

FormElementKey::~FormElementKey() {
  deref();
}

FormElementKey::FormElementKey(const FormElementKey& other)
    : m_name(other.name()), m_type(other.type()) {
  ref();
}

FormElementKey& FormElementKey::operator=(const FormElementKey& other) {
  other.ref();
  deref();
  m_name = other.name();
  m_type = other.type();
  return *this;
}

void FormElementKey::ref() const {
  if (name())
    name()->ref();
  if (type())
    type()->ref();
}

void FormElementKey::deref() const {
  if (name())
    name()->deref();
  if (type())
    type()->deref();
}

inline bool operator==(const FormElementKey& a, const FormElementKey& b) {
  return a.name() == b.name() && a.type() == b.type();
}

struct FormElementKeyHash {
  static unsigned hash(const FormElementKey&);
  static bool equal(const FormElementKey& a, const FormElementKey& b) {
    return a == b;
  }
  static const bool safeToCompareToEmptyOrDeleted = true;
};

unsigned FormElementKeyHash::hash(const FormElementKey& key) {
  return StringHasher::hashMemory<sizeof(FormElementKey)>(&key);
}

struct FormElementKeyHashTraits : WTF::GenericHashTraits<FormElementKey> {
  static void constructDeletedValue(FormElementKey& slot, bool) {
    new (NotNull, &slot) FormElementKey(WTF::HashTableDeletedValue);
  }
  static bool isDeletedValue(const FormElementKey& value) {
    return value.isHashTableDeletedValue();
  }
};

// ----------------------------------------------------------------------------

class SavedFormState {
  WTF_MAKE_NONCOPYABLE(SavedFormState);
  USING_FAST_MALLOC(SavedFormState);

 public:
  static std::unique_ptr<SavedFormState> create();
  static std::unique_ptr<SavedFormState> deserialize(const Vector<String>&,
                                                     size_t& index);
  void serializeTo(Vector<String>&) const;
  bool isEmpty() const { return m_stateForNewFormElements.isEmpty(); }
  void appendControlState(const AtomicString& name,
                          const AtomicString& type,
                          const FormControlState&);
  FormControlState takeControlState(const AtomicString& name,
                                    const AtomicString& type);

  Vector<String> getReferencedFilePaths() const;

 private:
  SavedFormState() : m_controlStateCount(0) {}

  using FormElementStateMap = HashMap<FormElementKey,
                                      Deque<FormControlState>,
                                      FormElementKeyHash,
                                      FormElementKeyHashTraits>;
  FormElementStateMap m_stateForNewFormElements;
  size_t m_controlStateCount;
};

std::unique_ptr<SavedFormState> SavedFormState::create() {
  return WTF::wrapUnique(new SavedFormState);
}

static bool isNotFormControlTypeCharacter(UChar ch) {
  return ch != '-' && (ch > 'z' || ch < 'a');
}

std::unique_ptr<SavedFormState> SavedFormState::deserialize(
    const Vector<String>& stateVector,
    size_t& index) {
  if (index >= stateVector.size())
    return nullptr;
  // FIXME: We need String::toSizeT().
  size_t itemCount = stateVector[index++].toUInt();
  if (!itemCount)
    return nullptr;
  std::unique_ptr<SavedFormState> savedFormState =
      WTF::wrapUnique(new SavedFormState);
  while (itemCount--) {
    if (index + 1 >= stateVector.size())
      return nullptr;
    String name = stateVector[index++];
    String type = stateVector[index++];
    FormControlState state = FormControlState::deserialize(stateVector, index);
    if (type.isEmpty() ||
        type.find(isNotFormControlTypeCharacter) != kNotFound ||
        state.isFailure())
      return nullptr;
    savedFormState->appendControlState(AtomicString(name), AtomicString(type),
                                       state);
  }
  return savedFormState;
}

void SavedFormState::serializeTo(Vector<String>& stateVector) const {
  stateVector.push_back(String::number(m_controlStateCount));
  for (const auto& formControl : m_stateForNewFormElements) {
    const FormElementKey& key = formControl.key;
    const Deque<FormControlState>& queue = formControl.value;
    for (const FormControlState& formControlState : queue) {
      stateVector.push_back(key.name());
      stateVector.push_back(key.type());
      formControlState.serializeTo(stateVector);
    }
  }
}

void SavedFormState::appendControlState(const AtomicString& name,
                                        const AtomicString& type,
                                        const FormControlState& state) {
  FormElementKey key(name.impl(), type.impl());
  FormElementStateMap::iterator it = m_stateForNewFormElements.find(key);
  if (it != m_stateForNewFormElements.end()) {
    it->value.append(state);
  } else {
    Deque<FormControlState> stateList;
    stateList.append(state);
    m_stateForNewFormElements.set(key, stateList);
  }
  m_controlStateCount++;
}

FormControlState SavedFormState::takeControlState(const AtomicString& name,
                                                  const AtomicString& type) {
  if (m_stateForNewFormElements.isEmpty())
    return FormControlState();
  FormElementStateMap::iterator it =
      m_stateForNewFormElements.find(FormElementKey(name.impl(), type.impl()));
  if (it == m_stateForNewFormElements.end())
    return FormControlState();
  DCHECK_GT(it->value.size(), 0u);
  FormControlState state = it->value.takeFirst();
  m_controlStateCount--;
  if (!it->value.size())
    m_stateForNewFormElements.remove(it);
  return state;
}

Vector<String> SavedFormState::getReferencedFilePaths() const {
  Vector<String> toReturn;
  for (const auto& formControl : m_stateForNewFormElements) {
    const FormElementKey& key = formControl.key;
    if (!equal(key.type(), "file", 4))
      continue;
    const Deque<FormControlState>& queue = formControl.value;
    for (const FormControlState& formControlState : queue) {
      const Vector<FileChooserFileInfo>& selectedFiles =
          HTMLInputElement::filesFromFileInputFormControlState(
              formControlState);
      for (const auto& file : selectedFiles)
        toReturn.push_back(file.path);
    }
  }
  return toReturn;
}

// ----------------------------------------------------------------------------

class FormKeyGenerator final
    : public GarbageCollectedFinalized<FormKeyGenerator> {
  WTF_MAKE_NONCOPYABLE(FormKeyGenerator);

 public:
  static FormKeyGenerator* create() { return new FormKeyGenerator; }
  DEFINE_INLINE_TRACE() { visitor->trace(m_formToKeyMap); }
  const AtomicString& formKey(const HTMLFormControlElementWithState&);
  void willDeleteForm(HTMLFormElement*);

 private:
  FormKeyGenerator() {}

  using FormToKeyMap = HeapHashMap<Member<HTMLFormElement>, AtomicString>;
  using FormSignatureToNextIndexMap = HashMap<String, unsigned>;
  FormToKeyMap m_formToKeyMap;
  FormSignatureToNextIndexMap m_formSignatureToNextIndexMap;
};

static inline void recordFormStructure(const HTMLFormElement& form,
                                       StringBuilder& builder) {
  // 2 is enough to distinguish forms in webkit.org/b/91209#c0
  const size_t namedControlsToBeRecorded = 2;
  const ListedElement::List& controls = form.listedElements();
  builder.append(" [");
  for (size_t i = 0, namedControls = 0;
       i < controls.size() && namedControls < namedControlsToBeRecorded; ++i) {
    if (!controls[i]->isFormControlElementWithState())
      continue;
    HTMLFormControlElementWithState* control =
        toHTMLFormControlElementWithState(controls[i]);
    if (!ownerFormForState(*control))
      continue;
    AtomicString name = control->name();
    if (name.isEmpty())
      continue;
    namedControls++;
    builder.append(name);
    builder.append(' ');
  }
  builder.append(']');
}

static inline String formSignature(const HTMLFormElement& form) {
  KURL actionURL = form.getURLAttribute(actionAttr);
  // Remove the query part because it might contain volatile parameters such
  // as a session key.
  if (!actionURL.isEmpty())
    actionURL.setQuery(String());

  StringBuilder builder;
  if (!actionURL.isEmpty())
    builder.append(actionURL.getString());

  recordFormStructure(form, builder);
  return builder.toString();
}

const AtomicString& FormKeyGenerator::formKey(
    const HTMLFormControlElementWithState& control) {
  HTMLFormElement* form = ownerFormForState(control);
  if (!form) {
    DEFINE_STATIC_LOCAL(const AtomicString, formKeyForNoOwner, ("No owner"));
    return formKeyForNoOwner;
  }
  FormToKeyMap::const_iterator it = m_formToKeyMap.find(form);
  if (it != m_formToKeyMap.end())
    return it->value;

  String signature = formSignature(*form);
  DCHECK(!signature.isNull());
  FormSignatureToNextIndexMap::AddResult result =
      m_formSignatureToNextIndexMap.insert(signature, 0);
  unsigned nextIndex = result.storedValue->value++;

  StringBuilder formKeyBuilder;
  formKeyBuilder.append(signature);
  formKeyBuilder.append(" #");
  formKeyBuilder.appendNumber(nextIndex);
  FormToKeyMap::AddResult addFormKeyresult =
      m_formToKeyMap.insert(form, formKeyBuilder.toAtomicString());
  return addFormKeyresult.storedValue->value;
}

void FormKeyGenerator::willDeleteForm(HTMLFormElement* form) {
  DCHECK(form);
  m_formToKeyMap.erase(form);
}

// ----------------------------------------------------------------------------

DocumentState* DocumentState::create() {
  return new DocumentState;
}

DEFINE_TRACE(DocumentState) {
  visitor->trace(m_formControls);
}

void DocumentState::addControl(HTMLFormControlElementWithState* control) {
  DCHECK(!m_formControls.contains(control));
  m_formControls.insert(control);
}

void DocumentState::removeControl(HTMLFormControlElementWithState* control) {
  CHECK(m_formControls.contains(control));
  m_formControls.erase(control);
}

static String formStateSignature() {
  // In the legacy version of serialized state, the first item was a name
  // attribute value of a form control. The following string literal should
  // contain some characters which are rarely used for name attribute values.
  DEFINE_STATIC_LOCAL(String, signature,
                      ("\n\r?% Blink serialized form state version 9 \n\r=&"));
  return signature;
}

Vector<String> DocumentState::toStateVector() {
  FormKeyGenerator* keyGenerator = FormKeyGenerator::create();
  std::unique_ptr<SavedFormStateMap> stateMap =
      WTF::wrapUnique(new SavedFormStateMap);
  for (const auto& formControl : m_formControls) {
    HTMLFormControlElementWithState* control = formControl.get();
    DCHECK(control->isConnected());
    if (!control->shouldSaveAndRestoreFormControlState())
      continue;
    SavedFormStateMap::AddResult result =
        stateMap->insert(keyGenerator->formKey(*control), nullptr);
    if (result.isNewEntry)
      result.storedValue->value = SavedFormState::create();
    result.storedValue->value->appendControlState(
        control->name(), control->type(), control->saveFormControlState());
  }

  Vector<String> stateVector;
  stateVector.reserveInitialCapacity(m_formControls.size() * 4);
  stateVector.push_back(formStateSignature());
  for (const auto& savedFormState : *stateMap) {
    stateVector.push_back(savedFormState.key);
    savedFormState.value->serializeTo(stateVector);
  }
  bool hasOnlySignature = stateVector.size() == 1;
  if (hasOnlySignature)
    stateVector.clear();
  return stateVector;
}

// ----------------------------------------------------------------------------

FormController::FormController() : m_documentState(DocumentState::create()) {}

FormController::~FormController() {}

DEFINE_TRACE(FormController) {
  visitor->trace(m_documentState);
  visitor->trace(m_formKeyGenerator);
}

DocumentState* FormController::formElementsState() const {
  return m_documentState.get();
}

void FormController::setStateForNewFormElements(
    const Vector<String>& stateVector) {
  formStatesFromStateVector(stateVector, m_savedFormStateMap);
}

bool FormController::hasFormStates() const {
  return !m_savedFormStateMap.isEmpty();
}

FormControlState FormController::takeStateForFormElement(
    const HTMLFormControlElementWithState& control) {
  if (m_savedFormStateMap.isEmpty())
    return FormControlState();
  if (!m_formKeyGenerator)
    m_formKeyGenerator = FormKeyGenerator::create();
  SavedFormStateMap::iterator it =
      m_savedFormStateMap.find(m_formKeyGenerator->formKey(control));
  if (it == m_savedFormStateMap.end())
    return FormControlState();
  FormControlState state =
      it->value->takeControlState(control.name(), control.type());
  if (it->value->isEmpty())
    m_savedFormStateMap.remove(it);
  return state;
}

void FormController::formStatesFromStateVector(
    const Vector<String>& stateVector,
    SavedFormStateMap& map) {
  map.clear();

  size_t i = 0;
  if (stateVector.size() < 1 || stateVector[i++] != formStateSignature())
    return;

  while (i + 1 < stateVector.size()) {
    AtomicString formKey = AtomicString(stateVector[i++]);
    std::unique_ptr<SavedFormState> state =
        SavedFormState::deserialize(stateVector, i);
    if (!state) {
      i = 0;
      break;
    }
    map.insert(formKey, std::move(state));
  }
  if (i != stateVector.size())
    map.clear();
}

void FormController::willDeleteForm(HTMLFormElement* form) {
  if (m_formKeyGenerator)
    m_formKeyGenerator->willDeleteForm(form);
}

void FormController::restoreControlStateFor(
    HTMLFormControlElementWithState& control) {
  // We don't save state of a control with
  // shouldSaveAndRestoreFormControlState() == false. But we need to skip
  // restoring process too because a control in another form might have the same
  // pair of name and type and saved its state.
  if (!control.shouldSaveAndRestoreFormControlState())
    return;
  if (ownerFormForState(control))
    return;
  FormControlState state = takeStateForFormElement(control);
  if (state.valueSize() > 0)
    control.restoreFormControlState(state);
}

void FormController::restoreControlStateIn(HTMLFormElement& form) {
  EventQueueScope scope;
  const ListedElement::List& elements = form.listedElements();
  for (const auto& element : elements) {
    if (!element->isFormControlElementWithState())
      continue;
    HTMLFormControlElementWithState* control =
        toHTMLFormControlElementWithState(element);
    if (!control->shouldSaveAndRestoreFormControlState())
      continue;
    if (ownerFormForState(*control) != &form)
      continue;
    FormControlState state = takeStateForFormElement(*control);
    if (state.valueSize() > 0) {
      // restoreFormControlState might dispatch input/change events.
      control->restoreFormControlState(state);
    }
  }
}

Vector<String> FormController::getReferencedFilePaths(
    const Vector<String>& stateVector) {
  Vector<String> toReturn;
  SavedFormStateMap map;
  formStatesFromStateVector(stateVector, map);
  for (const auto& savedFormState : map)
    toReturn.appendVector(savedFormState.value->getReferencedFilePaths());
  return toReturn;
}

void FormController::registerStatefulFormControl(
    HTMLFormControlElementWithState& control) {
  m_documentState->addControl(&control);
}

void FormController::unregisterStatefulFormControl(
    HTMLFormControlElementWithState& control) {
  m_documentState->removeControl(&control);
}

}  // namespace blink
