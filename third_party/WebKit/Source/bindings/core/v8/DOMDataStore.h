/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef DOMDataStore_h
#define DOMDataStore_h

#include <memory>

#include "bindings/core/v8/DOMWrapperMap.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "v8/include/v8.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/Optional.h"
#include "wtf/StackUtil.h"
#include "wtf/StdLibExtras.h"

namespace blink {

class DOMDataStore {
  WTF_MAKE_NONCOPYABLE(DOMDataStore);
  USING_FAST_MALLOC(DOMDataStore);

 public:
  DOMDataStore(v8::Isolate* isolate, bool isMainWorld)
      : m_isMainWorld(isMainWorld) {
    // We never use |m_wrapperMap| when it's the main world.
    if (!isMainWorld)
      m_wrapperMap.emplace(isolate);
  }

  static DOMDataStore& current(v8::Isolate* isolate) {
    return DOMWrapperWorld::current(isolate).domDataStore();
  }

  static bool setReturnValue(v8::ReturnValue<v8::Value> returnValue,
                             ScriptWrappable* object) {
    if (canUseMainWorldWrapper())
      return object->setReturnValue(returnValue);
    return current(returnValue.GetIsolate())
        .setReturnValueFrom(returnValue, object);
  }

  static bool setReturnValueForMainWorld(v8::ReturnValue<v8::Value> returnValue,
                                         ScriptWrappable* object) {
    return object->setReturnValue(returnValue);
  }

  static bool setReturnValueFast(v8::ReturnValue<v8::Value> returnValue,
                                 ScriptWrappable* object,
                                 v8::Local<v8::Object> holder,
                                 const ScriptWrappable* wrappable) {
    if (canUseMainWorldWrapper()
        // The second fastest way to check if we're in the main world is to
        // check if the wrappable's wrapper is the same as the holder.
        || holderContainsWrapper(holder, wrappable))
      return object->setReturnValue(returnValue);
    return current(returnValue.GetIsolate())
        .setReturnValueFrom(returnValue, object);
  }

  static v8::Local<v8::Object> getWrapper(ScriptWrappable* object,
                                          v8::Isolate* isolate) {
    if (canUseMainWorldWrapper())
      return object->mainWorldWrapper(isolate);
    return current(isolate).get(object, isolate);
  }

  // Associates the given |object| with the given |wrapper| if the object is
  // not yet associated with any wrapper.  Returns true if the given wrapper
  // is associated with the object, or false if the object is already
  // associated with a wrapper.  In the latter case, |wrapper| will be updated
  // to the existing wrapper.
  WARN_UNUSED_RESULT static bool setWrapper(
      v8::Isolate* isolate,
      ScriptWrappable* object,
      const WrapperTypeInfo* wrapperTypeInfo,
      v8::Local<v8::Object>& wrapper) {
    if (canUseMainWorldWrapper())
      return object->setWrapper(isolate, wrapperTypeInfo, wrapper);
    return current(isolate).set(isolate, object, wrapperTypeInfo, wrapper);
  }

  static bool containsWrapper(ScriptWrappable* object, v8::Isolate* isolate) {
    return current(isolate).containsWrapper(object);
  }

  v8::Local<v8::Object> get(ScriptWrappable* object, v8::Isolate* isolate) {
    if (m_isMainWorld)
      return object->mainWorldWrapper(isolate);
    return m_wrapperMap->newLocal(isolate, object);
  }

  void markWrapper(ScriptWrappable* scriptWrappable) {
    m_wrapperMap->markWrapper(scriptWrappable);
  }

  bool setReturnValueFrom(v8::ReturnValue<v8::Value> returnValue,
                          ScriptWrappable* object) {
    if (m_isMainWorld)
      return object->setReturnValue(returnValue);
    return m_wrapperMap->setReturnValueFrom(returnValue, object);
  }

  bool containsWrapper(ScriptWrappable* object) {
    if (m_isMainWorld)
      return object->containsWrapper();
    return m_wrapperMap->containsKey(object);
  }

 private:
  WARN_UNUSED_RESULT bool set(v8::Isolate* isolate,
                              ScriptWrappable* object,
                              const WrapperTypeInfo* wrapperTypeInfo,
                              v8::Local<v8::Object>& wrapper) {
    ASSERT(object);
    ASSERT(!wrapper.IsEmpty());
    if (m_isMainWorld)
      return object->setWrapper(isolate, wrapperTypeInfo, wrapper);
    return m_wrapperMap->set(object, wrapperTypeInfo, wrapper);
  }

  // We can use a wrapper stored in a ScriptWrappable when we're in the main
  // world.  This method does the fast check if we're in the main world. If this
  // method returns true, it is guaranteed that we're in the main world. On the
  // other hand, if this method returns false, nothing is guaranteed (we might
  // be in the main world).
  static bool canUseMainWorldWrapper() {
    return !WTF::mayNotBeMainThread() &&
           !DOMWrapperWorld::nonMainWorldsInMainThread();
  }

  static bool holderContainsWrapper(v8::Local<v8::Object> holder,
                                    const ScriptWrappable* wrappable) {
    // Verify our assumptions about the main world.
    ASSERT(wrappable);
    ASSERT(!wrappable->containsWrapper() || !wrappable->isEqualTo(holder) ||
           current(v8::Isolate::GetCurrent()).m_isMainWorld);
    return wrappable->isEqualTo(holder);
  }

  bool m_isMainWorld;
  WTF::Optional<DOMWrapperMap<ScriptWrappable>> m_wrapperMap;
};

template <>
inline void DOMWrapperMap<ScriptWrappable>::PersistentValueMapTraits::Dispose(
    v8::Isolate*,
    v8::Global<v8::Object> value,
    ScriptWrappable*) {
  toWrapperTypeInfo(value)->wrapperDestroyed();
}

template <>
inline void
DOMWrapperMap<ScriptWrappable>::PersistentValueMapTraits::DisposeWeak(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  auto wrapperTypeInfo = reinterpret_cast<WrapperTypeInfo*>(
      data.GetInternalField(v8DOMWrapperTypeIndex));
  wrapperTypeInfo->wrapperDestroyed();
}

}  // namespace blink

#endif  // DOMDataStore_h
