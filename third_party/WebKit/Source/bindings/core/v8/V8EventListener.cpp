/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8EventListener.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"

namespace blink {

V8EventListener::V8EventListener(bool isAttribute, ScriptState* scriptState)
    : V8AbstractEventListener(isAttribute,
                              scriptState->world(),
                              scriptState->isolate()) {}

v8::Local<v8::Function> V8EventListener::getListenerFunction(
    ScriptState* scriptState) {
  v8::Local<v8::Object> listener =
      getListenerObject(scriptState->getExecutionContext());

  // Has the listener been disposed?
  if (listener.IsEmpty())
    return v8::Local<v8::Function>();

  if (listener->IsFunction())
    return v8::Local<v8::Function>::Cast(listener);

  // The EventHandler callback function type (used for event handler
  // attributes in HTML) has [TreatNonObjectAsNull], which implies that
  // non-function objects should be treated as no-op functions that return
  // undefined.
  if (isAttribute())
    return v8::Local<v8::Function>();

  // Getting the handleEvent property can runs script in the getter.
  if (ScriptForbiddenScope::isScriptForbidden()) {
    V8ThrowException::throwError(isolate(), "Script execution is forbidden.");
    return v8::Local<v8::Function>();
  }

  if (listener->IsObject()) {
    // Check that no exceptions were thrown when getting the
    // handleEvent property and that the value is a function.
    v8::Local<v8::Value> property;
    if (listener
            ->Get(scriptState->context(),
                  v8AtomicString(isolate(), "handleEvent"))
            .ToLocal(&property) &&
        property->IsFunction())
      return v8::Local<v8::Function>::Cast(property);
  }

  return v8::Local<v8::Function>();
}

v8::Local<v8::Value> V8EventListener::callListenerFunction(
    ScriptState* scriptState,
    v8::Local<v8::Value> jsEvent,
    Event* event) {
  ASSERT(!jsEvent.IsEmpty());
  v8::Local<v8::Function> handlerFunction = getListenerFunction(scriptState);
  v8::Local<v8::Object> receiver = getReceiverObject(scriptState, event);
  if (handlerFunction.IsEmpty() || receiver.IsEmpty())
    return v8::Local<v8::Value>();

  if (!scriptState->getExecutionContext()->isDocument())
    return v8::Local<v8::Value>();

  LocalFrame* frame = toDocument(scriptState->getExecutionContext())->frame();
  if (!frame)
    return v8::Local<v8::Value>();

  // TODO(jochen): Consider moving this check into canExecuteScripts.
  // http://crbug.com/608641
  if (scriptState->world().isMainWorld() &&
      !scriptState->getExecutionContext()->canExecuteScripts(
          AboutToExecuteScript))
    return v8::Local<v8::Value>();

  v8::Local<v8::Value> parameters[1] = {jsEvent};
  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::callFunction(handlerFunction, frame->document(),
                                    receiver, WTF_ARRAY_LENGTH(parameters),
                                    parameters, scriptState->isolate())
           .ToLocal(&result))
    return v8::Local<v8::Value>();
  return result;
}

}  // namespace blink
