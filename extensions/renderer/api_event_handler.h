// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_EVENT_HANDLER_H_
#define EXTENSIONS_RENDERER_API_EVENT_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/renderer/api_binding_types.h"
#include "extensions/renderer/event_emitter.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace extensions {

// The object to handle API events. This includes vending v8::Objects for the
// event; handling adding, removing, and querying listeners; and firing events
// to subscribed listeners. Designed to be used across JS contexts, but on a
// single thread.
class APIEventHandler {
 public:
  using EventListenersChangedMethod =
      base::Callback<void(const std::string& event_name,
                          binding::EventListenersChanged,
                          v8::Local<v8::Context>)>;

  APIEventHandler(const binding::RunJSFunction& call_js,
                  const EventListenersChangedMethod& listeners_changed);
  ~APIEventHandler();

  // Returns a new v8::Object for an event with the given |event_name|.
  v8::Local<v8::Object> CreateEventInstance(const std::string& event_name,
                                            v8::Local<v8::Context> context);

  // Notifies all listeners of the event with the given |event_name| in the
  // specified |context|, sending the included |arguments|.
  void FireEventInContext(const std::string& event_name,
                          v8::Local<v8::Context> context,
                          const base::ListValue& arguments);

  // Registers a |function| to serve as an "argument massager" for the given
  // |event_name|, mutating the original arguments.
  // The function is called with two arguments: the array of original arguments
  // being dispatched to the event, and the function to dispatch the event to
  // listeners.
  void RegisterArgumentMassager(v8::Local<v8::Context> context,
                                const std::string& event_name,
                                v8::Local<v8::Function> function);

  // Returns the EventListeners for a given |event_name| and |context|.
  size_t GetNumEventListenersForTesting(const std::string& event_name,
                                        v8::Local<v8::Context> context);

  // Invalidates listeners for the given |context|. It's a shame we have to
  // have this separately (as opposed to hooking into e.g. a PerContextData
  // destructor), but we need to do this before the context is fully removed
  // (because the associated extension ScriptContext needs to be valid).
  void InvalidateContext(v8::Local<v8::Context> context);

 private:
  // Method to run a given v8::Function. Curried in for testing.
  binding::RunJSFunction call_js_;

  EventListenersChangedMethod listeners_changed_;

  DISALLOW_COPY_AND_ASSIGN(APIEventHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_EVENT_HANDLER_H_
