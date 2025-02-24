// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/keychain_reauthorize.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <string.h>

#include <vector>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_generic.h"
#include "components/os_crypt/keychain_password_mac.h"
#include "crypto/apple_keychain.h"

namespace chrome {

namespace {

struct VectorScramblerTraits {
  static std::vector<uint8_t>* InvalidValue() { return nullptr; }

  static void Free(std::vector<uint8_t>* buf) {
    memset(buf->data(), 0x11, buf->size());
    delete buf;
  }
};

typedef base::ScopedGeneric<std::vector<uint8_t>*, VectorScramblerTraits>
    ScopedVectorScrambler;

// Reauthorizes the Safe Storage keychain item, which protects the randomly
// generated password that encrypts the user's saved passwords. This reads out
// the keychain item, deletes it, and re-adds it to the keychain. This works
// because the keychain uses an app's designated requirement as the ACL for
// reading an item. Chrome will be signed with a designated requirement that
// accepts both the old and new certificates.
bool KeychainReauthorize() {
  base::ScopedCFTypeRef<SecKeychainItemRef> storage_item;
  UInt32 pw_length = 0;
  void* password_data = nullptr;

  crypto::AppleKeychain keychain;
  OSStatus error = keychain.FindGenericPassword(
      nullptr, strlen(KeychainPassword::service_name),
      KeychainPassword::service_name, strlen(KeychainPassword::account_name),
      KeychainPassword::account_name, &pw_length, &password_data,
      storage_item.InitializeInto());

  base::ScopedCFTypeRef<SecKeychainItemRef> backup_item;
  std::string backup_service_name =
      std::string(KeychainPassword::service_name) + ".bak";
  if (error != noErr) {
    // If the main entry does not exist, nor does the backup, exit.
    if (keychain.FindGenericPassword(
            nullptr, backup_service_name.size(), backup_service_name.data(),
            strlen(KeychainPassword::account_name),
            KeychainPassword::account_name, &pw_length, &password_data,
            backup_item.InitializeInto()) != noErr) {
      OSSTATUS_LOG(ERROR, error)
          << "KeychainReauthorize failed. Cannot retrieve item.";
      return false;
    }
  }

  // At this point, a password was retrieved, either from the main or backup.
  ScopedVectorScrambler password;
  password.reset(new std::vector<uint8_t>(
      static_cast<uint8_t*>(password_data),
      static_cast<uint8_t*>(password_data) + pw_length));
  memset(password_data, 0x11, pw_length);
  keychain.ItemFreeContent(nullptr, password_data);

  if (backup_item.get() == nullptr) {
    // If writing the backup fails, still attempt the re-auth.
    keychain.AddGenericPassword(
        nullptr, backup_service_name.size(), backup_service_name.data(),
        strlen(KeychainPassword::account_name), KeychainPassword::account_name,
        password.get()->size(), password.get()->data(),
        backup_item.InitializeInto());
  }

  if (storage_item) {
    error = keychain.ItemDelete(storage_item);
    if (error != noErr) {
      OSSTATUS_LOG(WARNING, error)
          << "KeychainReauthorize failed to delete item.";
    }
  }

  error = keychain.AddGenericPassword(
      nullptr, strlen(KeychainPassword::service_name),
      KeychainPassword::service_name, strlen(KeychainPassword::account_name),
      KeychainPassword::account_name, password.get()->size(),
      password.get()->data(), nullptr);

  if (error != noErr) {
    OSSTATUS_LOG(ERROR, error) << "Failed to re-add storage password.";
    return false;
  }

  if (backup_item.get() == nullptr) {
    // Attempt to get the backup entry in case one exists. Ignore return value.
    // This could happen if Chrome crashed after writing the backup entry and
    // before deleting the main entry.
    keychain.FindGenericPassword(
        nullptr, backup_service_name.size(), backup_service_name.data(),
        strlen(KeychainPassword::account_name), KeychainPassword::account_name,
        &pw_length, &password_data, backup_item.InitializeInto());
  }

  if (backup_item.get()) {
    error = keychain.ItemDelete(backup_item);
    if (error != noErr) {
      OSSTATUS_LOG(WARNING, error) << "Deleting backup entry failed.";
    }
  }

  return true;
}

}  // namespace

void KeychainReauthorizeIfNeeded(NSString* pref_key, int max_tries) {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  int pref_value = [user_defaults integerForKey:pref_key];

  if (pref_value >= max_tries)
    return;

  NSString* success_pref_key = [pref_key stringByAppendingString:@"Success"];
  BOOL success_value = [user_defaults boolForKey:success_pref_key];
  if (success_value)
    return;

  if (pref_value > 0) {
    // Logs the number of previous tries that didn't complete.
    if (base::mac::AmIBundled()) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("OSX.KeychainReauthorizeIfNeeded",
                                  pref_value);
    } else {
      UMA_HISTOGRAM_SPARSE_SLOWLY("OSX.KeychainReauthorizeIfNeededAtUpdate",
                                  pref_value);
    }
  }

  ++pref_value;
  [user_defaults setInteger:pref_value forKey:pref_key];
  [user_defaults synchronize];

  bool success = KeychainReauthorize();

  if (!success)
    return;

  [user_defaults setBool:YES forKey:success_pref_key];
  [user_defaults synchronize];

  // Logs the try number (1, 2) that succeeded.
  if (base::mac::AmIBundled()) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("OSX.KeychainReauthorizeIfNeededSuccess",
                                pref_value);
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "OSX.KeychainReauthorizeIfNeededAtUpdateSuccess", pref_value);
  }
}

}  // namespace chrome
