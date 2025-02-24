// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOrigin[] = "https://example.com/";
const char kJobGuid[] = "TestRequestGuid";
constexpr int64_t kServiceWorkerRegistrationId = 9001;
const char kTestUrl[] = "http://www.example.com/example.html";
const char kTag[] = "testTag";

}  // namespace

namespace content {

class BackgroundFetchJobControllerTest : public ::testing::Test {
 public:
  BackgroundFetchJobControllerTest()
      : download_manager_(new MockDownloadManager()) {}
  ~BackgroundFetchJobControllerTest() override = default;

  void SetUp() override {
    // The download_manager_ ownership is given to the BrowserContext, and the
    // BrowserContext will take care of deallocating it.
    BrowserContext::SetDownloadManagerForTesting(&browser_context_,
                                                 download_manager_);
  }

  void InitializeJobController(
      std::unique_ptr<BackgroundFetchJobData> job_data) {
    job_controller_ = base::MakeUnique<BackgroundFetchJobController>(
        kJobGuid, &browser_context_,
        BrowserContext::GetDefaultStoragePartition(&browser_context_),
        std::move(job_data));
  }

  void StartProcessing() {
    base::RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BackgroundFetchJobControllerTest::StartProcessingOnIO,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void StartProcessingOnIO(const base::Closure& closure) {
    job_controller_->StartProcessing();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, closure);
  }

  BackgroundFetchJobController* job_controller() {
    return job_controller_.get();
  }
  MockDownloadManager* download_manager() { return download_manager_; }

 private:
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<BackgroundFetchJobController> job_controller_;
  MockDownloadManager* download_manager_;
};

TEST_F(BackgroundFetchJobControllerTest, StartDownload) {
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);
  BackgroundFetchRequestInfo request_info(GURL(kTestUrl), kJobGuid);
  std::vector<BackgroundFetchRequestInfo> request_infos{request_info};

  // Get a JobData to give to the JobController. The JobController then gets
  // the BackgroundFetchRequestInfos from the JobData.
  std::unique_ptr<BackgroundFetchJobData> job_data =
      base::MakeUnique<BackgroundFetchJobData>(request_infos);
  InitializeJobController(std::move(job_data));

  EXPECT_CALL(*(download_manager()),
              DownloadUrlMock(::testing::Pointee(::testing::Property(
                  &DownloadUrlParameters::url, GURL(kTestUrl)))))
      .Times(1);

  StartProcessing();
}

}  // namespace content
