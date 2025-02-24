// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_store_client.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::WithArg;
using testing::WithoutArgs;
using testing::_;

namespace prefs {

namespace {

class PrefStoreObserverMock : public PrefStore::Observer {
 public:
  MOCK_METHOD1(OnPrefValueChanged, void(const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void(bool succeeded));
};

class PrefStoreConnectorMock : public mojom::PrefStoreConnector {
 public:
  MOCK_METHOD1(Connect, void(const ConnectCallback&));
};

}  // namespace

class PrefStoreClientTest : public testing::Test {
 public:
  PrefStoreClientTest() = default;
  ~PrefStoreClientTest() override {}

  PrefStoreObserverMock& observer() { return observer_; }
  PrefStoreClient* store() { return store_.get(); }

  bool initialized() { return store_->initialized_; }
  void OnPrefChanged(const std::string& key, const base::Value& value) {
    observer_ptr_->OnPrefChanged(key, value.CreateDeepCopy());
  }
  void OnInitializationCompleted() {
    observer_ptr_->OnInitializationCompleted(true);
  }

  // testing::Test:
  void SetUp() override {
    store_ = new PrefStoreClient(mojom::PrefStoreConnection::New(
        mojo::MakeRequest(&observer_ptr_),
        base::MakeUnique<base::DictionaryValue>(), false));
    store_->AddObserver(&observer_);
  }
  void TearDown() override {
    store_->RemoveObserver(&observer_);
    store_ = nullptr;
  }

 private:
  mojom::PrefStoreObserverPtr observer_ptr_;
  PrefStoreObserverMock observer_;
  scoped_refptr<PrefStoreClient> store_;

  // Required by mojo binding code within PrefStoreClient.
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreClientTest);
};

// Tests that observers are notified upon the completion of initialization, and
// that values become available.
TEST_F(PrefStoreClientTest, Initialization) {
  // The store should start out uninitialized if the backing store does.
  EXPECT_FALSE(initialized());
  EXPECT_CALL(observer(), OnInitializationCompleted(_)).Times(0);

  testing::Mock::VerifyAndClearExpectations(&observer());

  const char key[] = "hey";
  const int kValue = 42;
  base::Value pref(kValue);

  // PrefStore notifies of PreferencesChanged, completing
  // initialization.
  base::RunLoop loop;
  EXPECT_CALL(observer(), OnInitializationCompleted(true));
  EXPECT_CALL(observer(), OnPrefValueChanged(key))
      .WillOnce(WithoutArgs(Invoke([&loop]() { loop.Quit(); })));
  OnInitializationCompleted();
  OnPrefChanged(key, pref);
  loop.Run();
  EXPECT_TRUE(initialized());

  const base::Value* value = nullptr;
  int actual_value;
  EXPECT_TRUE(store()->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&actual_value));
  EXPECT_EQ(kValue, actual_value);
}

// Test that when initialized with multiple keys, that observers receive a
// notification for each key.
TEST_F(PrefStoreClientTest, MultipleKeyInitialization) {
  const char key1[] = "hey";
  const char key2[] = "listen";

  EXPECT_FALSE(initialized());
  EXPECT_CALL(observer(), OnInitializationCompleted(_)).Times(0);

  testing::Mock::VerifyAndClearExpectations(&observer());

  const int kValue = 42;
  base::Value pref1(kValue);
  base::Value pref2("look");

  base::DictionaryValue prefs;
  prefs.Set(key1, pref1.CreateDeepCopy());
  prefs.Set(key2, pref2.CreateDeepCopy());

  // The observer should be notified of all keys set.
  base::RunLoop loop;
  EXPECT_CALL(observer(), OnInitializationCompleted(true));
  EXPECT_CALL(observer(), OnPrefValueChanged(key1));
  EXPECT_CALL(observer(), OnPrefValueChanged(key2))
      .WillOnce(WithoutArgs(Invoke([&loop]() { loop.Quit(); })));
  OnInitializationCompleted();
  OnPrefChanged(key1, pref1);
  OnPrefChanged(key2, pref2);
  loop.Run();
  EXPECT_TRUE(initialized());
}

// Tests that multiple PrefStore::Observers can be added to a PrefStoreClient
// and that they are each notified of changes.
TEST_F(PrefStoreClientTest, MultipleObservers) {
  PrefStoreObserverMock observer2;
  store()->AddObserver(&observer2);

  const char key[] = "hey";
  const int kValue = 42;
  base::Value pref(kValue);

  // PrefStore notifies of PreferencesChanged, completing
  // initialization.
  base::RunLoop loop;
  EXPECT_CALL(observer(), OnInitializationCompleted(true));
  EXPECT_CALL(observer2, OnInitializationCompleted(true));
  EXPECT_CALL(observer(), OnPrefValueChanged(key));
  EXPECT_CALL(observer2, OnPrefValueChanged(key))
      .WillOnce(WithoutArgs(Invoke([&loop]() { loop.Quit(); })));
  OnInitializationCompleted();
  OnPrefChanged(key, pref);
  loop.Run();

  store()->RemoveObserver(&observer2);
}

TEST_F(PrefStoreClientTest, Initialized) {
  mojom::PrefStoreObserverPtr observer_ptr;
  PrefStoreObserverMock observer;
  const char key[] = "hey";
  const int kValue = 42;
  base::Value pref(kValue);
  auto prefs = base::MakeUnique<base::DictionaryValue>();
  prefs->Set(key, pref.CreateDeepCopy());
  auto store =
      make_scoped_refptr(new PrefStoreClient(mojom::PrefStoreConnection::New(
          mojo::MakeRequest(&observer_ptr), std::move(prefs), true)));
  store->AddObserver(&observer);

  const base::Value* value = nullptr;
  int actual_value = 0;
  EXPECT_TRUE(store->GetValue(key, &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&actual_value));
  EXPECT_EQ(kValue, actual_value);
  EXPECT_CALL(observer, OnInitializationCompleted(_)).Times(0);
  EXPECT_CALL(observer, OnPrefValueChanged(_)).Times(0);
  observer_ptr.FlushForTesting();

  store->RemoveObserver(&observer);
}

}  // namespace prefs
