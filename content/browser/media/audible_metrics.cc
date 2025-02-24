// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audible_metrics.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/default_tick_clock.h"

namespace content {

AudibleMetrics::AudibleMetrics()
    : max_concurrent_audible_web_contents_in_session_(0),
      clock_(new base::DefaultTickClock()) {
}

AudibleMetrics::~AudibleMetrics() {
}

void AudibleMetrics::UpdateAudibleWebContentsState(
    const WebContents* web_contents, bool audible) {
  bool found =
      audible_web_contents_.find(web_contents) != audible_web_contents_.end();
  if (found == audible)
    return;

  if (audible)
    AddAudibleWebContents(web_contents);
  else
    RemoveAudibleWebContents(web_contents);
}

void AudibleMetrics::SetClockForTest(
    std::unique_ptr<base::TickClock> test_clock) {
  clock_ = std::move(test_clock);
}

void AudibleMetrics::AddAudibleWebContents(const WebContents* web_contents) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Media.Audible.ConcurrentTabsWhenStarting", audible_web_contents_.size(),
      1, 10, 11);

  audible_web_contents_.insert(web_contents);
  if (audible_web_contents_.size() > 1 &&
      concurrent_web_contents_start_time_.is_null()) {
    concurrent_web_contents_start_time_ = clock_->NowTicks();
  }

  if (audible_web_contents_.size() >
      max_concurrent_audible_web_contents_in_session_) {
    max_concurrent_audible_web_contents_in_session_ =
        audible_web_contents_.size();

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Media.Audible.MaxConcurrentTabsInSession",
        max_concurrent_audible_web_contents_in_session_,
        1, 10, 11);
  }
}

void AudibleMetrics::RemoveAudibleWebContents(const WebContents* web_contents) {
  audible_web_contents_.erase(web_contents);

  if (audible_web_contents_.size() <= 1 &&
      !concurrent_web_contents_start_time_.is_null()) {
    base::TimeDelta concurrent_total_time =
        clock_->NowTicks() - concurrent_web_contents_start_time_;
    concurrent_web_contents_start_time_ = base::TimeTicks();

    UMA_HISTOGRAM_LONG_TIMES("Media.Audible.ConcurrentTabsTime",
                             concurrent_total_time);
  }
}

}  // namespace content
