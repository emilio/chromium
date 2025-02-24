// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/union_container.cpp.tmpl

// clang-format off
#include "StringOrDouble.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ToV8.h"

namespace blink {

StringOrDouble::StringOrDouble() : m_type(SpecificTypeNone) {}

String StringOrDouble::getAsString() const {
  DCHECK(isString());
  return m_string;
}

void StringOrDouble::setString(String value) {
  DCHECK(isNull());
  m_string = value;
  m_type = SpecificTypeString;
}

StringOrDouble StringOrDouble::fromString(String value) {
  StringOrDouble container;
  container.setString(value);
  return container;
}

double StringOrDouble::getAsDouble() const {
  DCHECK(isDouble());
  return m_double;
}

void StringOrDouble::setDouble(double value) {
  DCHECK(isNull());
  m_double = value;
  m_type = SpecificTypeDouble;
}

StringOrDouble StringOrDouble::fromDouble(double value) {
  StringOrDouble container;
  container.setDouble(value);
  return container;
}

StringOrDouble::StringOrDouble(const StringOrDouble&) = default;
StringOrDouble::~StringOrDouble() = default;
StringOrDouble& StringOrDouble::operator=(const StringOrDouble&) = default;

DEFINE_TRACE(StringOrDouble) {
}

void V8StringOrDouble::toImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, StringOrDouble& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::Nullable && isUndefinedOrNull(v8Value))
    return;

  if (v8Value->IsNumber()) {
    double cppValue = NativeValueTraits<IDLDouble>::nativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.hadException())
      return;
    impl.setDouble(cppValue);
    return;
  }

  {
    V8StringResource<> cppValue = v8Value;
    if (!cppValue.prepare(exceptionState))
      return;
    impl.setString(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const StringOrDouble& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.m_type) {
    case StringOrDouble::SpecificTypeNone:
      return v8::Null(isolate);
    case StringOrDouble::SpecificTypeString:
      return v8String(isolate, impl.getAsString());
    case StringOrDouble::SpecificTypeDouble:
      return v8::Number::New(isolate, impl.getAsDouble());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

StringOrDouble NativeValueTraits<StringOrDouble>::nativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  StringOrDouble impl;
  V8StringOrDouble::toImpl(isolate, value, impl, UnionTypeConversionMode::NotNullable, exceptionState);
  return impl;
}

}  // namespace blink
