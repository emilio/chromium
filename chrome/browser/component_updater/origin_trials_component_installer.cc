// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/origin_trials_component_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

// The client-side configuration for the origin trial framework can be
// overridden by an installed component named 'OriginTrials' (extension id
// kfoklmclfodeliojeaekpoflbkkhojea. This component currently consists of just a
// manifest.json file, which can contain a custom key named 'origin-trials'. The
// value of this key is a dictionary:
//
// {
//   "public-key": "<base64-encoding of replacement public key>",
//   "disabled-features": [<list of features to disable>],
//   "revoked-tokens": "<base64-encoded data>"
// }
//
// TODO(iclelland): Implement support for revoked tokens.
//
// If the component is not present in the user data directory, the default
// configuration will be used.

namespace component_updater {

namespace {

static const char kManifestOriginTrialsKey[] = "origin-trials";
static const char kManifestPublicKeyPath[] = "origin-trials.public-key";
static const char kManifestDisabledFeaturesPath[] =
    "origin-trials.disabled-features";

// Extension id is llkgjffcdpffmhiakmfcdcblohccpfmo
const uint8_t kSha256Hash[] = {0xbb, 0xa6, 0x95, 0x52, 0x3f, 0x55, 0xc7, 0x80,
                               0xac, 0x52, 0x32, 0x1b, 0xe7, 0x22, 0xf5, 0xce,
                               0x6a, 0xfd, 0x9c, 0x9e, 0xa9, 0x2a, 0x0b, 0x50,
                               0x60, 0x2b, 0x7f, 0x6c, 0x64, 0x80, 0x09, 0x04};

}  // namespace

bool OriginTrialsComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Test if the "origin-trials" key is present in the manifest.
  return manifest.HasKey(kManifestOriginTrialsKey);
}

bool OriginTrialsComponentInstallerTraits::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool OriginTrialsComponentInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
OriginTrialsComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void OriginTrialsComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // Read the configuration from the manifest and set values in browser
  // local_state. These will be used on the next browser restart.
  // If an individual configuration value is missing, treat as a reset to the
  // browser defaults.
  PrefService* local_state = g_browser_process->local_state();
  std::string override_public_key;
  if (manifest->GetString(kManifestPublicKeyPath, &override_public_key)) {
    local_state->Set(prefs::kOriginTrialPublicKey,
                     base::Value(override_public_key));
  } else {
    local_state->ClearPref(prefs::kOriginTrialPublicKey);
  }
  base::ListValue* override_disabled_feature_list = nullptr;
  const bool manifest_has_disabled_features = manifest->GetList(
      kManifestDisabledFeaturesPath, &override_disabled_feature_list);
  if (manifest_has_disabled_features &&
      !override_disabled_feature_list->empty()) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledFeatures);
    update->Swap(override_disabled_feature_list);
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledFeatures);
  }
}

base::FilePath OriginTrialsComponentInstallerTraits::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("OriginTrials"));
}

void OriginTrialsComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  if (!hash)
    return;
  hash->assign(kSha256Hash, kSha256Hash + arraysize(kSha256Hash));
}

std::string OriginTrialsComponentInstallerTraits::GetName() const {
  return "Origin Trials";
}

update_client::InstallerAttributes
OriginTrialsComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> OriginTrialsComponentInstallerTraits::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void RegisterOriginTrialsComponent(ComponentUpdateService* cus,
                                   const base::FilePath& user_data_dir) {
  std::unique_ptr<ComponentInstallerTraits> traits(
      new OriginTrialsComponentInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
