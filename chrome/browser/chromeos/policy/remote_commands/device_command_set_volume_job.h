// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_SET_VOLUME_JOB_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_SET_VOLUME_JOB_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

class DeviceCommandSetVolumeJob : public RemoteCommandJob {
 public:
  using VolumeCallback = base::Callback<void(int)>;

  DeviceCommandSetVolumeJob();
  ~DeviceCommandSetVolumeJob() override;

  void SetVolumeCallbackForTesting(const VolumeCallback& callback);

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;
  base::TimeDelta GetCommmandTimeout() const override;

 protected:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  bool IsExpired(base::TimeTicks now) override;
  void RunImpl(const CallbackWithResult& succeeded_callback,
               const CallbackWithResult& failed_callback) override;

 private:
  // New volume level to be set, value in range [0,100].
  int volume_;

  // Used in tests instead of CrasAudioHandler::SetOutputVolumePercent.
  VolumeCallback volume_callback_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandSetVolumeJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_SET_VOLUME_JOB_H_
