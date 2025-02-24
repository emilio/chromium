// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"

namespace subresource_filter {

// AsyncDocumentSubresourceFilter::InitializationParams ------------------------

using InitializationParams =
    AsyncDocumentSubresourceFilter::InitializationParams;

InitializationParams::InitializationParams() = default;

InitializationParams::InitializationParams(GURL document_url,
                                           ActivationLevel activation_level,
                                           bool measure_performance)
    : document_url(std::move(document_url)),
      parent_activation_state(activation_level) {
  DCHECK_NE(ActivationLevel::DISABLED, activation_level);
  parent_activation_state.measure_performance = measure_performance;
}

InitializationParams::InitializationParams(
    GURL document_url,
    url::Origin parent_document_origin,
    ActivationState parent_activation_state)
    : document_url(std::move(document_url)),
      parent_document_origin(std::move(parent_document_origin)),
      parent_activation_state(parent_activation_state) {
  DCHECK_NE(ActivationLevel::DISABLED,
            parent_activation_state.activation_level);
}

InitializationParams::~InitializationParams() = default;
InitializationParams::InitializationParams(InitializationParams&&) = default;
InitializationParams& InitializationParams::operator=(InitializationParams&&) =
    default;

// AsyncDocumentSubresourceFilter ----------------------------------------------

AsyncDocumentSubresourceFilter::AsyncDocumentSubresourceFilter(
    VerifiedRuleset::Handle* ruleset_handle,
    InitializationParams params,
    base::Callback<void(ActivationState)> activation_state_callback,
    base::OnceClosure first_disallowed_load_callback)
    : task_runner_(ruleset_handle->task_runner()),
      core_(new Core(), base::OnTaskRunnerDeleter(task_runner_)),
      first_disallowed_load_callback_(
          std::move(first_disallowed_load_callback)) {
  DCHECK_NE(ActivationLevel::DISABLED,
            params.parent_activation_state.activation_level);

  // Note: It is safe to post |ruleset_handle|'s VerifiedRuleset pointer,
  // because a task to delete it can only be posted to (and, therefore,
  // processed by) |task_runner| after this method returns, hence after the
  // below task is posted.
  base::PostTaskAndReplyWithResult(
      task_runner_, FROM_HERE,
      base::Bind(&Core::Initialize, base::Unretained(core_.get()),
                 base::Passed(&params), ruleset_handle->ruleset_.get()),
      std::move(activation_state_callback));
}

AsyncDocumentSubresourceFilter::~AsyncDocumentSubresourceFilter() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void AsyncDocumentSubresourceFilter::GetLoadPolicyForSubdocument(
    const GURL& subdocument_url,
    LoadPolicyCallback result_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(pkalinnikov): Think about avoiding copy of |subdocument_url| if it is
  // too big and won't be allowed anyway (e.g., it's a data: URI).
  base::PostTaskAndReplyWithResult(
      task_runner_, FROM_HERE,
      base::Bind(
          [](AsyncDocumentSubresourceFilter::Core* core,
             const GURL& subdocument_url) {
            DCHECK(core);
            DocumentSubresourceFilter* filter = core->filter();
            return filter
                       ? filter->GetLoadPolicy(subdocument_url,
                                               proto::ELEMENT_TYPE_SUBDOCUMENT)
                       : LoadPolicy::ALLOW;
          },
          core_.get(), subdocument_url),
      std::move(result_callback));
}

void AsyncDocumentSubresourceFilter::ReportDisallowedLoad() {
  if (!first_disallowed_load_callback_.is_null())
    std::move(first_disallowed_load_callback_).Run();
}

// AsyncDocumentSubresourceFilter::Core ----------------------------------------

AsyncDocumentSubresourceFilter::Core::Core() {
  thread_checker_.DetachFromThread();
}

AsyncDocumentSubresourceFilter::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

ActivationState AsyncDocumentSubresourceFilter::Core::Initialize(
    InitializationParams params,
    VerifiedRuleset* verified_ruleset) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(verified_ruleset);

  if (!verified_ruleset->Get())
    return ActivationState(ActivationLevel::DISABLED);

  ActivationState activation_state = ComputeActivationState(
      params.document_url, params.parent_document_origin,
      params.parent_activation_state, verified_ruleset->Get());

  DCHECK_NE(ActivationLevel::DISABLED, activation_state.activation_level);
  filter_.emplace(url::Origin(params.document_url), activation_state,
                  verified_ruleset->Get());

  return activation_state;
}

}  // namespace subresource_filter
