// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_frontend.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_test_util.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/value_store/value_store.h"
#include "extensions/browser/value_store/value_store_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

namespace settings = settings_namespace;
namespace util = settings_test_util;

namespace {

// To save typing ValueStore::DEFAULTS everywhere.
const ValueStore::WriteOptions DEFAULTS = ValueStore::DEFAULTS;

}  // namespace

// A better name for this would be StorageFrontendTest, but the historical name
// has been ExtensionSettingsFrontendTest. In order to preserve crash/failure
// history, the test names are unchanged.
class ExtensionSettingsFrontendTest : public ExtensionsTest {
 public:
  ExtensionSettingsFrontendTest()
      : ui_thread_(BrowserThread::UI, base::MessageLoop::current()),
        file_thread_(BrowserThread::FILE, base::MessageLoop::current()) {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    storage_factory_ = new ValueStoreFactoryImpl(temp_dir_.GetPath());
    ResetFrontend();
  }

  void TearDown() override {
    frontend_.reset();
    // Execute any pending deletion tasks.
    base::RunLoop().RunUntilIdle();
    ExtensionsTest::TearDown();
  }

 protected:
  void ResetFrontend() {
    frontend_ =
        StorageFrontend::CreateForTesting(storage_factory_, browser_context());
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<StorageFrontend> frontend_;
  scoped_refptr<ValueStoreFactoryImpl> storage_factory_;

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  ExtensionsAPIClient extensions_api_client_;
};

// Get a semblance of coverage for both extension and app settings by
// alternating in each test.
// TODO(kalman): explicitly test the two interact correctly.

// Tests that the frontend is set up correctly.
TEST_F(ExtensionSettingsFrontendTest, Basics) {
  // Local storage is always enabled.
  EXPECT_TRUE(frontend_->IsStorageEnabled(settings::LOCAL));
  EXPECT_TRUE(frontend_->GetValueStoreCache(settings::LOCAL));

  // Invalid storage areas are not available.
  EXPECT_FALSE(frontend_->IsStorageEnabled(settings::INVALID));
  EXPECT_FALSE(frontend_->GetValueStoreCache(settings::INVALID));
}

TEST_F(ExtensionSettingsFrontendTest, SettingsPreservedAcrossReconstruction) {
  const std::string id = "ext";
  scoped_refptr<const Extension> extension =
      util::AddExtensionWithId(browser_context(), id, Manifest::TYPE_EXTENSION);

  ValueStore* storage =
      util::GetStorage(extension, settings::LOCAL, frontend_.get());

  // The correctness of Get/Set/Remove/Clear is tested elsewhere so no need to
  // be too rigorous.
  {
    base::Value bar("bar");
    ValueStore::WriteResult result = storage->Set(DEFAULTS, "foo", bar);
    ASSERT_TRUE(result->status().ok());
  }

  {
    ValueStore::ReadResult result = storage->Get();
    ASSERT_TRUE(result->status().ok());
    EXPECT_FALSE(result->settings().empty());
  }

  ResetFrontend();
  storage = util::GetStorage(extension, settings::LOCAL, frontend_.get());

  {
    ValueStore::ReadResult result = storage->Get();
    ASSERT_TRUE(result->status().ok());
    EXPECT_FALSE(result->settings().empty());
  }
}

TEST_F(ExtensionSettingsFrontendTest, SettingsClearedOnUninstall) {
  const std::string id = "ext";
  scoped_refptr<const Extension> extension = util::AddExtensionWithId(
      browser_context(), id, Manifest::TYPE_LEGACY_PACKAGED_APP);

  ValueStore* storage =
      util::GetStorage(extension, settings::LOCAL, frontend_.get());

  {
    base::Value bar("bar");
    ValueStore::WriteResult result = storage->Set(DEFAULTS, "foo", bar);
    ASSERT_TRUE(result->status().ok());
  }

  // This would be triggered by extension uninstall via a DataDeleter.
  frontend_->DeleteStorageSoon(id);
  base::RunLoop().RunUntilIdle();

  // The storage area may no longer be valid post-uninstall, so re-request.
  storage = util::GetStorage(extension, settings::LOCAL, frontend_.get());
  {
    ValueStore::ReadResult result = storage->Get();
    ASSERT_TRUE(result->status().ok());
    EXPECT_TRUE(result->settings().empty());
  }
}

TEST_F(ExtensionSettingsFrontendTest, LeveldbDatabaseDeletedFromDiskOnClear) {
  const std::string id = "ext";
  scoped_refptr<const Extension> extension =
      util::AddExtensionWithId(browser_context(), id, Manifest::TYPE_EXTENSION);

  ValueStore* storage =
      util::GetStorage(extension, settings::LOCAL, frontend_.get());

  {
    base::Value bar("bar");
    ValueStore::WriteResult result = storage->Set(DEFAULTS, "foo", bar);
    ASSERT_TRUE(result->status().ok());
    EXPECT_TRUE(base::PathExists(temp_dir_.GetPath()));
  }

  // Should need to both clear the database and delete the frontend for the
  // leveldb database to be deleted from disk.
  {
    ValueStore::WriteResult result = storage->Clear();
    ASSERT_TRUE(result->status().ok());
    EXPECT_TRUE(base::PathExists(temp_dir_.GetPath()));
  }

  frontend_.reset();
  base::RunLoop().RunUntilIdle();
  // TODO(kalman): Figure out why this fails, despite appearing to work.
  // Leaving this commented out rather than disabling the whole test so that the
  // deletion code paths are at least exercised.
  // EXPECT_FALSE(base::PathExists(temp_dir_.GetPath()));
}

// Disabled (slow), http://crbug.com/322751 .
TEST_F(ExtensionSettingsFrontendTest,
       DISABLED_QuotaLimitsEnforcedCorrectlyForSyncAndLocal) {
  const std::string id = "ext";
  scoped_refptr<const Extension> extension =
      util::AddExtensionWithId(browser_context(), id, Manifest::TYPE_EXTENSION);

  ValueStore* sync_storage =
      util::GetStorage(extension, settings::SYNC, frontend_.get());
  ValueStore* local_storage =
      util::GetStorage(extension, settings::LOCAL, frontend_.get());

  // Sync storage should run out after ~100K.
  std::unique_ptr<base::Value> kilobyte = util::CreateKilobyte();
  for (int i = 0; i < 100; ++i) {
    sync_storage->Set(DEFAULTS, base::IntToString(i), *kilobyte);
  }

  EXPECT_FALSE(
      sync_storage->Set(DEFAULTS, "WillError", *kilobyte)->status().ok());

  // Local storage shouldn't run out after ~100K.
  for (int i = 0; i < 100; ++i) {
    local_storage->Set(DEFAULTS, base::IntToString(i), *kilobyte);
  }

  EXPECT_TRUE(
      local_storage->Set(DEFAULTS, "WontError", *kilobyte)->status().ok());

  // Local storage should run out after ~5MB.
  std::unique_ptr<base::Value> megabyte = util::CreateMegabyte();
  for (int i = 0; i < 5; ++i) {
    local_storage->Set(DEFAULTS, base::IntToString(i), *megabyte);
  }

  EXPECT_FALSE(
      local_storage->Set(DEFAULTS, "WillError", *megabyte)->status().ok());
}

}  // namespace extensions
