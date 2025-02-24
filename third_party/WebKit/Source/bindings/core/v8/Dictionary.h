/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Dictionary_h
#define Dictionary_h

#include "bindings/core/v8/DictionaryIterator.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/CoreExport.h"
#include "v8/include/v8.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/StringView.h"

namespace blink {

class ExceptionState;
class ExecutionContext;

// Dictionary class provides ways to retrieve property values as C++ objects
// from a V8 object. Instances of this class must not outlive V8's handle scope
// because they hold a V8 value without putting it on persistent handles.
class CORE_EXPORT Dictionary final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  Dictionary() : m_isolate(nullptr) {}
  Dictionary(v8::Isolate*,
             v8::Local<v8::Value> dictionaryObject,
             ExceptionState&);

  Dictionary& operator=(const Dictionary&) = default;

  bool isObject() const { return !m_dictionaryObject.IsEmpty(); }
  bool isUndefinedOrNull() const { return !isObject(); }

  v8::Local<v8::Value> v8Value() const {
    if (!m_isolate)
      return v8::Local<v8::Value>();
    switch (m_valueType) {
      case ValueType::Undefined:
        return v8::Undefined(m_isolate);
      case ValueType::Null:
        return v8::Null(m_isolate);
      case ValueType::Object:
        return m_dictionaryObject;
      default:
        NOTREACHED();
        return v8::Local<v8::Value>();
    }
  }

  bool get(const StringView& key, v8::Local<v8::Value>& value) const {
    return m_isolate && getInternal(v8String(m_isolate, key), value);
  }
  bool get(const StringView& key, Dictionary&) const;

  HashMap<String, String> getOwnPropertiesAsStringHashMap(
      ExceptionState&) const;
  Vector<String> getPropertyNames(ExceptionState&) const;

  bool hasProperty(const StringView& key, ExceptionState&) const;

  v8::Isolate* isolate() const { return m_isolate; }
  v8::Local<v8::Context> v8Context() const {
    ASSERT(m_isolate);
    return m_isolate->GetCurrentContext();
  }

  DictionaryIterator getIterator(ExecutionContext*) const;

 private:
  bool getInternal(const v8::Local<v8::Value>& key,
                   v8::Local<v8::Value>& result) const;

  v8::Isolate* m_isolate;
  // Undefined, Null, or Object is allowed as type of dictionary.
  enum class ValueType {
    Undefined,
    Null,
    Object
  } m_valueType = ValueType::Undefined;
  v8::Local<v8::Object> m_dictionaryObject;  // an Object or empty
};

template <>
struct NativeValueTraits<Dictionary>
    : public NativeValueTraitsBase<Dictionary> {
  static Dictionary nativeValue(v8::Isolate* isolate,
                                v8::Local<v8::Value> value,
                                ExceptionState& exceptionState) {
    return Dictionary(isolate, value, exceptionState);
  }
};

// DictionaryHelper is a collection of static methods for getting or
// converting a value from Dictionary.
struct DictionaryHelper {
  STATIC_ONLY(DictionaryHelper);
  template <typename T>
  static bool get(const Dictionary&, const StringView& key, T& value);
  template <typename T>
  static bool get(const Dictionary&,
                  const StringView& key,
                  T& value,
                  bool& hasValue);
  template <typename T>
  static bool get(const Dictionary&,
                  const StringView& key,
                  T& value,
                  ExceptionState&);
  template <typename T>
  static bool getWithUndefinedOrNullCheck(const Dictionary& dictionary,
                                          const StringView& key,
                                          T& value) {
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value) || isUndefinedOrNull(v8Value))
      return false;
    return DictionaryHelper::get(dictionary, key, value);
  }
  template <template <typename> class PointerType, typename T>
  static bool get(const Dictionary&,
                  const StringView& key,
                  PointerType<T>& value);
  template <typename T>
  static bool get(const Dictionary&, const StringView& key, Nullable<T>& value);
};

}  // namespace blink

#endif  // Dictionary_h
