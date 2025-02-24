// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/interface.cpp.tmpl

// clang-format off
#include "V8TestInterfaceNamedConstructor2.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "wtf/GetPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceNamedConstructor2::wrapperTypeInfo = { gin::kEmbedderBlink, V8TestInterfaceNamedConstructor2::domTemplate, V8TestInterfaceNamedConstructor2::trace, V8TestInterfaceNamedConstructor2::traceWrappers, nullptr, "TestInterfaceNamedConstructor2", 0, WrapperTypeInfo::WrapperTypeObjectPrototype, WrapperTypeInfo::ObjectClassId, WrapperTypeInfo::NotInheritFromActiveScriptWrappable, WrapperTypeInfo::Independent };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceNamedConstructor2.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// bindings/core/v8/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceNamedConstructor2::s_wrapperTypeInfo = V8TestInterfaceNamedConstructor2::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceNamedConstructor2>::value,
    "TestInterfaceNamedConstructor2 inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceNamedConstructor2::hasPendingActivity),
                 decltype(&ScriptWrappable::hasPendingActivity)>::value,
    "TestInterfaceNamedConstructor2 is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace TestInterfaceNamedConstructor2V8Internal {

} // namespace TestInterfaceNamedConstructor2V8Internal

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceNamedConstructor2Constructor::wrapperTypeInfo = { gin::kEmbedderBlink, V8TestInterfaceNamedConstructor2Constructor::domTemplate, V8TestInterfaceNamedConstructor2::trace, V8TestInterfaceNamedConstructor2::traceWrappers, nullptr, "TestInterfaceNamedConstructor2", 0, WrapperTypeInfo::WrapperTypeObjectPrototype, WrapperTypeInfo::ObjectClassId, WrapperTypeInfo::NotInheritFromActiveScriptWrappable, WrapperTypeInfo::Independent };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

static void V8TestInterfaceNamedConstructor2ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (!info.IsConstructCall()) {
    V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::constructorNotCallableAsFunction("Audio"));
    return;
  }

  if (ConstructorMode::current(info.GetIsolate()) == ConstructorMode::WrapExistingObject) {
    v8SetReturnValue(info, info.Holder());
    return;
  }

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToConstruct("TestInterfaceNamedConstructor2", ExceptionMessages::notEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.prepare())
    return;

  TestInterfaceNamedConstructor2* impl = TestInterfaceNamedConstructor2::createForJSConstructor(stringArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->associateWithWrapper(info.GetIsolate(), &V8TestInterfaceNamedConstructor2Constructor::wrapperTypeInfo, wrapper);
  v8SetReturnValue(info, wrapper);
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceNamedConstructor2Constructor::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::from(isolate);
  v8::Local<v8::FunctionTemplate> result = data->findInterfaceTemplate(world, &domTemplateKey);
  if (!result.IsEmpty())
    return result;

  result = v8::FunctionTemplate::New(isolate, V8TestInterfaceNamedConstructor2ConstructorCallback);
  v8::Local<v8::ObjectTemplate> instanceTemplate = result->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(V8TestInterfaceNamedConstructor2::internalFieldCount);
  result->SetClassName(v8AtomicString(isolate, "Audio"));
  result->Inherit(V8TestInterfaceNamedConstructor2::domTemplate(isolate, world));
  data->setInterfaceTemplate(world, &domTemplateKey, result);
  return result;
}

void V8TestInterfaceNamedConstructor2Constructor::NamedConstructorAttributeGetter(
    v8::Local<v8::Name> propertyName,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Context> creationContext = info.Holder()->CreationContext();
  V8PerContextData* perContextData = V8PerContextData::from(creationContext);
  if (!perContextData) {
    // TODO(yukishiino): Return a valid named constructor even after the context is detached
    return;
  }

  v8::Local<v8::Function> namedConstructor = perContextData->constructorForType(&V8TestInterfaceNamedConstructor2Constructor::wrapperTypeInfo);

  // Set the prototype of named constructors to the regular constructor.
  auto privateProperty = V8PrivateProperty::getNamedConstructorInitialized(info.GetIsolate());
  v8::Local<v8::Context> currentContext = info.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Value> privateValue = privateProperty.get(currentContext, namedConstructor);

  if (privateValue.IsEmpty()) {
    v8::Local<v8::Function> interface = perContextData->constructorForType(&V8TestInterfaceNamedConstructor2::wrapperTypeInfo);
    v8::Local<v8::Value> interfacePrototype = interface->Get(currentContext, v8AtomicString(info.GetIsolate(), "prototype")).ToLocalChecked();
    bool result = namedConstructor->Set(currentContext, v8AtomicString(info.GetIsolate(), "prototype"), interfacePrototype).ToChecked();
    if (!result)
      return;
    privateProperty.set(currentContext, namedConstructor, v8::True(info.GetIsolate()));
  }

  v8SetReturnValue(info, namedConstructor);
}

static void installV8TestInterfaceNamedConstructor2Template(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::initializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceNamedConstructor2::wrapperTypeInfo.interfaceName, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceNamedConstructor2::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register DOM constants, attributes and operations.
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceNamedConstructor2::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::domClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceNamedConstructor2Template);
}

bool V8TestInterfaceNamedConstructor2::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::from(isolate)->hasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceNamedConstructor2::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::from(isolate)->findInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceNamedConstructor2* V8TestInterfaceNamedConstructor2::toImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? toImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceNamedConstructor2* NativeValueTraits<TestInterfaceNamedConstructor2>::nativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  return V8TestInterfaceNamedConstructor2::toImplWithTypeCheck(isolate, value);
}

}  // namespace blink
