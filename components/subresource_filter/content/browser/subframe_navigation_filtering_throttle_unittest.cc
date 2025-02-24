// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

class SubframeNavigationFilteringThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  SubframeNavigationFilteringThrottleTest() {}
  ~SubframeNavigationFilteringThrottleTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.test"));
    Observe(RenderViewHostTestHarness::web_contents());
  }

  void TearDown() override {
    dealer_handle_.reset();
    ruleset_handle_.reset();
    parent_filter_.reset();
    RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
  }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_FALSE(navigation_handle->IsInMainFrame());
    // The |parent_filter_| is the parent frame's filter. Do not register a
    // throttle if the parent is not activated with a valid filter.
    if (parent_filter_) {
      navigation_handle->RegisterThrottleForTesting(
          base::MakeUnique<SubframeNavigationFilteringThrottle>(
              navigation_handle, parent_filter_.get()));
    }
  }

  void InitializeDocumentSubresourceFilter(const GURL& document_url) {
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            "disallowed.html", &test_ruleset_pair_));

    // Make the blocking task runner run on the current task runner for the
    // tests, to ensure that the NavigationSimulator properly runs all necessary
    // tasks while waiting for throttle checks to finish.
    dealer_handle_ = base::MakeUnique<VerifiedRulesetDealer::Handle>(
        base::MessageLoop::current()->task_runner());
    dealer_handle_->SetRulesetFile(
        testing::TestRuleset::Open(test_ruleset_pair_.indexed));
    ruleset_handle_ =
        base::MakeUnique<VerifiedRuleset::Handle>(dealer_handle_.get());

    parent_filter_ = base::MakeUnique<AsyncDocumentSubresourceFilter>(
        ruleset_handle_.get(),
        AsyncDocumentSubresourceFilter::InitializationParams(
            document_url, ActivationLevel::ENABLED,
            false /* measure_performance */),
        base::Bind([](ActivationState state) {
          EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
        }),
        base::OnceClosure());
    RunUntilIdle();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void CreateTestSubframeAndInitNavigation(const GURL& first_url,
                                           content::RenderFrameHost* parent) {
    content::RenderFrameHost* render_frame =
        content::RenderFrameHostTester::For(parent)->AppendChild(
            base::StringPrintf("subframe-%s", first_url.spec().c_str()));
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              render_frame);
  }

  void SimulateStartAndExpectResult(
      content::NavigationThrottle::ThrottleCheckResult expect_result) {
    navigation_simulator_->Start();
    EXPECT_EQ(expect_result,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

  void SimulateRedirectAndExpectResult(
      const GURL& new_url,
      content::NavigationThrottle::ThrottleCheckResult expect_result) {
    navigation_simulator_->Redirect(new_url);
    EXPECT_EQ(expect_result,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

  void SimulateCommitAndExpectResult(
      content::NavigationThrottle::ThrottleCheckResult expect_result) {
    navigation_simulator_->Commit();
    EXPECT_EQ(expect_result,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;
  std::unique_ptr<VerifiedRuleset::Handle> ruleset_handle_;

  std::unique_ptr<AsyncDocumentSubresourceFilter> parent_filter_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  DISALLOW_COPY_AND_ASSIGN(SubframeNavigationFilteringThrottleTest);
};

TEST_F(SubframeNavigationFilteringThrottleTest, FilterOnStart) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(
      GURL("https://example.test/disallowed.html"), main_rfh());
  SimulateStartAndExpectResult(content::NavigationThrottle::CANCEL);
}

TEST_F(SubframeNavigationFilteringThrottleTest, FilterOnRedirect) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());

  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  SimulateRedirectAndExpectResult(GURL("https://example.test/disallowed.html"),
                                  content::NavigationThrottle::CANCEL);
}

TEST_F(SubframeNavigationFilteringThrottleTest, FilterOnSecondRedirect) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());

  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  SimulateRedirectAndExpectResult(GURL("https://example.test/allowed2.html"),
                                  content::NavigationThrottle::PROCEED);
  SimulateRedirectAndExpectResult(GURL("https://example.test/disallowed.html"),
                                  content::NavigationThrottle::CANCEL);
}

TEST_F(SubframeNavigationFilteringThrottleTest, NeverFilterNonMatchingRule) {
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  CreateTestSubframeAndInitNavigation(GURL("https://example.test/allowed.html"),
                                      main_rfh());

  SimulateStartAndExpectResult(content::NavigationThrottle::PROCEED);
  SimulateRedirectAndExpectResult(GURL("https://example.test/allowed2.html"),
                                  content::NavigationThrottle::PROCEED);
  SimulateCommitAndExpectResult(content::NavigationThrottle::PROCEED);
}

TEST_F(SubframeNavigationFilteringThrottleTest, FilterSubsubframe) {
  // Fake an activation of the subframe.
  content::RenderFrameHost* parent_subframe =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("parent-sub");
  GURL test_url = GURL("https://example.test");
  content::RenderFrameHostTester::For(parent_subframe)
      ->SimulateNavigationStart(test_url);
  InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  content::RenderFrameHostTester::For(parent_subframe)
      ->SimulateNavigationCommit(test_url);

  CreateTestSubframeAndInitNavigation(
      GURL("https://example.test/disallowed.html"), parent_subframe);
  SimulateStartAndExpectResult(content::NavigationThrottle::CANCEL);
}

}  // namespace subresource_filter
