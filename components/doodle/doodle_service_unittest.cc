// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/doodle/doodle_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::StrictMock;

namespace doodle {

namespace {

class FakeDoodleFetcher : public DoodleFetcher {
 public:
  FakeDoodleFetcher() = default;
  ~FakeDoodleFetcher() override = default;

  void FetchDoodle(FinishedCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }

  size_t num_pending_callbacks() const { return callbacks_.size(); }

  void ServeAllCallbacks(DoodleState state,
                         base::TimeDelta time_to_live,
                         const base::Optional<DoodleConfig>& config) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(state, time_to_live, config);
    }
    callbacks_.clear();
  }

 private:
  std::vector<FinishedCallback> callbacks_;
};

class MockDoodleObserver : public DoodleService::Observer {
 public:
  MOCK_METHOD1(OnDoodleConfigUpdated,
               void(const base::Optional<DoodleConfig>&));
};

}  // namespace

class DoodleServiceTest : public testing::Test {
 public:
  DoodleServiceTest()
      : fetcher_(nullptr),
        task_runner_(new base::TestMockTimeTaskRunner()),
        task_runner_handle_(task_runner_),
        tick_clock_(task_runner_->GetMockTickClock()),
        expiry_timer_(nullptr) {
    task_runner_->FastForwardBy(base::TimeDelta::FromHours(12345));

    auto expiry_timer = base::MakeUnique<base::OneShotTimer>(tick_clock_.get());
    expiry_timer->SetTaskRunner(task_runner_);
    expiry_timer_ = expiry_timer.get();

    auto fetcher = base::MakeUnique<FakeDoodleFetcher>();
    fetcher_ = fetcher.get();

    service_ = base::MakeUnique<DoodleService>(std::move(fetcher),
                                               std::move(expiry_timer));
  }

  DoodleService* service() { return service_.get(); }
  FakeDoodleFetcher* fetcher() { return fetcher_; }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  // Weak, owned by the service.
  FakeDoodleFetcher* fetcher_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<base::TickClock> tick_clock_;
  // Weak, owned by the service.
  base::OneShotTimer* expiry_timer_;

  std::unique_ptr<DoodleService> service_;
};

TEST_F(DoodleServiceTest, FetchesConfigOnRefresh) {
  ASSERT_THAT(service()->config(), Eq(base::nullopt));

  // Request a refresh of the doodle config.
  service()->Refresh();
  // The request should have arrived at the fetcher.
  EXPECT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve it (with an arbitrary config).
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), config);

  // The config should be available.
  EXPECT_THAT(service()->config(), Eq(config));

  // Request a refresh again.
  service()->Refresh();
  // The request should have arrived at the fetcher again.
  EXPECT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve it with a different config.
  DoodleConfig other_config;
  other_config.doodle_type = DoodleType::SLIDESHOW;
  DCHECK(config != other_config);
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), other_config);

  // The config should have been updated.
  EXPECT_THAT(service()->config(), Eq(other_config));
}

TEST_F(DoodleServiceTest, CallsObserverOnConfigReceived) {
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);

  ASSERT_THAT(service()->config(), Eq(base::nullopt));

  // Request a refresh of the doodle config.
  service()->Refresh();
  // The request should have arrived at the fetcher.
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve it (with an arbitrary config). The observer should get notified.
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(config)));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), config);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, CallsObserverOnConfigRemoved) {
  // Load some doodle config.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Register an observer and request a refresh.
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);
  service()->Refresh();
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve the request with an empty doodle config. The observer should get
  // notified.
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(base::nullopt)));
  fetcher()->ServeAllCallbacks(DoodleState::NO_DOODLE, base::TimeDelta(),
                               base::nullopt);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, CallsObserverOnConfigUpdated) {
  // Load some doodle config.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Register an observer and request a refresh.
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);
  service()->Refresh();
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve the request with a different doodle config. The observer should get
  // notified.
  DoodleConfig other_config;
  other_config.doodle_type = DoodleType::SLIDESHOW;
  DCHECK(config != other_config);
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(other_config)));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), other_config);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, DoesNotCallObserverIfConfigEquivalent) {
  // Load some doodle config.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Register an observer and request a refresh.
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);
  service()->Refresh();
  ASSERT_THAT(fetcher()->num_pending_callbacks(), Eq(1u));

  // Serve the request with an equivalent doodle config. The observer should
  // *not* get notified.
  DoodleConfig equivalent_config;
  equivalent_config.doodle_type = DoodleType::SIMPLE;
  DCHECK(config == equivalent_config);
  fetcher()->ServeAllCallbacks(
      DoodleState::AVAILABLE, base::TimeDelta::FromHours(1), equivalent_config);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, CallsObserverWhenConfigExpires) {
  // Load some doodle config.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Make sure the task arrived at the timer's task runner.
  ASSERT_THAT(task_runner()->GetPendingTaskCount(), Eq(1u));
  EXPECT_THAT(task_runner()->NextPendingTaskDelay(),
              Eq(base::TimeDelta::FromHours(1)));

  // Register an observer.
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);

  // Fast-forward time so that the expiry task will run. The observer should get
  // notified that there's no config anymore.
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(base::nullopt)));
  task_runner()->FastForwardBy(base::TimeDelta::FromHours(1));

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

TEST_F(DoodleServiceTest, DisregardsAlreadyExpiredConfigs) {
  StrictMock<MockDoodleObserver> observer;
  service()->AddObserver(&observer);

  ASSERT_THAT(service()->config(), Eq(base::nullopt));

  // Load an already-expired config. This should have no effect; in particular
  // no call to the observer.
  service()->Refresh();
  DoodleConfig config;
  config.doodle_type = DoodleType::SIMPLE;
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromSeconds(0), config);
  EXPECT_THAT(service()->config(), Eq(base::nullopt));

  // Load a doodle config as usual. Nothing to see here.
  service()->Refresh();
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(config)));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromHours(1), config);
  ASSERT_THAT(service()->config(), Eq(config));

  // Now load an expired config again. The cached one should go away.
  service()->Refresh();
  EXPECT_CALL(observer, OnDoodleConfigUpdated(Eq(base::nullopt)));
  fetcher()->ServeAllCallbacks(DoodleState::AVAILABLE,
                               base::TimeDelta::FromSeconds(0), config);

  // Remove the observer before the service gets destroyed.
  service()->RemoveObserver(&observer);
}

}  // namespace doodle
