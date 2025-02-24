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

#ifndef DOMWrapperWorld_h
#define DOMWrapperWorld_h

#include <memory>

#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "v8/include/v8.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

class DOMDataStore;
class DOMObjectHolderBase;

// This class represent a collection of DOM wrappers for a specific world. This
// is identified by a world id that is a per-thread global identifier (see
// WorldId enum).
class CORE_EXPORT DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
 public:
  // Per-thread global identifiers for DOMWrapperWorld.
  enum WorldId {
    InvalidWorldId = -1,
    MainWorldId = 0,

    // Embedder isolated worlds can use IDs in [1, 1<<29).
    EmbedderWorldIdLimit = (1 << 29),
    DocumentXMLTreeViewerWorldId,
    IsolatedWorldIdLimit,

    // TODO(nhiroki): Dynamically allocate a world id for the following worlds
    // instead of a fixed value (https://crbug.com/697622).
    GarbageCollectorWorldId,
    RegExpWorldId,
    TestingWorldId,
    WorkerWorldId,
  };

  enum class WorldType {
    Main,
    Isolated,
    GarbageCollector,
    RegExp,
    Testing,
    Worker,
  };

  // Creates a world other than IsolatedWorld.
  static PassRefPtr<DOMWrapperWorld> create(v8::Isolate*, WorldType);

  // Ensures an IsolatedWorld for |worldId|.
  static PassRefPtr<DOMWrapperWorld> ensureIsolatedWorld(v8::Isolate*,
                                                         int worldId);
  ~DOMWrapperWorld();
  void dispose();

  static bool nonMainWorldsInMainThread() {
    return s_numberOfNonMainWorldsInMainThread;
  }
  static void allWorldsInMainThread(Vector<RefPtr<DOMWrapperWorld>>& worlds);
  static void markWrappersInAllWorlds(ScriptWrappable*,
                                      const ScriptWrappableVisitor*);

  static DOMWrapperWorld& world(v8::Local<v8::Context> context) {
    return ScriptState::from(context)->world();
  }

  static DOMWrapperWorld& current(v8::Isolate* isolate) {
    return world(isolate->GetCurrentContext());
  }

  static DOMWrapperWorld*& workerWorld();
  static DOMWrapperWorld& mainWorld();
  static PassRefPtr<DOMWrapperWorld> fromWorldId(v8::Isolate*, int worldId);

  static void setIsolatedWorldHumanReadableName(int worldID, const String&);
  String isolatedWorldHumanReadableName();

  // Associates an isolated world (see above for description) with a security
  // origin. XMLHttpRequest instances used in that world will be considered
  // to come from that origin, not the frame's.
  static void setIsolatedWorldSecurityOrigin(int worldId,
                                             PassRefPtr<SecurityOrigin>);
  SecurityOrigin* isolatedWorldSecurityOrigin();

  // Associated an isolated world with a Content Security Policy. Resources
  // embedded into the main world's DOM from script executed in an isolated
  // world should be restricted based on the isolated world's DOM, not the
  // main world's.
  //
  // FIXME: Right now, resource injection simply bypasses the main world's
  // DOM. More work is necessary to allow the isolated world's policy to be
  // applied correctly.
  static void setIsolatedWorldContentSecurityPolicy(int worldId,
                                                    const String& policy);
  bool isolatedWorldHasContentSecurityPolicy();

  bool isMainWorld() const { return m_worldType == WorldType::Main; }
  bool isWorkerWorld() const { return m_worldType == WorldType::Worker; }
  bool isIsolatedWorld() const { return m_worldType == WorldType::Isolated; }

  int worldId() const { return m_worldId; }
  DOMDataStore& domDataStore() const { return *m_domDataStore; }

 public:
  template <typename T>
  void registerDOMObjectHolder(v8::Isolate*, T*, v8::Local<v8::Value>);

 private:
  DOMWrapperWorld(v8::Isolate*, WorldType, int worldId);

  static void weakCallbackForDOMObjectHolder(
      const v8::WeakCallbackInfo<DOMObjectHolderBase>&);
  void registerDOMObjectHolderInternal(std::unique_ptr<DOMObjectHolderBase>);
  void unregisterDOMObjectHolder(DOMObjectHolderBase*);

  static unsigned s_numberOfNonMainWorldsInMainThread;

  // Returns an identifier for a given world type. This must not call for
  // WorldType::IsolatedWorld because an identifier for the world is given from
  // out of DOMWrapperWorld.
  static int getWorldIdForType(WorldType);

  const WorldType m_worldType;
  const int m_worldId;
  std::unique_ptr<DOMDataStore> m_domDataStore;
  HashSet<std::unique_ptr<DOMObjectHolderBase>> m_domObjectHolders;
};

}  // namespace blink

#endif  // DOMWrapperWorld_h
