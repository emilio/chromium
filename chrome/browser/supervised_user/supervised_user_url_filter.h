// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "chrome/browser/safe_search_api/safe_search_url_checker.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/supervised_user_error_page/supervised_user_error_page.h"
#include "third_party/re2/src/re2/re2.h"

class GURL;
class SupervisedUserBlacklist;

namespace base {
class TaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

// This class manages the filtering behavior for a given URL, i.e. it tells
// callers if a given URL should be allowed, blocked or warned about. It uses
// information from multiple sources:
//   * A default setting (allow, block or warn).
//   * The set of installed and enabled whitelists which contain URL patterns
//     and hostname hashes that should be allowed.
//   * User-specified manual overrides (allow or block) for either sites
//     (hostnames) or exact URLs, which take precedence over the previous
//     sources.
// References to it can be passed around on different threads (the refcounting
// is thread-safe), but the object itself should always be accessed on the same
// thread (member access isn't thread-safe).
class SupervisedUserURLFilter
    : public base::RefCountedThreadSafe<SupervisedUserURLFilter>,
      public base::NonThreadSafe {
 public:
  enum FilteringBehavior {
    ALLOW,
    WARN,
    BLOCK,
    INVALID
  };

  using FilteringBehaviorCallback =
      base::Callback<void(FilteringBehavior,
                          supervised_user_error_page::FilteringBehaviorReason,
                          bool /* uncertain */)>;

  class Observer {
   public:
    virtual void OnSiteListUpdated() = 0;
    virtual void OnURLChecked(
        const GURL& url,
        FilteringBehavior behavior,
        supervised_user_error_page::FilteringBehaviorReason reason,
        bool uncertain) {}
  };

  struct Contents;

  SupervisedUserURLFilter();

  static FilteringBehavior BehaviorFromInt(int behavior_value);

  static bool ReasonIsAutomatic(
      supervised_user_error_page::FilteringBehaviorReason reason);

  // Normalizes a URL for matching purposes.
  static GURL Normalize(const GURL& url);

  // For known "cache" URLs (e.g. from the AMP project CDN), this returns the
  // embedded URL. For all other URLs, returns an empty GURL.
  // TODO(treib): Merge this with Normalize()? See also crbug.com/663678.
  GURL GetEmbeddedURL(const GURL& url) const;

  // Returns true if the URL has a standard scheme. Only URLs with standard
  // schemes are filtered.
  // This method is public for testing.
  static bool HasFilteredScheme(const GURL& url);

  // Returns true if the |host| matches the pattern. A pattern is a hostname
  // with one or both of the following modifications:
  // - If the pattern starts with "*.", it matches the host or any subdomain
  //   (e.g. the pattern "*.google.com" would match google.com, www.google.com,
  //   or accounts.google.com).
  // - If the pattern ends with ".*", it matches the host on any known TLD
  //   (e.g. the pattern "google.*" would match google.com or google.co.uk).
  // See the SupervisedUserURLFilterTest.HostMatchesPattern unit test for more
  // examples.
  // Asterisks in other parts of the pattern are not allowed.
  // |host| and |pattern| are assumed to be normalized to lower-case.
  // This method is public for testing.
  static bool HostMatchesPattern(const std::string& canonical_host,
                                 const std::string& pattern);

  // Returns the filtering behavior for a given URL, based on the default
  // behavior and whether it is on a site list.
  FilteringBehavior GetFilteringBehaviorForURL(const GURL& url) const;

  // Checks for a manual setting (i.e. manual exceptions and content packs)
  // for the given URL. If there is one, returns true and writes the result
  // into |behavior|. Otherwise returns false; in this case the value of
  // |behavior| is unspecified.
  bool GetManualFilteringBehaviorForURL(const GURL& url,
                                        FilteringBehavior* behavior) const;

  // Like |GetFilteringBehaviorForURL|, but also includes asynchronous checks
  // against a remote service. If the result is already determined by the
  // synchronous checks, then |callback| will be called synchronously.
  // Returns true if |callback| was called synchronously.
  bool GetFilteringBehaviorForURLWithAsyncChecks(
      const GURL& url,
      const FilteringBehaviorCallback& callback) const;

  // Gets all the whitelists that the url is part of. Returns id->name of each
  // whitelist.
  std::map<std::string, base::string16> GetMatchingWhitelistTitles(
      const GURL& url) const;

  // Sets the filtering behavior for pages not on a list (default is ALLOW).
  void SetDefaultFilteringBehavior(FilteringBehavior behavior);

  FilteringBehavior GetDefaultFilteringBehavior() const;

  // Asynchronously loads the specified site lists and updates the
  // filter to recognize each site on them.
  void LoadWhitelists(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists);

  // Sets the static blacklist of blocked hosts.
  void SetBlacklist(const SupervisedUserBlacklist* blacklist);
  // Returns whether the static blacklist is set up.
  bool HasBlacklist() const;

  // Set the list of matched patterns to the passed in list, for testing.
  void SetFromPatternsForTesting(const std::vector<std::string>& patterns);

  // Sets the site lists to the passed list, for testing.
  void SetFromSiteListsForTesting(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists);

  // Sets the set of manually allowed or blocked hosts.
  void SetManualHosts(const std::map<std::string, bool>* host_map);

  // Sets the set of manually allowed or blocked URLs.
  void SetManualURLs(const std::map<GURL, bool>* url_map);

  // Initializes the experimental asynchronous checker.
  void InitAsyncURLChecker(net::URLRequestContextGetter* context);

  // Clears any asynchronous checker.
  void ClearAsyncURLChecker();

  // Returns whether the asynchronous checker is set up.
  bool HasAsyncURLChecker() const;

  // Removes all filter entries, clears the blacklist and async checker if
  // present, and resets the default behavior to "allow".
  void Clear();

  void AddObserver(Observer* observer) const;
  void RemoveObserver(Observer* observer) const;

  // Sets a different task runner for testing.
  void SetBlockingTaskRunnerForTesting(
      const scoped_refptr<base::TaskRunner>& task_runner);

 private:
  friend class base::RefCountedThreadSafe<SupervisedUserURLFilter>;
  friend class SupervisedUserURLFilterTest;
  ~SupervisedUserURLFilter();

  void SetContents(std::unique_ptr<Contents> url_matcher);

  FilteringBehavior GetFilteringBehaviorForURL(
      const GURL& url,
      bool manual_only,
      supervised_user_error_page::FilteringBehaviorReason* reason) const;

  void CheckCallback(const FilteringBehaviorCallback& callback,
                     const GURL& url,
                     SafeSearchURLChecker::Classification classification,
                     bool uncertain) const;

  // This is mutable to allow notification in const member functions.
  mutable base::ObserverList<Observer> observers_;

  FilteringBehavior default_behavior_;
  std::unique_ptr<Contents> contents_;

  // Maps from a URL to whether it is manually allowed (true) or blocked
  // (false).
  std::map<GURL, bool> url_map_;

  // Maps from a hostname to whether it is manually allowed (true) or blocked
  // (false).
  std::map<std::string, bool> host_map_;

  // Not owned.
  const SupervisedUserBlacklist* blacklist_;

  std::unique_ptr<SafeSearchURLChecker> async_url_checker_;

  re2::RE2 amp_cache_path_regex_;
  re2::RE2 google_amp_viewer_path_regex_;
  re2::RE2 google_web_cache_query_regex_;

  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserURLFilter);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_
