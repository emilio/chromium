// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using page_load_metrics::PageAbortReason;

namespace internal {

const char kHistogramFromGWSDomContentLoaded[] =
    "PageLoad.Clients.FromGoogleSearch.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramFromGWSLoad[] =
    "PageLoad.Clients.FromGoogleSearch.DocumentTiming."
    "NavigationToLoadEventFired";
const char kHistogramFromGWSFirstPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming.NavigationToFirstPaint";
const char kHistogramFromGWSFirstTextPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming.NavigationToFirstTextPaint";
const char kHistogramFromGWSFirstImagePaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming.NavigationToFirstImagePaint";
const char kHistogramFromGWSFirstContentfulPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramFromGWSParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.FromGoogleSearch.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramFromGWSParseDuration[] =
    "PageLoad.Clients.FromGoogleSearch.ParseTiming.ParseDuration";
const char kHistogramFromGWSParseStart[] =
    "PageLoad.Clients.FromGoogleSearch.ParseTiming.NavigationToParseStart";

const char kHistogramFromGWSAbortNewNavigationBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.NewNavigation."
    "BeforeCommit";
const char kHistogramFromGWSAbortNewNavigationBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.NewNavigation."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortNewNavigationBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.NewNavigation."
    "AfterPaint.BeforeInteraction";
const char kHistogramFromGWSAbortStopBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Stop."
    "BeforeCommit";
const char kHistogramFromGWSAbortStopBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Stop."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortStopBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Stop."
    "AfterPaint.BeforeInteraction";
const char kHistogramFromGWSAbortCloseBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Close."
    "BeforeCommit";
const char kHistogramFromGWSAbortCloseBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Close."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortCloseBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Close."
    "AfterPaint.BeforeInteraction";
const char kHistogramFromGWSAbortOtherBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Other."
    "BeforeCommit";
const char kHistogramFromGWSAbortReloadBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Reload."
    "BeforeCommit";
const char kHistogramFromGWSAbortReloadBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Reload."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortReloadBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Reload."
    "AfterPaint.Before1sDelayedInteraction";
const char kHistogramFromGWSAbortForwardBackBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming."
    "ForwardBackNavigation.BeforeCommit";
const char kHistogramFromGWSAbortForwardBackBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming."
    "ForwardBackNavigation.AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortForwardBackBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming."
    "ForwardBackNavigation.AfterPaint.Before1sDelayedInteraction";
const char kHistogramFromGWSAbortBackgroundBeforeCommit[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Background."
    "BeforeCommit";
const char kHistogramFromGWSAbortBackgroundBeforePaint[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Background."
    "AfterCommit.BeforePaint";
const char kHistogramFromGWSAbortBackgroundBeforeInteraction[] =
    "PageLoad.Clients.FromGoogleSearch.Experimental.AbortTiming.Background."
    "AfterPaint.BeforeInteraction";

}  // namespace internal

namespace {

void LogCommittedAbortsBeforePaint(PageAbortReason abort_reason,
                                   base::TimeDelta page_end_time) {
  switch (abort_reason) {
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortStopBeforePaint,
                          page_end_time);
      break;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortCloseBeforePaint,
                          page_end_time);
      break;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortNewNavigationBeforePaint,
          page_end_time);
      break;
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortReloadBeforePaint,
                          page_end_time);
      break;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortForwardBackBeforePaint,
          page_end_time);
      break;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortBackgroundBeforePaint,
                          page_end_time);
      break;
    default:
      // These should only be logged for provisional aborts.
      DCHECK_NE(abort_reason, PageAbortReason::ABORT_OTHER);
      break;
  }
}

void LogAbortsAfterPaintBeforeInteraction(
    const page_load_metrics::PageAbortInfo& abort_info) {
  switch (abort_info.reason) {
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortStopBeforeInteraction,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortCloseBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortNewNavigationBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortReloadBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortForwardBackBeforeInteraction,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortBackgroundBeforeInteraction,
          abort_info.time_to_abort);
      break;
    default:
      // These should only be logged for provisional aborts.
      DCHECK_NE(abort_info.reason, PageAbortReason::ABORT_OTHER);
      break;
  }
}

void LogProvisionalAborts(const page_load_metrics::PageAbortInfo& abort_info) {
  switch (abort_info.reason) {
    case PageAbortReason::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortStopBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortCloseBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_OTHER:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortOtherBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortNewNavigationBeforeCommit,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSAbortReloadBeforeCommit,
                          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortForwardBackBeforeCommit,
          abort_info.time_to_abort);
      break;
    case PageAbortReason::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFromGWSAbortBackgroundBeforeCommit,
          abort_info.time_to_abort);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool WasAbortedInForeground(
    const page_load_metrics::PageLoadExtraInfo& info,
    const page_load_metrics::PageAbortInfo& abort_info) {
  if (!info.started_in_foreground ||
      abort_info.reason == PageAbortReason::ABORT_NONE)
    return false;

  base::Optional<base::TimeDelta> time_to_abort(abort_info.time_to_abort);
  if (WasStartedInForegroundOptionalEventInForeground(time_to_abort, info))
    return true;

  const base::TimeDelta time_to_first_background =
      info.first_background_time.value();
  DCHECK_GT(abort_info.time_to_abort, time_to_first_background);
  base::TimeDelta background_abort_delta =
      abort_info.time_to_abort - time_to_first_background;
  // Consider this a foregrounded abort if it occurred within 100ms of a
  // background. This is needed for closing some tabs, where the signal for
  // background is often slightly ahead of the signal for close.
  if (background_abort_delta.InMilliseconds() < 100)
    return true;
  return false;
}

bool WasAbortedBeforeInteraction(
    const page_load_metrics::PageAbortInfo& abort_info,
    const base::Optional<base::TimeDelta>& time_to_interaction) {
  // These conditions should be guaranteed by the call to
  // WasAbortedInForeground, which is called before WasAbortedBeforeInteraction
  // gets invoked.
  DCHECK(abort_info.reason != PageAbortReason::ABORT_NONE);

  if (!time_to_interaction)
    return true;
  // For the case the abort is a reload or forward_back. Since pull to
  // reload / forward_back is the most common user case such aborts being
  // triggered, add a sanitization threshold here: if the first user
  // interaction are received before a reload / forward_back in a very
  // short time, treat the interaction as a gesture to perform the abort.

  // Why 1000ms?
  // 1000ms is enough to perform a pull to reload / forward_back gesture.
  // It's also too short a time for a user to consume any content
  // revealed by the interaction.
  if (abort_info.reason == PageAbortReason::ABORT_RELOAD ||
      abort_info.reason == PageAbortReason::ABORT_FORWARD_BACK) {
    return time_to_interaction.value() +
               base::TimeDelta::FromMilliseconds(1000) >
           abort_info.time_to_abort;
  } else {
    return time_to_interaction > abort_info.time_to_abort;
  }
}

}  // namespace

// See
// https://docs.google.com/document/d/1jNPZ6Aeh0KV6umw1yZrrkfXRfxWNruwu7FELLx_cpOg/edit
// for additional details.

// static
bool FromGWSPageLoadMetricsLogger::IsGoogleSearchHostname(
    base::StringPiece host) {
  const char kGoogleSearchHostnamePrefix[] = "www.";

  // Hostname must start with 'www.' Hostnames are not case sensitive.
  if (!base::StartsWith(host, kGoogleSearchHostnamePrefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      host,
      // Do not include private registries, such as appspot.com. We don't want
      // to match URLs like www.google.appspot.com.
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  // Domain and registry must start with 'google.' e.g. 'google.com' or
  // 'google.co.uk'.
  if (!base::StartsWith(domain, "google.",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  // Finally, the length of the URL before the domain and registry must be equal
  // in length to the search hostname prefix.
  const size_t url_hostname_prefix_length = host.length() - domain.length();
  return url_hostname_prefix_length == strlen(kGoogleSearchHostnamePrefix);
}

// static
bool FromGWSPageLoadMetricsLogger::IsGoogleSearchResultUrl(const GURL& url) {
  // NOTE: we do not require 'q=' in the query, as AJAXy search may instead
  // store the query in the URL fragment.
  if (!IsGoogleSearchHostname(url.host_piece())) {
    return false;
  }

  if (!QueryContainsComponentPrefix(url.query_piece(), "q=") &&
      !QueryContainsComponentPrefix(url.ref_piece(), "q=")) {
    return false;
  }

  const base::StringPiece path = url.path_piece();
  return path == "/search" || path == "/webhp" || path == "/custom" ||
         path == "/";
}

// static
bool FromGWSPageLoadMetricsLogger::IsGoogleSearchRedirectorUrl(
    const GURL& url) {
  if (!IsGoogleSearchHostname(url.host_piece()))
    return false;

  // The primary search redirector.  Google search result redirects are
  // differentiated from other general google redirects by 'source=web' in the
  // query string.
  if (url.path_piece() == "/url" && url.has_query() &&
      QueryContainsComponent(url.query_piece(), "source=web")) {
    return true;
  }

  // Intent-based navigations from search are redirected through a second
  // redirector, which receives its redirect URL in the fragment/hash/ref
  // portion of the URL (the portion after '#'). We don't check for the presence
  // of certain params in the ref since this redirector is only used for
  // redirects from search.
  return url.path_piece() == "/searchurl/r.html" && url.has_ref();
}

// static
bool FromGWSPageLoadMetricsLogger::QueryContainsComponent(
    const base::StringPiece query,
    const base::StringPiece component) {
  return QueryContainsComponentHelper(query, component, false);
}

// static
bool FromGWSPageLoadMetricsLogger::QueryContainsComponentPrefix(
    const base::StringPiece query,
    const base::StringPiece component) {
  return QueryContainsComponentHelper(query, component, true);
}

// static
bool FromGWSPageLoadMetricsLogger::QueryContainsComponentHelper(
    const base::StringPiece query,
    const base::StringPiece component,
    bool component_is_prefix) {
  if (query.empty() || component.empty() ||
      component.length() > query.length()) {
    return false;
  }

  // Verify that the provided query string does not include the query or
  // fragment start character, as the logic below depends on this character not
  // being included.
  DCHECK(query[0] != '?' && query[0] != '#');

  // We shouldn't try to find matches beyond the point where there aren't enough
  // characters left in query to fully match the component.
  const size_t last_search_start = query.length() - component.length();

  // We need to search for matches in a loop, rather than stopping at the first
  // match, because we may initially match a substring that isn't a full query
  // string component. Consider, for instance, the query string 'ab=cd&b=c'. If
  // we search for component 'b=c', the first substring match will be characters
  // 1-3 (zero-based) in the query string. However, this isn't a full component
  // (the full component is ab=cd) so the match will fail. Thus, we must
  // continue our search to find the second substring match, which in the
  // example is at characters 6-8 (the end of the query string) and is a
  // successful component match.
  for (size_t start_offset = 0; start_offset <= last_search_start;
       start_offset += component.length()) {
    start_offset = query.find(component, start_offset);
    if (start_offset == std::string::npos) {
      // We searched to end of string and did not find a match.
      return false;
    }
    // Verify that the character prior to the component is valid (either we're
    // at the beginning of the query string, or are preceded by an ampersand).
    if (start_offset != 0 && query[start_offset - 1] != '&') {
      continue;
    }
    if (!component_is_prefix) {
      // Verify that the character after the component substring is valid
      // (either we're at the end of the query string, or are followed by an
      // ampersand).
      const size_t after_offset = start_offset + component.length();
      if (after_offset < query.length() && query[after_offset] != '&') {
        continue;
      }
    }
    return true;
  }
  return false;
}

FromGWSPageLoadMetricsLogger::FromGWSPageLoadMetricsLogger() {}

void FromGWSPageLoadMetricsLogger::SetPreviouslyCommittedUrl(const GURL& url) {
  previously_committed_url_is_search_results_ = IsGoogleSearchResultUrl(url);
  previously_committed_url_is_search_redirector_ =
      IsGoogleSearchRedirectorUrl(url);
}

void FromGWSPageLoadMetricsLogger::SetProvisionalUrl(const GURL& url) {
  provisional_url_has_search_hostname_ =
      IsGoogleSearchHostname(url.host_piece());
}

FromGWSPageLoadMetricsObserver::FromGWSPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  logger_.SetPreviouslyCommittedUrl(currently_committed_url);
  logger_.SetProvisionalUrl(navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // We'd like to also check navigation_handle->HasUserGesture() here, however
  // this signal is not carried forward for navigations that open links in new
  // tabs, so we look only at PAGE_TRANSITION_LINK. Back/forward navigations
  // that were originally navigated from a link will continue to report a core
  // type of link, so to filter out back/forward navs, we also check that the
  // page transition is a new navigation.
  logger_.set_navigation_initiated_via_link(
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_LINK) &&
      ui::PageTransitionIsNewNavigation(
          navigation_handle->GetPageTransition()));

  logger_.SetNavigationStart(navigation_handle->NavigationStart());
  return CONTINUE_OBSERVING;
}

void FromGWSPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnDomContentLoadedEventStart(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnLoadEventStart(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnFirstPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnFirstPaint(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnFirstTextPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnFirstTextPaint(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnFirstImagePaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnFirstImagePaint(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnFirstContentfulPaint(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnParseStart(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnParseStop(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnComplete(timing, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnFailedProvisionalLoad(failed_load_info, extra_info);
}

void FromGWSPageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event) {
  logger_.OnUserInput(event);
}

void FromGWSPageLoadMetricsLogger::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!ShouldLogPostCommitMetrics(extra_info.url))
    return;

  page_load_metrics::PageAbortInfo abort_info = GetPageAbortInfo(extra_info);
  if (!WasAbortedInForeground(extra_info, abort_info))
    return;

  // If we did not receive any timing IPCs from the render process, we can't
  // know for certain if the page was truly aborted before paint, or if the
  // abort happened before we received the IPC from the render process. Thus, we
  // do not log aborts for these page loads. Tracked page loads that receive no
  // timing IPCs are tracked via the ERR_NO_IPCS_RECEIVED error code in the
  // PageLoad.Events.InternalError histogram, so we can keep track of how often
  // this happens.
  if (timing.IsEmpty())
    return;

  if (!timing.first_paint || timing.first_paint >= abort_info.time_to_abort) {
    LogCommittedAbortsBeforePaint(abort_info.reason, abort_info.time_to_abort);
  } else if (WasAbortedBeforeInteraction(abort_info,
                                         first_user_interaction_after_paint_)) {
    LogAbortsAfterPaintBeforeInteraction(abort_info);
  }
}

void FromGWSPageLoadMetricsLogger::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!ShouldLogFailedProvisionalLoadMetrics())
    return;

  page_load_metrics::PageAbortInfo abort_info = GetPageAbortInfo(extra_info);
  if (!WasAbortedInForeground(extra_info, abort_info))
    return;

  LogProvisionalAborts(abort_info);
}

bool FromGWSPageLoadMetricsLogger::ShouldLogFailedProvisionalLoadMetrics() {
  // See comment in ShouldLogPostCommitMetrics above the call to
  // IsGoogleSearchHostname for more info on this if test.
  if (provisional_url_has_search_hostname_)
    return false;

  return previously_committed_url_is_search_results_ ||
         previously_committed_url_is_search_redirector_;
}

bool FromGWSPageLoadMetricsLogger::ShouldLogPostCommitMetrics(const GURL& url) {
  DCHECK(!url.is_empty());

  // If this page has a URL on a known google search hostname, then it may be a
  // page associated with search (either a search results page, or a search
  // redirector url), so we should not log stats. We could try to detect only
  // the specific known search URLs here, and log navigations to other pages on
  // the google search hostname (for example, a search for 'about google'
  // includes a result for https://www.google.com/about/), however, we assume
  // these cases are relatively uncommon, and we run the risk of logging metrics
  // for some search redirector URLs. Thus we choose the more conservative
  // approach of ignoring all urls on known search hostnames.
  if (IsGoogleSearchHostname(url.host_piece()))
    return false;

  // We're only interested in tracking navigations (e.g. clicks) initiated via
  // links. Note that the redirector will mask these, so don't enforce this if
  // the navigation came from a redirect url. TODO(csharrison): Use this signal
  // for provisional loads when the content APIs allow for it.
  if (previously_committed_url_is_search_results_ &&
      navigation_initiated_via_link_) {
    return true;
  }

  // If the navigation was via the search redirector, then the information about
  // whether the navigation was from a link would have been associated with the
  // navigation to the redirector, and not included in the redirected
  // navigation. Therefore, do not require link navigation this case.
  return previously_committed_url_is_search_redirector_;
}

bool FromGWSPageLoadMetricsLogger::ShouldLogForegroundEventAfterCommit(
    const base::Optional<base::TimeDelta>& event,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK(info.did_commit)
      << "ShouldLogForegroundEventAfterCommit called without committed URL.";
  return ShouldLogPostCommitMetrics(info.url) &&
         WasStartedInForegroundOptionalEventInForeground(event, info);
}

void FromGWSPageLoadMetricsLogger::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.dom_content_loaded_event_start,
                                          extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSDomContentLoaded,
                        timing.dom_content_loaded_event_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.load_event_start,
                                          extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSLoad,
                        timing.load_event_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnFirstPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.first_paint, extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstPaint,
                        timing.first_paint.value());
  }
  first_paint_triggered_ = true;
}

void FromGWSPageLoadMetricsLogger::OnFirstTextPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.first_text_paint,
                                          extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstTextPaint,
                        timing.first_text_paint.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnFirstImagePaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.first_image_paint,
                                          extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstImagePaint,
                        timing.first_image_paint.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.first_contentful_paint,
                                          extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstContentfulPaint,
                        timing.first_contentful_paint.value());

    // If we have a foreground paint, we should have a foreground parse start,
    // since paints can't happen until after parsing starts.
    DCHECK(WasStartedInForegroundOptionalEventInForeground(timing.parse_start,
                                                           extra_info));
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFromGWSParseStartToFirstContentfulPaint,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.parse_start, extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSParseStart,
                        timing.parse_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnParseStop(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogForegroundEventAfterCommit(timing.parse_stop, extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSParseDuration,
                        timing.parse_stop.value() - timing.parse_start.value());
  }
}

void FromGWSPageLoadMetricsLogger::OnUserInput(
    const blink::WebInputEvent& event) {
  if (first_paint_triggered_ && !first_user_interaction_after_paint_) {
    DCHECK(!navigation_start_.is_null());
    first_user_interaction_after_paint_ =
        base::TimeTicks::Now() - navigation_start_;
  }
}
