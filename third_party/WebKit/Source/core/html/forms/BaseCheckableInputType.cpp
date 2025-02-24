/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/forms/BaseCheckableInputType.h"

#include "core/HTMLNames.h"
#include "core/events/KeyboardEvent.h"
#include "core/html/FormData.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/FormController.h"

namespace blink {

using namespace HTMLNames;

DEFINE_TRACE(BaseCheckableInputType) {
  InputTypeView::trace(visitor);
  InputType::trace(visitor);
}

InputTypeView* BaseCheckableInputType::createView() {
  return this;
}

FormControlState BaseCheckableInputType::saveFormControlState() const {
  return FormControlState(element().checked() ? "on" : "off");
}

void BaseCheckableInputType::restoreFormControlState(
    const FormControlState& state) {
  element().setChecked(state[0] == "on");
}

void BaseCheckableInputType::appendToFormData(FormData& formData) const {
  if (element().checked())
    formData.append(element().name(), element().value());
}

void BaseCheckableInputType::handleKeydownEvent(KeyboardEvent* event) {
  const String& key = event->key();
  if (key == " ") {
    element().setActive(true);
    // No setDefaultHandled(), because IE dispatches a keypress in this case
    // and the caller will only dispatch a keypress if we don't call
    // setDefaultHandled().
  }
}

void BaseCheckableInputType::handleKeypressEvent(KeyboardEvent* event) {
  if (event->charCode() == ' ') {
    // Prevent scrolling down the page.
    event->setDefaultHandled();
  }
}

bool BaseCheckableInputType::canSetStringValue() const {
  return false;
}

// FIXME: Could share this with KeyboardClickableInputTypeView and
// RangeInputType if we had a common base class.
void BaseCheckableInputType::accessKeyAction(bool sendMouseEvents) {
  InputTypeView::accessKeyAction(sendMouseEvents);

  element().dispatchSimulatedClick(
      0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

bool BaseCheckableInputType::matchesDefaultPseudoClass() {
  return element().fastHasAttribute(checkedAttr);
}

InputType::ValueMode BaseCheckableInputType::valueMode() const {
  return ValueMode::kDefaultOn;
}

void BaseCheckableInputType::setValue(const String& sanitizedValue,
                                      bool,
                                      TextFieldEventBehavior,
                                      TextControlSetValueSelection) {
  element().setAttribute(valueAttr, AtomicString(sanitizedValue));
}

void BaseCheckableInputType::readingChecked() const {
  if (m_isInClickHandler)
    UseCounter::count(element().document(),
                      UseCounter::ReadingCheckedInClickHandler);
}

bool BaseCheckableInputType::isCheckable() {
  return true;
}

bool BaseCheckableInputType::shouldDispatchFormControlChangeEvent(
    String& oldValue,
    String& newValue) {
  return oldValue != newValue;
}

}  // namespace blink
