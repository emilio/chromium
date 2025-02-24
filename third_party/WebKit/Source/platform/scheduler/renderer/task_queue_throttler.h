// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_QUEUE_THROTTLER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_QUEUE_THROTTLER_H_

#include <set>
#include <unordered_map>

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "platform/scheduler/base/cancelable_closure_holder.h"
#include "platform/scheduler/base/time_domain.h"
#include "public/platform/WebViewScheduler.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace blink {
namespace scheduler {

class BudgetPool;
class RendererSchedulerImpl;
class ThrottledTimeDomain;
class CPUTimeBudgetPool;

// The job of the TaskQueueThrottler is to control when tasks posted on
// throttled queues get run. The TaskQueueThrottler:
// - runs throttled tasks once per second,
// - controls time budget for task queues grouped in CPUTimeBudgetPools.
//
// This is done by disabling throttled queues and running
// a special "heart beat" function |PumpThrottledTasks| which when run
// temporarily enables throttled queues and inserts a fence to ensure tasks
// posted from a throttled task run next time the queue is pumped.
//
// Of course the TaskQueueThrottler isn't the only sub-system that wants to
// enable or disable queues. E.g. RendererSchedulerImpl also does this for
// policy reasons. To prevent the systems from fighting, clients of
// TaskQueueThrottler must use SetQueueEnabled rather than calling the function
// directly on the queue.
//
// There may be more than one system that wishes to throttle a queue (e.g.
// renderer suspension vs tab level suspension) so the TaskQueueThrottler keeps
// a count of the number of systems that wish a queue to be throttled.
// See IncreaseThrottleRefCount & DecreaseThrottleRefCount.
//
// This class is main-thread only.
class BLINK_PLATFORM_EXPORT TaskQueueThrottler : public TimeDomain::Observer {
 public:
  // TODO(altimin): Do not pass tracing category as const char*,
  // hard-code string instead.
  TaskQueueThrottler(RendererSchedulerImpl* renderer_scheduler,
                     const char* tracing_category);

  ~TaskQueueThrottler() override;

  // TimeDomain::Observer implementation:
  void OnTimeDomainHasImmediateWork(TaskQueue*) override;
  void OnTimeDomainHasDelayedWork(TaskQueue*) override;

  // Increments the throttled refcount and causes |task_queue| to be throttled
  // if its not already throttled.
  void IncreaseThrottleRefCount(TaskQueue* task_queue);

  // If the refcouint is non-zero it's decremented.  If the throttled refcount
  // becomes zero then |task_queue| is unthrottled.  If the refcount was already
  // zero this function does nothing.
  void DecreaseThrottleRefCount(TaskQueue* task_queue);

  // Removes |task_queue| from |queue_details| and from appropriate budget pool.
  void UnregisterTaskQueue(TaskQueue* task_queue);

  // Returns true if the |task_queue| is throttled.
  bool IsThrottled(TaskQueue* task_queue) const;

  // Disable throttling for all queues, this setting takes precedence over
  // all other throttling settings. Designed to be used when a global event
  // disabling throttling happens (e.g. audio is playing).
  void DisableThrottling();

  // Enable back global throttling.
  void EnableThrottling();

  const ThrottledTimeDomain* time_domain() const { return time_domain_.get(); }

  static base::TimeTicks AlignedThrottledRunTime(
      base::TimeTicks unthrottled_runtime);

  const scoped_refptr<TaskQueue>& task_runner() const { return task_runner_; }

  // Returned object is owned by |TaskQueueThrottler|.
  CPUTimeBudgetPool* CreateCPUTimeBudgetPool(
      const char* name,
      base::Optional<base::TimeDelta> max_budget_level,
      base::Optional<base::TimeDelta> max_throttling_duration);

  // Accounts for given task for cpu-based throttling needs.
  void OnTaskRunTimeReported(TaskQueue* task_queue,
                             base::TimeTicks start_time,
                             base::TimeTicks end_time);

  void AsValueInto(base::trace_event::TracedValue* state,
                   base::TimeTicks now) const;

 private:
  friend class BudgetPool;
  friend class CPUTimeBudgetPool;

  struct Metadata {
    Metadata() : throttling_ref_count(0), time_budget_pool(nullptr) {}

    size_t throttling_ref_count;

    CPUTimeBudgetPool* time_budget_pool;
  };
  using TaskQueueMap = std::unordered_map<TaskQueue*, Metadata>;

  void PumpThrottledTasks();

  // Note |unthrottled_runtime| might be in the past. When this happens we
  // compute the delay to the next runtime based on now rather than
  // unthrottled_runtime.
  void MaybeSchedulePumpThrottledTasks(
      const tracked_objects::Location& from_here,
      base::TimeTicks now,
      base::TimeTicks runtime);

  CPUTimeBudgetPool* GetTimeBudgetPoolForQueue(TaskQueue* queue);

  // Schedule pumping because of given task queue.
  void MaybeSchedulePumpQueue(
      const tracked_objects::Location& from_here,
      base::TimeTicks now,
      TaskQueue* queue,
      base::Optional<base::TimeTicks> next_possible_run_time);

  // Return next possible time when queue is allowed to run in accordance
  // with throttling policy.
  base::TimeTicks GetNextAllowedRunTime(base::TimeTicks now, TaskQueue* queue);

  void MaybeDeleteQueueMetadata(TaskQueueMap::iterator it);

  TaskQueueMap queue_details_;
  base::Callback<void(TaskQueue*)> forward_immediate_work_callback_;
  scoped_refptr<TaskQueue> task_runner_;
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED
  base::TickClock* tick_clock_;                // NOT OWNED
  const char* tracing_category_;               // NOT OWNED
  std::unique_ptr<ThrottledTimeDomain> time_domain_;

  CancelableClosureHolder pump_throttled_tasks_closure_;
  base::Optional<base::TimeTicks> pending_pump_throttled_tasks_runtime_;
  bool allow_throttling_;

  std::unordered_map<BudgetPool*, std::unique_ptr<BudgetPool>>
      time_budget_pools_;

  base::WeakPtrFactory<TaskQueueThrottler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueThrottler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_TASK_QUEUE_THROTTLER_H_
