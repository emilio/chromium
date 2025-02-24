// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class BackgroundFetchJobData;
class BackgroundFetchRequestInfo;
class BrowserContext;
class StoragePartition;

// The JobController will be responsible for coordinating communication with the
// DownloadManager. It will get requests from the JobData and dispatch them to
// the DownloadManager. It lives entirely on the IO thread.
// TODO(harkness): The JobController should also observe downloads.
class CONTENT_EXPORT BackgroundFetchJobController {
 public:
  BackgroundFetchJobController(
      const std::string& job_guid,
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      std::unique_ptr<BackgroundFetchJobData> job_data);
  ~BackgroundFetchJobController();

  // Start processing on a batch of requests. Some of these may already be in
  // progress or completed from a previous chromium instance.
  void StartProcessing();

  // Called by the BackgroundFetchContext when the system is shutting down.
  void Shutdown();

 private:
  void ProcessRequest(const BackgroundFetchRequestInfo& request);

  // Pointer to the browser context. The BackgroundFetchJobController is owned
  // by the BrowserContext via the StoragePartition.
  BrowserContext* browser_context_;

  // Pointer to the storage partition. This object is owned by the partition
  // (through a sequence of other classes).
  StoragePartition* storage_partition_;

  // The JobData which talks to the DataManager for this job_guid.
  std::unique_ptr<BackgroundFetchJobData> job_data_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
