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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "modules/indexeddb/IDBFactory.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IndexedDBClient.h"
#include "platform/Histogram.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "public/platform/modules/indexeddb/WebIDBFactory.h"
#include <memory>

namespace blink {

static const char permissionDeniedErrorMessage[] =
    "The user denied permission to access the database.";

IDBFactory::IDBFactory() {}

static bool isContextValid(ExecutionContext* context) {
  ASSERT(context->isDocument() || context->isWorkerGlobalScope());
  if (context->isDocument()) {
    Document* document = toDocument(context);
    return document->frame() && document->page();
  }
  return true;
}

IDBRequest* IDBFactory::getDatabaseNames(ScriptState* scriptState,
                                         ExceptionState& exceptionState) {
  IDB_TRACE("IDBFactory::getDatabaseNames");
  if (!isContextValid(scriptState->getExecutionContext()))
    return nullptr;
  if (!scriptState->getExecutionContext()
           ->getSecurityOrigin()
           ->canAccessDatabase()) {
    exceptionState.throwSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  IDBRequest* request =
      IDBRequest::create(scriptState, IDBAny::createNull(), nullptr);

  if (!IndexedDBClient::from(scriptState->getExecutionContext())
           ->allowIndexedDB(scriptState->getExecutionContext(),
                            "Database Listing")) {
    request->onError(
        DOMException::create(UnknownError, permissionDeniedErrorMessage));
    return request;
  }

  Platform::current()->idbFactory()->getDatabaseNames(
      request->createWebCallbacks().release(),
      WebSecurityOrigin(
          scriptState->getExecutionContext()->getSecurityOrigin()));
  return request;
}

IDBOpenDBRequest* IDBFactory::open(ScriptState* scriptState,
                                   const String& name,
                                   unsigned long long version,
                                   ExceptionState& exceptionState) {
  IDB_TRACE("IDBFactory::open");
  if (!version) {
    exceptionState.throwTypeError("The version provided must not be 0.");
    return nullptr;
  }
  return openInternal(scriptState, name, version, exceptionState);
}

IDBOpenDBRequest* IDBFactory::openInternal(ScriptState* scriptState,
                                           const String& name,
                                           int64_t version,
                                           ExceptionState& exceptionState) {
  IDBDatabase::recordApiCallsHistogram(IDBOpenCall);
  ASSERT(version >= 1 || version == IDBDatabaseMetadata::NoVersion);
  if (!isContextValid(scriptState->getExecutionContext()))
    return nullptr;
  if (!scriptState->getExecutionContext()
           ->getSecurityOrigin()
           ->canAccessDatabase()) {
    exceptionState.throwSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  IDBDatabaseCallbacks* databaseCallbacks = IDBDatabaseCallbacks::create();
  int64_t transactionId = IDBDatabase::nextTransactionId();
  IDBOpenDBRequest* request = IDBOpenDBRequest::create(
      scriptState, databaseCallbacks, transactionId, version);

  if (!IndexedDBClient::from(scriptState->getExecutionContext())
           ->allowIndexedDB(scriptState->getExecutionContext(), name)) {
    request->onError(
        DOMException::create(UnknownError, permissionDeniedErrorMessage));
    return request;
  }

  Platform::current()->idbFactory()->open(
      name, version, transactionId, request->createWebCallbacks().release(),
      databaseCallbacks->createWebCallbacks().release(),
      WebSecurityOrigin(
          scriptState->getExecutionContext()->getSecurityOrigin()));
  return request;
}

IDBOpenDBRequest* IDBFactory::open(ScriptState* scriptState,
                                   const String& name,
                                   ExceptionState& exceptionState) {
  IDB_TRACE("IDBFactory::open");
  return openInternal(scriptState, name, IDBDatabaseMetadata::NoVersion,
                      exceptionState);
}

IDBOpenDBRequest* IDBFactory::deleteDatabase(ScriptState* scriptState,
                                             const String& name,
                                             ExceptionState& exceptionState) {
  return deleteDatabaseInternal(scriptState, name, exceptionState,
                                false /* force_close */);
}

IDBOpenDBRequest* IDBFactory::closeConnectionsAndDeleteDatabase(
    ScriptState* scriptState,
    const String& name,
    ExceptionState& exceptionState) {
  return deleteDatabaseInternal(scriptState, name, exceptionState,
                                true /* force_close */);
}

IDBOpenDBRequest* IDBFactory::deleteDatabaseInternal(
    ScriptState* scriptState,
    const String& name,
    ExceptionState& exceptionState,
    bool forceClose) {
  IDB_TRACE("IDBFactory::deleteDatabase");
  IDBDatabase::recordApiCallsHistogram(IDBDeleteDatabaseCall);
  if (!isContextValid(scriptState->getExecutionContext()))
    return nullptr;
  if (!scriptState->getExecutionContext()
           ->getSecurityOrigin()
           ->canAccessDatabase()) {
    exceptionState.throwSecurityError(
        "access to the Indexed Database API is denied in this context.");
    return nullptr;
  }

  IDBOpenDBRequest* request = IDBOpenDBRequest::create(
      scriptState, nullptr, 0, IDBDatabaseMetadata::DefaultVersion);

  if (!IndexedDBClient::from(scriptState->getExecutionContext())
           ->allowIndexedDB(scriptState->getExecutionContext(), name)) {
    request->onError(
        DOMException::create(UnknownError, permissionDeniedErrorMessage));
    return request;
  }

  Platform::current()->idbFactory()->deleteDatabase(
      name, request->createWebCallbacks().release(),
      WebSecurityOrigin(
          scriptState->getExecutionContext()->getSecurityOrigin()),
      forceClose);
  return request;
}

short IDBFactory::cmp(ScriptState* scriptState,
                      const ScriptValue& firstValue,
                      const ScriptValue& secondValue,
                      ExceptionState& exceptionState) {
  IDBKey* first = ScriptValue::to<IDBKey*>(scriptState->isolate(), firstValue,
                                           exceptionState);
  if (exceptionState.hadException())
    return 0;
  ASSERT(first);
  if (!first->isValid()) {
    exceptionState.throwDOMException(DataError,
                                     IDBDatabase::notValidKeyErrorMessage);
    return 0;
  }

  IDBKey* second = ScriptValue::to<IDBKey*>(scriptState->isolate(), secondValue,
                                            exceptionState);
  if (exceptionState.hadException())
    return 0;
  ASSERT(second);
  if (!second->isValid()) {
    exceptionState.throwDOMException(DataError,
                                     IDBDatabase::notValidKeyErrorMessage);
    return 0;
  }

  return static_cast<short>(first->compare(second));
}

}  // namespace blink
