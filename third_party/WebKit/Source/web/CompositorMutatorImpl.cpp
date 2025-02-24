// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/CompositorMutatorImpl.h"

#include "core/animation/CustomCompositorAnimationManager.h"
#include "core/dom/CompositorProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/graphics/CompositorMutationsTarget.h"
#include "platform/graphics/CompositorMutatorClient.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/Platform.h"
#include "web/CompositorAnimator.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

void createCompositorMutatorClient(
    std::unique_ptr<CompositorMutatorClient>* ptr,
    WaitableEvent* doneEvent) {
  CompositorMutatorImpl* mutator = CompositorMutatorImpl::create();
  ptr->reset(new CompositorMutatorClient(mutator, mutator->animationManager()));
  mutator->setClient(ptr->get());
  doneEvent->signal();
}

}  // namespace

CompositorMutatorImpl::CompositorMutatorImpl()
    : m_animationManager(WTF::wrapUnique(new CustomCompositorAnimationManager)),
      m_client(nullptr) {}

std::unique_ptr<CompositorMutatorClient> CompositorMutatorImpl::createClient() {
  std::unique_ptr<CompositorMutatorClient> mutatorClient;
  WaitableEvent doneEvent;
  if (WebThread* compositorThread = Platform::current()->compositorThread()) {
    compositorThread->getWebTaskRunner()->postTask(
        BLINK_FROM_HERE, crossThreadBind(&createCompositorMutatorClient,
                                         crossThreadUnretained(&mutatorClient),
                                         crossThreadUnretained(&doneEvent)));
  } else {
    createCompositorMutatorClient(&mutatorClient, &doneEvent);
  }
  // TODO(flackr): Instead of waiting for this event, we may be able to just set
  // the mutator on the CompositorWorkerProxyClient directly from the compositor
  // thread before it gets used there. We still need to make sure we only
  // create one mutator though.
  doneEvent.wait();
  return mutatorClient;
}

CompositorMutatorImpl* CompositorMutatorImpl::create() {
  return new CompositorMutatorImpl();
}

bool CompositorMutatorImpl::mutate(
    double monotonicTimeNow,
    CompositorMutableStateProvider* stateProvider) {
  TRACE_EVENT0("compositor-worker", "CompositorMutatorImpl::mutate");
  bool needToReinvoke = false;
  // TODO(vollick): we should avoid executing the animation frame
  // callbacks if none of the proxies in the global scope are affected by
  // m_mutations.
  for (CompositorAnimator* animator : m_animators) {
    if (animator->mutate(monotonicTimeNow, stateProvider))
      needToReinvoke = true;
  }

  return needToReinvoke;
}

void CompositorMutatorImpl::registerCompositorAnimator(
    CompositorAnimator* animator) {
  TRACE_EVENT0("compositor-worker",
               "CompositorMutatorImpl::registerCompositorAnimator");
  DCHECK(!m_animators.contains(animator));
  m_animators.insert(animator);
  setNeedsMutate();
}

void CompositorMutatorImpl::unregisterCompositorAnimator(
    CompositorAnimator* animator) {
  DCHECK(m_animators.contains(animator));
  m_animators.erase(animator);
}

void CompositorMutatorImpl::setNeedsMutate() {
  TRACE_EVENT0("compositor-worker", "CompositorMutatorImpl::setNeedsMutate");
  m_client->setNeedsMutate();
}

}  // namespace blink
