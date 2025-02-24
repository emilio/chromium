// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_H_
#define CONTENT_RENDERER_RENDER_PROCESS_H_

#include <vector>

#include "base/macros.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/task_scheduler.h"
#include "content/child/child_process.h"

namespace content {

// A abstract interface representing the renderer end of the browser<->renderer
// connection. The opposite end is the RenderProcessHost. This is a singleton
// object for each renderer.
//
// RenderProcessImpl implements this interface for the regular browser.
// MockRenderProcess implements this interface for certain tests, especially
// ones derived from RenderViewTest.
class RenderProcess : public ChildProcess {
 public:
  RenderProcess() = default;
  RenderProcess(
      const std::vector<base::SchedulerWorkerPoolParams>& worker_pool_params,
      base::TaskScheduler::WorkerPoolIndexForTraitsCallback
          worker_pool_index_for_traits_callback);
  ~RenderProcess() override {}

  // Keep track of the cumulative set of enabled bindings for this process,
  // across any view.
  virtual void AddBindings(int bindings) = 0;

  // The cumulative set of enabled bindings for this process.
  virtual int GetEnabledBindings() const = 0;

  // Returns a pointer to the RenderProcess singleton instance. Assuming that
  // we're actually a renderer or a renderer test, this static cast will
  // be correct.
  static RenderProcess* current() {
    return static_cast<RenderProcess*>(ChildProcess::current());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderProcess);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_PROCESS_H_
