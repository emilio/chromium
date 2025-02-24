// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

namespace midi {

namespace {

using Sample = base::HistogramBase::Sample;
using midi::mojom::PortState;
using midi::mojom::Result;

// If many users have more devices, this number will be increased.
// But the number is expected to be big enough for now.
const Sample kMaxUmaDevices = 31;

// Used to count events for usage histogram.
enum class Usage {
  CREATED,
  CREATED_ON_UNSUPPORTED_PLATFORMS,
  SESSION_STARTED,
  SESSION_ENDED,
  INITIALIZED,
  INPUT_PORT_ADDED,
  OUTPUT_PORT_ADDED,

  // New items should be inserted here, and |MAX| should point the last item.
  MAX = INITIALIZED,
};

void ReportUsage(Usage usage) {
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.Usage",
                            static_cast<Sample>(usage),
                            static_cast<Sample>(Usage::MAX) + 1);
}

}  // namespace

MidiManager::MidiManager(MidiService* service)
    : initialization_state_(InitializationState::NOT_STARTED),
      finalized_(false),
      result_(Result::NOT_INITIALIZED),
      service_(service) {
  ReportUsage(Usage::CREATED);
}

MidiManager::~MidiManager() {
  // Make sure that Finalize() is called to clean up resources allocated on
  // the Chrome_IOThread.
  base::AutoLock auto_lock(lock_);
  CHECK(finalized_);
}

#if !defined(OS_MACOSX) && !defined(OS_WIN) && \
    !(defined(USE_ALSA) && defined(USE_UDEV)) && !defined(OS_ANDROID)
MidiManager* MidiManager::Create(MidiService* service) {
  ReportUsage(Usage::CREATED_ON_UNSUPPORTED_PLATFORMS);
  return new MidiManager(service);
}
#endif

void MidiManager::Shutdown() {
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.ResultOnShutdown",
                            static_cast<int>(result_),
                            static_cast<int>(Result::MAX) + 1);
  bool shutdown_synchronously = false;
  {
    base::AutoLock auto_lock(lock_);
    if (session_thread_runner_) {
      if (session_thread_runner_->BelongsToCurrentThread()) {
        shutdown_synchronously = true;
      } else {
        session_thread_runner_->PostTask(
            FROM_HERE, base::Bind(&MidiManager::ShutdownOnSessionThread,
                                  base::Unretained(this)));
      }
      session_thread_runner_ = nullptr;
    } else {
      finalized_ = true;
    }
  }
  if (shutdown_synchronously)
    ShutdownOnSessionThread();
}

void MidiManager::StartSession(MidiManagerClient* client) {
  ReportUsage(Usage::SESSION_STARTED);

  bool needs_initialization = false;

  {
    base::AutoLock auto_lock(lock_);
    if (clients_.find(client) != clients_.end() ||
        pending_clients_.find(client) != pending_clients_.end()) {
      // Should not happen. But just in case the renderer is compromised.
      NOTREACHED();
      return;
    }

    // Do not accept a new request if Shutdown() was already called.
    if (finalized_) {
      client->CompleteStartSession(Result::INITIALIZATION_ERROR);
      return;
    }

    if (initialization_state_ == InitializationState::COMPLETED) {
      // Platform dependent initialization was already finished for previously
      // initialized clients.
      if (result_ == Result::OK) {
        AddInitialPorts(client);
        clients_.insert(client);
      }
      // Complete synchronously with |result_|;
      client->CompleteStartSession(result_);
      return;
    }

    // Do not accept a new request if the pending client list contains too
    // many clients.
    if (pending_clients_.size() >= kMaxPendingClientCount) {
      client->CompleteStartSession(Result::INITIALIZATION_ERROR);
      return;
    }

    if (initialization_state_ == InitializationState::NOT_STARTED) {
      // Set fields protected by |lock_| here and call StartInitialization()
      // later.
      needs_initialization = true;
      session_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
      initialization_state_ = InitializationState::STARTED;
    }

    pending_clients_.insert(client);
  }

  if (needs_initialization) {
    // Lazily initialize the MIDI back-end.
    TRACE_EVENT0("midi", "MidiManager::StartInitialization");
    // CompleteInitialization() will be called asynchronously when platform
    // dependent initialization is finished.
    StartInitialization();
  }
}

void MidiManager::EndSession(MidiManagerClient* client) {
  ReportUsage(Usage::SESSION_ENDED);

  // At this point, |client| can be in the destruction process, and calling
  // any method of |client| is dangerous. Calls on clients *must* be protected
  // by |lock_| to prevent race conditions.
  base::AutoLock auto_lock(lock_);
  clients_.erase(client);
  pending_clients_.erase(client);
}

void MidiManager::AccumulateMidiBytesSent(MidiManagerClient* client, size_t n) {
  base::AutoLock auto_lock(lock_);
  if (clients_.find(client) == clients_.end())
    return;

  // Continue to hold lock_ here in case another thread is currently doing
  // EndSession.
  client->AccumulateMidiBytesSent(n);
}

void MidiManager::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32_t port_index,
                                       const std::vector<uint8_t>& data,
                                       double timestamp) {
  NOTREACHED();
}

void MidiManager::StartInitialization() {
  CompleteInitialization(Result::NOT_SUPPORTED);
}

void MidiManager::CompleteInitialization(Result result) {
  bool complete_asynchronously = false;
  {
    base::AutoLock auto_lock(lock_);
    if (session_thread_runner_) {
      if (session_thread_runner_->BelongsToCurrentThread()) {
        complete_asynchronously = true;
      } else {
        session_thread_runner_->PostTask(
            FROM_HERE, base::Bind(&MidiManager::CompleteInitializationInternal,
                                  base::Unretained(this), result));
      }
    }
  }
  if (complete_asynchronously)
    CompleteInitializationInternal(result);
}

void MidiManager::AddInputPort(const MidiPortInfo& info) {
  ReportUsage(Usage::INPUT_PORT_ADDED);
  base::AutoLock auto_lock(lock_);
  input_ports_.push_back(info);
  for (auto* client : clients_)
    client->AddInputPort(info);
}

void MidiManager::AddOutputPort(const MidiPortInfo& info) {
  ReportUsage(Usage::OUTPUT_PORT_ADDED);
  base::AutoLock auto_lock(lock_);
  output_ports_.push_back(info);
  for (auto* client : clients_)
    client->AddOutputPort(info);
}

void MidiManager::SetInputPortState(uint32_t port_index, PortState state) {
  base::AutoLock auto_lock(lock_);
  DCHECK_LT(port_index, input_ports_.size());
  input_ports_[port_index].state = state;
  for (auto* client : clients_)
    client->SetInputPortState(port_index, state);
}

void MidiManager::SetOutputPortState(uint32_t port_index, PortState state) {
  base::AutoLock auto_lock(lock_);
  DCHECK_LT(port_index, output_ports_.size());
  output_ports_[port_index].state = state;
  for (auto* client : clients_)
    client->SetOutputPortState(port_index, state);
}

void MidiManager::ReceiveMidiData(uint32_t port_index,
                                  const uint8_t* data,
                                  size_t length,
                                  double timestamp) {
  base::AutoLock auto_lock(lock_);

  for (auto* client : clients_)
    client->ReceiveMidiData(port_index, data, length, timestamp);
}

void MidiManager::CompleteInitializationInternal(Result result) {
  TRACE_EVENT0("midi", "MidiManager::CompleteInitialization");
  ReportUsage(Usage::INITIALIZED);
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.InputPorts",
                            static_cast<Sample>(input_ports_.size()),
                            kMaxUmaDevices + 1);
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.OutputPorts",
                            static_cast<Sample>(output_ports_.size()),
                            kMaxUmaDevices + 1);

  base::AutoLock auto_lock(lock_);
  DCHECK(clients_.empty());
  DCHECK_EQ(initialization_state_, InitializationState::STARTED);
  initialization_state_ = InitializationState::COMPLETED;
  result_ = result;

  for (auto* client : pending_clients_) {
    if (result_ == Result::OK) {
      AddInitialPorts(client);
      clients_.insert(client);
    }
    client->CompleteStartSession(result_);
  }
  pending_clients_.clear();
}

void MidiManager::AddInitialPorts(MidiManagerClient* client) {
  lock_.AssertAcquired();

  for (const auto& info : input_ports_)
    client->AddInputPort(info);
  for (const auto& info : output_ports_)
    client->AddOutputPort(info);
}

void MidiManager::ShutdownOnSessionThread() {
  Finalize();
  base::AutoLock auto_lock(lock_);
  finalized_ = true;

  // Detach all clients so that they do not call MidiManager methods any more.
  for (auto* client : clients_)
    client->Detach();
}

}  // namespace midi
