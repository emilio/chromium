// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {

namespace {

const size_t kMaxSenders = 100;

}  // namespace

InstanceIDHandler::InstanceIDHandler() {
}

InstanceIDHandler::~InstanceIDHandler() {
}

void InstanceIDHandler::DeleteAllTokensForApp(
    const std::string& app_id, const DeleteTokenCallback& callback) {
  DeleteToken(app_id, "*", "*", callback);
}

GCMDriver::GCMDriver(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : weak_ptr_factory_(this) {
  // The |blocking_task_runner| can be NULL for tests that do not need the
  // encryption capabilities of the GCMDriver class.
  if (blocking_task_runner)
    encryption_provider_.Init(store_path, blocking_task_runner);
}

GCMDriver::~GCMDriver() {
}

void GCMDriver::Register(const std::string& app_id,
                         const std::vector<std::string>& sender_ids,
                         const RegisterCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!sender_ids.empty() && sender_ids.size() <= kMaxSenders);
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    callback.Run(std::string(), result);
    return;
  }

  // If previous register operation is still in progress, bail out.
  if (register_callbacks_.find(app_id) != register_callbacks_.end()) {
    callback.Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  // Normalize the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  register_callbacks_[app_id] = callback;

  // If previous unregister operation is still in progress, wait until it
  // finishes. We don't want to throw ASYNC_OPERATION_PENDING when the user
  // uninstalls an app (ungistering) and then reinstalls the app again
  // (registering).
  std::map<std::string, UnregisterCallback>::iterator unregister_iter =
      unregister_callbacks_.find(app_id);
  if (unregister_iter != unregister_callbacks_.end()) {
    // Replace the original unregister callback with an intermediate callback
    // that will invoke the original unregister callback and trigger the pending
    // registration after the unregistration finishes.
    // Note that some parameters to RegisterAfterUnregister are specified here
    // when the callback is created (base::Bind supports the partial binding
    // of parameters).
    unregister_iter->second = base::Bind(
        &GCMDriver::RegisterAfterUnregister,
        weak_ptr_factory_.GetWeakPtr(),
        app_id,
        normalized_sender_ids,
        unregister_iter->second);
    return;
  }

  RegisterImpl(app_id, normalized_sender_ids);
}

void GCMDriver::Unregister(const std::string& app_id,
                           const UnregisterCallback& callback) {
  UnregisterInternal(app_id, nullptr /* sender_id */, callback);
}

void GCMDriver::UnregisterWithSenderId(
    const std::string& app_id,
    const std::string& sender_id,
    const UnregisterCallback& callback) {
  DCHECK(!sender_id.empty());
  UnregisterInternal(app_id, &sender_id, callback);
}

void GCMDriver::UnregisterInternal(const std::string& app_id,
                                   const std::string* sender_id,
                                   const UnregisterCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    callback.Run(result);
    return;
  }

  // If previous un/register operation is still in progress, bail out.
  if (register_callbacks_.find(app_id) != register_callbacks_.end() ||
      unregister_callbacks_.find(app_id) != unregister_callbacks_.end()) {
    callback.Run(GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  unregister_callbacks_[app_id] = callback;

  if (sender_id)
    UnregisterWithSenderIdImpl(app_id, *sender_id);
  else
    UnregisterImpl(app_id);
}

void GCMDriver::Send(const std::string& app_id,
                     const std::string& receiver_id,
                     const OutgoingMessage& message,
                     const SendCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!receiver_id.empty());
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    callback.Run(std::string(), result);
    return;
  }

  // If the message with send ID is still in progress, bail out.
  std::pair<std::string, std::string> key(app_id, message.id);
  if (send_callbacks_.find(key) != send_callbacks_.end()) {
    callback.Run(message.id, GCMClient::INVALID_PARAMETER);
    return;
  }

  send_callbacks_[key] = callback;

  SendImpl(app_id, receiver_id, message);
}

void GCMDriver::GetEncryptionInfo(
    const std::string& app_id,
    const GetEncryptionInfoCallback& callback) {
  encryption_provider_.GetEncryptionInfo(app_id, "" /* authorized_entity */,
                                         callback);
}

void GCMDriver::UnregisterWithSenderIdImpl(const std::string& app_id,
                                           const std::string& sender_id) {
  NOTREACHED();
}

void GCMDriver::RegisterFinished(const std::string& app_id,
                                 const std::string& registration_id,
                                 GCMClient::Result result) {
  std::map<std::string, RegisterCallback>::iterator callback_iter =
      register_callbacks_.find(app_id);
  if (callback_iter == register_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  RegisterCallback callback = callback_iter->second;
  register_callbacks_.erase(callback_iter);
  callback.Run(registration_id, result);
}

void GCMDriver::RemoveEncryptionInfoAfterUnregister(const std::string& app_id,
                                                    GCMClient::Result result) {
  encryption_provider_.RemoveEncryptionInfo(
      app_id, "" /* authorized_entity */,
      base::Bind(&GCMDriver::UnregisterFinished, weak_ptr_factory_.GetWeakPtr(),
                 app_id, result));
}

void GCMDriver::UnregisterFinished(const std::string& app_id,
                                   GCMClient::Result result) {
  std::map<std::string, UnregisterCallback>::iterator callback_iter =
      unregister_callbacks_.find(app_id);
  if (callback_iter == unregister_callbacks_.end())
    return;

  UnregisterCallback callback = callback_iter->second;
  unregister_callbacks_.erase(callback_iter);
  callback.Run(result);
}

void GCMDriver::SendFinished(const std::string& app_id,
                             const std::string& message_id,
                             GCMClient::Result result) {
  std::map<std::pair<std::string, std::string>, SendCallback>::iterator
      callback_iter = send_callbacks_.find(
          std::pair<std::string, std::string>(app_id, message_id));
  if (callback_iter == send_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  SendCallback callback = callback_iter->second;
  send_callbacks_.erase(callback_iter);
  callback.Run(message_id, result);
}

void GCMDriver::Shutdown() {
  for (GCMAppHandlerMap::const_iterator iter = app_handlers_.begin();
       iter != app_handlers_.end(); ++iter) {
    DVLOG(1) << "Calling ShutdownHandler for: " << iter->first;
    iter->second->ShutdownHandler();
  }
  app_handlers_.clear();
}

void GCMDriver::AddAppHandler(const std::string& app_id,
                              GCMAppHandler* handler) {
  DCHECK(!app_id.empty());
  DCHECK(handler);
  DCHECK_EQ(app_handlers_.count(app_id), 0u);
  app_handlers_[app_id] = handler;
  DVLOG(1) << "App handler added for: " << app_id;
}

void GCMDriver::RemoveAppHandler(const std::string& app_id) {
  DCHECK(!app_id.empty());
  app_handlers_.erase(app_id);
  DVLOG(1) << "App handler removed for: " << app_id;
}

GCMAppHandler* GCMDriver::GetAppHandler(const std::string& app_id) {
  // Look for exact match.
  GCMAppHandlerMap::const_iterator iter = app_handlers_.find(app_id);
  if (iter != app_handlers_.end())
    return iter->second;

  // Ask the handlers whether they know how to handle it.
  for (iter = app_handlers_.begin(); iter != app_handlers_.end(); ++iter) {
    if (iter->second->CanHandle(app_id))
      return iter->second;
  }

  return nullptr;
}

GCMEncryptionProvider* GCMDriver::GetEncryptionProviderInternal() {
  return &encryption_provider_;
}

bool GCMDriver::HasRegisterCallback(const std::string& app_id) {
  return register_callbacks_.find(app_id) != register_callbacks_.end();
}

void GCMDriver::ClearCallbacks() {
  register_callbacks_.clear();
  unregister_callbacks_.clear();
  send_callbacks_.clear();
}

void GCMDriver::DispatchMessage(const std::string& app_id,
                                const IncomingMessage& message) {
  encryption_provider_.DecryptMessage(
      app_id, message, base::Bind(&GCMDriver::DispatchMessageInternal,
                                  weak_ptr_factory_.GetWeakPtr(), app_id));
}

void GCMDriver::DispatchMessageInternal(
    const std::string& app_id,
    GCMEncryptionProvider::DecryptionResult result,
    const IncomingMessage& message) {
  UMA_HISTOGRAM_ENUMERATION("GCM.Crypto.DecryptMessageResult", result,
                            GCMEncryptionProvider::DECRYPTION_RESULT_LAST + 1);

  switch (result) {
    case GCMEncryptionProvider::DECRYPTION_RESULT_UNENCRYPTED:
    case GCMEncryptionProvider::DECRYPTION_RESULT_DECRYPTED: {
      GCMAppHandler* handler = GetAppHandler(app_id);
      if (handler)
        handler->OnMessage(app_id, message);

      // TODO(peter/harkness): Surface unavailable app handlers on
      // chrome://gcm-internals and send a delivery receipt.
      return;
    }
    case GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_ENCRYPTION_HEADER:
    case GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_CRYPTO_KEY_HEADER:
    case GCMEncryptionProvider::DECRYPTION_RESULT_NO_KEYS:
    case GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_SHARED_SECRET:
    case GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_PAYLOAD:
      RecordDecryptionFailure(app_id, result);
      return;
  }

  NOTREACHED();
}

void GCMDriver::RegisterAfterUnregister(
    const std::string& app_id,
    const std::vector<std::string>& normalized_sender_ids,
    const UnregisterCallback& unregister_callback,
    GCMClient::Result result) {
  // Invoke the original unregister callback.
  unregister_callback.Run(result);

  // Trigger the pending registration.
  DCHECK(register_callbacks_.find(app_id) != register_callbacks_.end());
  RegisterImpl(app_id, normalized_sender_ids);
}

}  // namespace gcm
