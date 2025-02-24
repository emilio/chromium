// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_BUDGET_POOL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_BUDGET_POOL_H_

#include <unordered_set>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "platform/scheduler/base/lazy_now.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace blink {
namespace scheduler {

class TaskQueue;
class TaskQueueThrottler;

// BudgetPool represents a group of task queues which share a limit
// on a resource. This limit applies when task queues are already throttled
// by TaskQueueThrottler.
class BLINK_PLATFORM_EXPORT BudgetPool {
 public:
  virtual ~BudgetPool();

  virtual const char* Name() const = 0;

  // Adds |queue| to given pool. If the pool restriction does not allow
  // a task to be run immediately and |queue| is throttled, |queue| becomes
  // disabled.
  virtual void AddQueue(base::TimeTicks now, TaskQueue* queue) = 0;

  // Removes |queue| from given pool. If it is throttled, it does not
  // become enabled immediately, but a call to |PumpThrottledTasks|
  // is scheduled.
  virtual void RemoveQueue(base::TimeTicks now, TaskQueue* queue) = 0;

  // Enables this time budget pool. Queues from this pool will be
  // throttled based on their run time.
  virtual void EnableThrottling(LazyNow* now) = 0;

  // Disables with time budget pool. Queues from this pool will not be
  // throttled based on their run time. A call to |PumpThrottledTasks|
  // will be scheduled to enable this queues back again and respect
  // timer alignment. Internal budget level will not regenerate with time.
  virtual void DisableThrottling(LazyNow* now) = 0;

  virtual bool IsThrottlingEnabled() const = 0;

  // Report task run time to the budget pool.
  virtual void RecordTaskRunTime(base::TimeTicks start_time,
                                 base::TimeTicks end_time) = 0;

  // All queues should be removed before calling Close().
  virtual void Close() = 0;

  // Retuns earliest time (can be in the past) when the next task can run.
  virtual base::TimeTicks GetNextAllowedRunTime() = 0;

  // Returns true at a task can be run immediately at the given time.
  virtual bool HasEnoughBudgetToRun(base::TimeTicks now) = 0;

  // Returns state for tracing.
  virtual void AsValueInto(base::trace_event::TracedValue* state,
                           base::TimeTicks now) const = 0;
};

// CPUTimeBudgetPool represents a collection of task queues which share a limit
// on total cpu time.
class BLINK_PLATFORM_EXPORT CPUTimeBudgetPool : public BudgetPool {
 public:
  ~CPUTimeBudgetPool();

  // Throttle task queues from this time budget pool if tasks are running
  // for more than |cpu_percentage| per cent of wall time.
  // This function does not affect internal time budget level.
  void SetTimeBudgetRecoveryRate(base::TimeTicks now, double cpu_percentage);

  // Increase budget level by given value. This function DOES NOT unblock
  // queues even if they are allowed to run with increased budget level.
  void GrantAdditionalBudget(base::TimeTicks now, base::TimeDelta budget_level);

  // Set callback which will be called every time when this budget pool
  // is throttled. Throttling duration (time until the queue is allowed
  // to run again) is passed as a parameter to callback.
  void SetReportingCallback(
      base::Callback<void(base::TimeDelta)> reporting_callback);

  // BudgetPool implementation:
  const char* Name() const override;
  void AddQueue(base::TimeTicks now, TaskQueue* queue) override;
  void RemoveQueue(base::TimeTicks now, TaskQueue* queue) override;
  void EnableThrottling(LazyNow* now) override;
  void DisableThrottling(LazyNow* now) override;
  bool IsThrottlingEnabled() const override;
  void RecordTaskRunTime(base::TimeTicks start_time,
                         base::TimeTicks end_time) override;
  void Close() override;
  bool HasEnoughBudgetToRun(base::TimeTicks now) override;
  base::TimeTicks GetNextAllowedRunTime() override;
  void AsValueInto(base::trace_event::TracedValue* state,
                   base::TimeTicks now) const override;

 private:
  friend class TaskQueueThrottler;

  FRIEND_TEST_ALL_PREFIXES(TaskQueueThrottlerTest, CPUTimeBudgetPool);

  CPUTimeBudgetPool(const char* name,
                    TaskQueueThrottler* task_queue_throttler,
                    base::TimeTicks now,
                    base::Optional<base::TimeDelta> max_budget_level,
                    base::Optional<base::TimeDelta> max_throttling_duration);

  // Advances |last_checkpoint_| to |now| if needed and recalculates
  // budget level.
  void Advance(base::TimeTicks now);

  // Disable all associated throttled queues.
  void BlockThrottledQueues(base::TimeTicks now);

  // Increase |current_budget_level_| to satisfy max throttling duration
  // condition if necessary.
  // Decrease |current_budget_level_| to satisfy max budget level
  // condition if necessary.
  void EnforceBudgetLevelRestrictions();

  const char* name_;  // NOT OWNED

  TaskQueueThrottler* task_queue_throttler_;

  // Max budget level which we can accrue.
  // Tasks will be allowed to run for this time before being throttled
  // after a very long period of inactivity.
  base::Optional<base::TimeDelta> max_budget_level_;
  // Max throttling duration places a lower limit on time budget level,
  // ensuring that one long task does not cause extremely long throttling.
  // Note that this is not the guarantee that every task will run
  // after desired run time + max throttling duration, but a guarantee
  // that at least one task will be run every max_throttling_duration.
  base::Optional<base::TimeDelta> max_throttling_duration_;

  base::TimeDelta current_budget_level_;
  base::TimeTicks last_checkpoint_;
  double cpu_percentage_;
  bool is_enabled_;

  std::unordered_set<TaskQueue*> associated_task_queues_;

  base::Callback<void(base::TimeDelta)> reporting_callback_;

  DISALLOW_COPY_AND_ASSIGN(CPUTimeBudgetPool);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_BUDGET_POOL_H_
