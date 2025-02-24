// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_API_BINDINGS_SYSTEM_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/renderer/api_binding.h"
#include "extensions/renderer/api_binding_types.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/api_last_error.h"
#include "extensions/renderer/api_request_handler.h"
#include "extensions/renderer/api_type_reference_map.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class APIBindingHooks;

// A class encompassing the necessary pieces to construct the JS entry points
// for Extension APIs. Designed to be used on a single thread, but safe between
// multiple v8::Contexts.
class APIBindingsSystem {
 public:
  using GetAPISchemaMethod =
      base::Callback<const base::DictionaryValue&(const std::string&)>;
  using CustomTypeHandler =
      base::Callback<v8::Local<v8::Object>(v8::Local<v8::Context> context,
                                           const std::string& property_name,
                                           APIRequestHandler* request_handler,
                                           APIEventHandler* event_handler,
                                           APITypeReferenceMap* type_refs)>;

  APIBindingsSystem(const binding::RunJSFunction& call_js,
                    const binding::RunJSFunctionSync& call_js_sync,
                    const GetAPISchemaMethod& get_api_schema,
                    const APIRequestHandler::SendRequestMethod& send_request,
                    const APIEventHandler::EventListenersChangedMethod&
                        event_listeners_changed,
                    APILastError last_error);
  ~APIBindingsSystem();

  // Returns a new v8::Object representing the api specified by |api_name|.
  v8::Local<v8::Object> CreateAPIInstance(
      const std::string& api_name,
      v8::Local<v8::Context> context,
      v8::Isolate* isolate,
      const APIBinding::AvailabilityCallback& is_available,
      APIBindingHooks** hooks_out);

  // Responds to the request with the given |request_id|, calling the callback
  // with |response|. If |error| is non-empty, sets the last error.
  void CompleteRequest(int request_id,
                       const base::ListValue& response,
                       const std::string& error);

  // Notifies the APIEventHandler to fire the corresponding event, notifying
  // listeners.
  void FireEventInContext(const std::string& event_name,
                          v8::Local<v8::Context> context,
                          const base::ListValue& response);

  // Returns the APIBindingHooks object for the given api to allow for
  // registering custom hooks. These must be registered *before* the
  // binding is instantiated.
  // TODO(devlin): It's a little weird that we don't just expose a
  // RegisterHooks-type method. Depending on how complex the hook interface
  // is, maybe we should rethink this. Downside would be that it's less
  // efficient to register multiple hooks for the same API.
  APIBindingHooks* GetHooksForAPI(const std::string& api_name);

  // Registers the handler for creating a custom type with the given
  // |type_name|, where |type_name| is the fully-qualified type (e.g.
  // storage.StorageArea).
  void RegisterCustomType(const std::string& type_name,
                          const CustomTypeHandler& function);

  // Handles any cleanup necessary before releasing the given |context|.
  void WillReleaseContext(v8::Local<v8::Context> context);

  APIRequestHandler* request_handler() { return &request_handler_; }
  APIEventHandler* event_handler() { return &event_handler_; }
  APITypeReferenceMap* type_reference_map() { return &type_reference_map_; }

 private:
  // Creates a new APIBinding for the given |api_name|.
  std::unique_ptr<APIBinding> CreateNewAPIBinding(const std::string& api_name);

  // Callback for the APITypeReferenceMap in order to initialize an unknown
  // type.
  void InitializeType(const std::string& name);

  // Handles creating the type for the specified property.
  v8::Local<v8::Object> CreateCustomType(v8::Local<v8::Context> context,
                                         const std::string& type_name,
                                         const std::string& property_name);

  // The map of cached API reference types.
  APITypeReferenceMap type_reference_map_;

  // The request handler associated with the system.
  APIRequestHandler request_handler_;

  // The event handler associated with the system.
  APIEventHandler event_handler_;

  // A map from api_name -> APIBinding for constructed APIs. APIBindings are
  // created lazily.
  std::map<std::string, std::unique_ptr<APIBinding>> api_bindings_;

  // A map from api_name -> APIBindingHooks for registering custom hooks.
  // TODO(devlin): This map is pretty pointer-y. Is that going to be a
  // performance concern?
  std::map<std::string, std::unique_ptr<APIBindingHooks>> binding_hooks_;

  std::map<std::string, CustomTypeHandler> custom_types_;

  binding::RunJSFunction call_js_;

  binding::RunJSFunctionSync call_js_sync_;

  // The method to retrieve the DictionaryValue describing a given extension
  // API. Curried in for testing purposes so we can use fake APIs.
  GetAPISchemaMethod get_api_schema_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingsSystem);
};

}  // namespace

#endif  // EXTENSIONS_RENDERER_API_BINDINGS_SYSTEM_H_
