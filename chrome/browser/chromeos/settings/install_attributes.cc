// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/install_attributes.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/proto/install_attributes.pb.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace cu = cryptohome_util;

namespace {

// Number of TPM lock state query retries during consistency check.
int kDbusRetryCount = 12;

// Interval of TPM lock state query retries during consistency check.
int kDbusRetryIntervalInSeconds = 5;

std::string ReadMapKey(const std::map<std::string, std::string>& map,
                       const std::string& key) {
  std::map<std::string, std::string>::const_iterator entry = map.find(key);
  if (entry != map.end()) {
    return entry->second;
  }
  return std::string();
}

void WarnIfNonempty(const std::map<std::string, std::string>& map,
                    const std::string& key) {
  if (!ReadMapKey(map, key).empty()) {
    LOG(WARNING) << key << " expected to be empty.";
  }
}

}  // namespace

// static
std::string
InstallAttributes::GetEnterpriseOwnedInstallAttributesBlobForTesting(
    const std::string& user_name) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  cryptohome::SerializedInstallAttributes::Attribute* attribute = nullptr;

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(InstallAttributes::kAttrEnterpriseOwned);
  attribute->set_value("true");

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(InstallAttributes::kAttrEnterpriseUser);
  attribute->set_value(user_name);

  return install_attrs_proto.SerializeAsString();
}

// static
std::string InstallAttributes::
    GetActiveDirectoryEnterpriseOwnedInstallAttributesBlobForTesting(
        const std::string& realm) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  cryptohome::SerializedInstallAttributes::Attribute* attribute = nullptr;

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(InstallAttributes::kAttrEnterpriseOwned);
  attribute->set_value("true");

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(InstallAttributes::kAttrEnterpriseMode);
  attribute->set_value(InstallAttributes::kEnterpriseADDeviceMode);

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(InstallAttributes::kAttrEnterpriseRealm);
  attribute->set_value(realm);

  return install_attrs_proto.SerializeAsString();
}

InstallAttributes::InstallAttributes(CryptohomeClient* cryptohome_client)
    : cryptohome_client_(cryptohome_client),
      weak_ptr_factory_(this) {
}

InstallAttributes::~InstallAttributes() {}

void InstallAttributes::Init(const base::FilePath& cache_file) {
  DCHECK(!device_locked_);

  // Mark the consistency check as running to ensure that LockDevice() is
  // blocked, but wait for the cryptohome service to be available before
  // actually calling TriggerConsistencyCheck().
  consistency_check_running_ = true;
  cryptohome_client_->WaitForServiceToBeAvailable(
      base::Bind(&InstallAttributes::OnCryptohomeServiceInitiallyAvailable,
                 weak_ptr_factory_.GetWeakPtr()));

  if (!base::PathExists(cache_file))
    return;

  device_locked_ = true;

  char buf[16384];
  int len = base::ReadFile(cache_file, buf, sizeof(buf));
  if (len == -1 || len >= static_cast<int>(sizeof(buf))) {
    PLOG(ERROR) << "Failed to read " << cache_file.value();
    return;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  if (!install_attrs_proto.ParseFromArray(buf, len)) {
    LOG(ERROR) << "Failed to parse install attributes cache.";
    return;
  }

  google::protobuf::RepeatedPtrField<
      const cryptohome::SerializedInstallAttributes::Attribute>::iterator entry;
  std::map<std::string, std::string> attr_map;
  for (entry = install_attrs_proto.attributes().begin();
       entry != install_attrs_proto.attributes().end();
       ++entry) {
    // The protobuf values contain terminating null characters, so we have to
    // sanitize the value here.
    attr_map.insert(std::make_pair(entry->name(),
                                   std::string(entry->value().c_str())));
  }

  DecodeInstallAttributes(attr_map);
}

void InstallAttributes::ReadImmutableAttributes(const base::Closure& callback) {
  if (device_locked_) {
    callback.Run();
    return;
  }

  cryptohome_client_->InstallAttributesIsReady(
      base::Bind(&InstallAttributes::ReadAttributesIfReady,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void InstallAttributes::ReadAttributesIfReady(const base::Closure& callback,
                                              DBusMethodCallStatus call_status,
                                              bool result) {
  if (call_status == DBUS_METHOD_CALL_SUCCESS && result) {
    registration_mode_ = policy::DEVICE_MODE_NOT_SET;
    if (!cu::InstallAttributesIsInvalid() &&
        !cu::InstallAttributesIsFirstInstall()) {
      device_locked_ = true;

      static const char* const kEnterpriseAttributes[] = {
        kAttrEnterpriseDeviceId,
        kAttrEnterpriseDomain,
        kAttrEnterpriseRealm,
        kAttrEnterpriseMode,
        kAttrEnterpriseOwned,
        kAttrEnterpriseUser,
        kAttrConsumerKioskEnabled,
      };
      std::map<std::string, std::string> attr_map;
      for (size_t i = 0; i < arraysize(kEnterpriseAttributes); ++i) {
        std::string value;
        if (cu::InstallAttributesGet(kEnterpriseAttributes[i], &value))
          attr_map[kEnterpriseAttributes[i]] = value;
      }

      DecodeInstallAttributes(attr_map);
    }
  }
  callback.Run();
}

void InstallAttributes::LockDevice(policy::DeviceMode device_mode,
                                   const std::string& domain,
                                   const std::string& realm,
                                   const std::string& device_id,
                                   const LockResultCallback& callback) {
  CHECK((device_mode == policy::DEVICE_MODE_ENTERPRISE &&
         !domain.empty() && realm.empty() && !device_id.empty()) ||
        (device_mode == policy::DEVICE_MODE_ENTERPRISE_AD &&
         domain.empty() && !realm.empty() && !device_id.empty()) ||
        (device_mode == policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH &&
         domain.empty() && realm.empty() && device_id.empty()));
  DCHECK(!callback.is_null());
  CHECK_EQ(device_lock_running_, false);

  // Check for existing lock first.
  if (device_locked_) {
    if (device_mode != registration_mode_) {
      LOG(ERROR) << "Trying to re-lock with wrong mode.";
      callback.Run(LOCK_WRONG_MODE);
      return;
    }

    if (domain != registration_domain_ ||
        realm != registration_realm_ ||
        device_id != registration_device_id_) {
      LOG(ERROR) << "Trying to re-lock with non-matching parameters.";
      callback.Run(LOCK_WRONG_DOMAIN);
      return;
    }

    // Already locked in the right mode, signal success.
    callback.Run(LOCK_SUCCESS);
    return;
  }

  // In case the consistency check is still running, postpone the device locking
  // until it has finished.  This should not introduce additional delay since
  // device locking must wait for TPM initialization anyways.
  if (consistency_check_running_) {
    CHECK(post_check_action_.is_null());
    post_check_action_ = base::Bind(&InstallAttributes::LockDevice,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    device_mode,
                                    domain,
                                    realm,
                                    device_id,
                                    callback);
    return;
  }

  device_lock_running_ = true;
  cryptohome_client_->InstallAttributesIsReady(
      base::Bind(&InstallAttributes::LockDeviceIfAttributesIsReady,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_mode,
                 domain,
                 realm,
                 device_id,
                 callback));
}

void InstallAttributes::LockDeviceIfAttributesIsReady(
    policy::DeviceMode device_mode,
    const std::string& domain,
    const std::string& realm,
    const std::string& device_id,
    const LockResultCallback& callback,
    DBusMethodCallStatus call_status,
    bool result) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS || !result) {
    device_lock_running_ = false;
    callback.Run(LOCK_NOT_READY);
    return;
  }

  // Clearing the TPM password seems to be always a good deal.
  if (cu::TpmIsEnabled() && !cu::TpmIsBeingOwned() && cu::TpmIsOwned()) {
    cryptohome_client_->CallTpmClearStoredPasswordAndBlock();
  }

  // Make sure we really have a working InstallAttrs.
  if (cu::InstallAttributesIsInvalid()) {
    LOG(ERROR) << "Install attributes invalid.";
    device_lock_running_ = false;
    callback.Run(LOCK_BACKEND_INVALID);
    return;
  }

  if (!cu::InstallAttributesIsFirstInstall()) {
    LOG(ERROR) << "Install attributes already installed.";
    device_lock_running_ = false;
    callback.Run(LOCK_ALREADY_LOCKED);
    return;
  }

  // Set values in the InstallAttrs.
  std::string kiosk_enabled, enterprise_owned;
  if (device_mode == policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH) {
    kiosk_enabled = "true";
  } else {
    enterprise_owned = "true";
  }
  std::string mode = GetDeviceModeString(device_mode);
  if (!cu::InstallAttributesSet(kAttrConsumerKioskEnabled, kiosk_enabled) ||
      !cu::InstallAttributesSet(kAttrEnterpriseOwned, enterprise_owned) ||
      !cu::InstallAttributesSet(kAttrEnterpriseMode, mode) ||
      !cu::InstallAttributesSet(kAttrEnterpriseDomain, domain) ||
      !cu::InstallAttributesSet(kAttrEnterpriseRealm, realm) ||
      !cu::InstallAttributesSet(kAttrEnterpriseDeviceId, device_id)) {
    LOG(ERROR) << "Failed writing attributes.";
    device_lock_running_ = false;
    callback.Run(LOCK_SET_ERROR);
    return;
  }

  if (!cu::InstallAttributesFinalize() ||
      cu::InstallAttributesIsFirstInstall()) {
    LOG(ERROR) << "Failed locking.";
    device_lock_running_ = false;
    callback.Run(LOCK_FINALIZE_ERROR);
    return;
  }

  ReadImmutableAttributes(
      base::Bind(&InstallAttributes::OnReadImmutableAttributes,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_mode,
                 domain,
                 realm,
                 device_id,
                 callback));
}

void InstallAttributes::OnReadImmutableAttributes(
    policy::DeviceMode mode,
    const std::string& domain,
    const std::string& realm,
    const std::string& device_id,
    const LockResultCallback& callback) {
  device_lock_running_ = false;

  if (registration_mode_ != mode ||
      registration_domain_ != domain ||
      registration_realm_ != realm ||
      registration_device_id_ != device_id) {
    LOG(ERROR) << "Locked data doesn't match.";
    callback.Run(LOCK_READBACK_ERROR);
    return;
  }

  callback.Run(LOCK_SUCCESS);
}

bool InstallAttributes::IsEnterpriseManaged() const {
  if (!device_locked_) {
    return false;
  }
  return registration_mode_ == policy::DEVICE_MODE_ENTERPRISE ||
      registration_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD;
}

bool InstallAttributes::IsActiveDirectoryManaged() const {
  if (!device_locked_) {
    return false;
  }
  return registration_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD;
}

bool InstallAttributes::IsCloudManaged() const {
  if (!device_locked_) {
    return false;
  }
  return registration_mode_ == policy::DEVICE_MODE_ENTERPRISE;
}

bool InstallAttributes::IsConsumerKioskDeviceWithAutoLaunch() {
  return device_locked_ &&
         registration_mode_ == policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
}

void InstallAttributes::TriggerConsistencyCheck(int dbus_retries) {
  cryptohome_client_->TpmIsOwned(
      base::Bind(&InstallAttributes::OnTpmOwnerCheckCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 dbus_retries));
}

void InstallAttributes::OnTpmOwnerCheckCompleted(
    int dbus_retries_remaining,
    DBusMethodCallStatus call_status,
    bool result) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS && dbus_retries_remaining) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&InstallAttributes::TriggerConsistencyCheck,
                   weak_ptr_factory_.GetWeakPtr(), dbus_retries_remaining - 1),
        base::TimeDelta::FromSeconds(kDbusRetryIntervalInSeconds));
    return;
  }

  base::HistogramBase::Sample state = device_locked_;
  state |= 0x2 * (registration_mode_ == policy::DEVICE_MODE_ENTERPRISE);
  if (call_status == DBUS_METHOD_CALL_SUCCESS)
    state |= 0x4 * result;
  else
    state = 0x8;  // This case is not a bit mask.
  UMA_HISTOGRAM_ENUMERATION("Enterprise.AttributesTPMConsistency", state, 9);

  // Run any action (LockDevice call) that might have queued behind the
  // consistency check.
  consistency_check_running_ = false;
  if (!post_check_action_.is_null()) {
    post_check_action_.Run();
    post_check_action_.Reset();
  }
}

// Warning: The values for these keys (but not the keys themselves) are stored
// in the protobuf with a trailing zero.  Also note that some of these constants
// have been copied to login_manager/device_policy_service.cc.  Please make sure
// that all changes to the constants are reflected there as well.
const char InstallAttributes::kConsumerDeviceMode[] = "consumer";
const char InstallAttributes::kEnterpriseDeviceMode[] = "enterprise";
const char InstallAttributes::kEnterpriseADDeviceMode[] = "enterprise_ad";
const char InstallAttributes::kLegacyRetailDeviceMode[] = "kiosk";
const char InstallAttributes::kConsumerKioskDeviceMode[] = "consumer_kiosk";

const char InstallAttributes::kAttrEnterpriseDeviceId[] =
    "enterprise.device_id";
const char InstallAttributes::kAttrEnterpriseDomain[] = "enterprise.domain";
const char InstallAttributes::kAttrEnterpriseRealm[] = "enterprise.realm";
const char InstallAttributes::kAttrEnterpriseMode[] = "enterprise.mode";
const char InstallAttributes::kAttrEnterpriseOwned[] = "enterprise.owned";
const char InstallAttributes::kAttrEnterpriseUser[] = "enterprise.user";
const char InstallAttributes::kAttrConsumerKioskEnabled[] =
    "consumer.app_kiosk_enabled";

void InstallAttributes::OnCryptohomeServiceInitiallyAvailable(
    bool service_is_ready) {
  if (!service_is_ready)
    LOG(ERROR) << "Failed waiting for cryptohome D-Bus service availability.";

  // Start the consistency check even if we failed to wait for availability;
  // hopefully the service will become available eventually.
  TriggerConsistencyCheck(kDbusRetryCount);
}

std::string InstallAttributes::GetDeviceModeString(policy::DeviceMode mode) {
  switch (mode) {
    case policy::DEVICE_MODE_CONSUMER:
      return InstallAttributes::kConsumerDeviceMode;
    case policy::DEVICE_MODE_ENTERPRISE:
      return InstallAttributes::kEnterpriseDeviceMode;
    case policy::DEVICE_MODE_ENTERPRISE_AD:
      return InstallAttributes::kEnterpriseADDeviceMode;
    case policy::DEVICE_MODE_LEGACY_RETAIL_MODE:
      return InstallAttributes::kLegacyRetailDeviceMode;
    case policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
      return InstallAttributes::kConsumerKioskDeviceMode;
    case policy::DEVICE_MODE_PENDING:
    case policy::DEVICE_MODE_NOT_SET:
      break;
  }
  NOTREACHED() << "Invalid device mode: " << mode;
  return std::string();
}

policy::DeviceMode InstallAttributes::GetDeviceModeFromString(
    const std::string& mode) {
  if (mode == InstallAttributes::kConsumerDeviceMode)
    return policy::DEVICE_MODE_CONSUMER;
  else if (mode == InstallAttributes::kEnterpriseDeviceMode)
    return policy::DEVICE_MODE_ENTERPRISE;
  else if (mode == InstallAttributes::kEnterpriseADDeviceMode)
    return policy::DEVICE_MODE_ENTERPRISE_AD;
  else if (mode == InstallAttributes::kLegacyRetailDeviceMode)
    return policy::DEVICE_MODE_LEGACY_RETAIL_MODE;
  else if (mode == InstallAttributes::kConsumerKioskDeviceMode)
    return policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
  return policy::DEVICE_MODE_NOT_SET;
}

void InstallAttributes::DecodeInstallAttributes(
    const std::map<std::string, std::string>& attr_map) {
  // Start from a clean slate.
  registration_mode_ = policy::DEVICE_MODE_NOT_SET;
  registration_domain_.clear();
  registration_realm_.clear();
  registration_device_id_.clear();

  const std::string enterprise_owned =
      ReadMapKey(attr_map, kAttrEnterpriseOwned);
  const std::string consumer_kiosk_enabled =
      ReadMapKey(attr_map, kAttrConsumerKioskEnabled);
  const std::string mode = ReadMapKey(attr_map, kAttrEnterpriseMode);
  const std::string domain = ReadMapKey(attr_map, kAttrEnterpriseDomain);
  const std::string realm = ReadMapKey(attr_map, kAttrEnterpriseRealm);
  const std::string device_id = ReadMapKey(attr_map, kAttrEnterpriseDeviceId);
  const std::string user_deprecated = ReadMapKey(attr_map, kAttrEnterpriseUser);

  if (enterprise_owned == "true") {
    WarnIfNonempty(attr_map, kAttrConsumerKioskEnabled);
    registration_device_id_ = device_id;

    // Set registration_mode_.
    registration_mode_ = GetDeviceModeFromString(mode);
    if (registration_mode_ != policy::DEVICE_MODE_ENTERPRISE &&
        registration_mode_ != policy::DEVICE_MODE_ENTERPRISE_AD) {
      if (!mode.empty()) {
        LOG(WARNING) << "Bad " << kAttrEnterpriseMode << ": " << mode;
      }
      registration_mode_ = policy::DEVICE_MODE_ENTERPRISE;
    }

    if (registration_mode_ == policy::DEVICE_MODE_ENTERPRISE) {
      // Either set registration_domain_ ...
      WarnIfNonempty(attr_map, kAttrEnterpriseRealm);
      if (!domain.empty()) {
        // The canonicalization is for compatibility with earlier versions.
        registration_domain_ = gaia::CanonicalizeDomain(domain);
      } else if (!user_deprecated.empty()) {
        // Compatibility for pre M19 code.
        registration_domain_ = gaia::ExtractDomainName(user_deprecated);
      } else {
        LOG(WARNING) << "Couldn't read domain.";
      }
    } else {
      // ... or set registration_realm_.
      WarnIfNonempty(attr_map, kAttrEnterpriseDomain);
      if (!realm.empty()) {
        registration_realm_ = realm;
      } else {
        LOG(WARNING) << "Couldn't read realm.";
      }
    }

    return;
  }

  WarnIfNonempty(attr_map, kAttrEnterpriseOwned);
  WarnIfNonempty(attr_map, kAttrEnterpriseDomain);
  WarnIfNonempty(attr_map, kAttrEnterpriseRealm);
  WarnIfNonempty(attr_map, kAttrEnterpriseDeviceId);
  WarnIfNonempty(attr_map, kAttrEnterpriseUser);
  if (consumer_kiosk_enabled == "true") {
    registration_mode_ = policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
    return;
  }

  WarnIfNonempty(attr_map, kAttrConsumerKioskEnabled);
  if (user_deprecated.empty()) {
    registration_mode_ = policy::DEVICE_MODE_CONSUMER;
  }
}

}  // namespace chromeos
