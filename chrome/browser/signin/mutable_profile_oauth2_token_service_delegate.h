// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"

class MutableProfileOAuth2TokenServiceDelegate
    : public OAuth2TokenServiceDelegate,
      public WebDataServiceConsumer,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  MutableProfileOAuth2TokenServiceDelegate(
      SigninClient* client,
      SigninErrorController* signin_error_controller,
      AccountTrackerService* account_tracker_service);
  ~MutableProfileOAuth2TokenServiceDelegate() override;

  // OAuth2TokenServiceDelegate overrides.
  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) override;

  // Updates the internal cache of the result from the most-recently-completed
  // auth request (used for reporting errors to the user).
  void UpdateAuthError(const std::string& account_id,
                       const GoogleServiceAuthError& error) override;

  bool RefreshTokenIsAvailable(const std::string& account_id) const override;
  bool RefreshTokenHasError(const std::string& account_id) const override;
  std::vector<std::string> GetAccounts() override;
  net::URLRequestContextGetter* GetRequestContext() const override;

  void LoadCredentials(const std::string& primary_account_id) override;
  void UpdateCredentials(const std::string& account_id,
                         const std::string& refresh_token) override;
  void RevokeAllCredentials() override;

  // Revokes credentials related to |account_id|.
  void RevokeCredentials(const std::string& account_id) override;

  // Overridden from OAuth2TokenServiceDelegate.
  void Shutdown() override;
  LoadCredentialsState GetLoadCredentialsState() const override;

  // Overridden from NetworkChangeObserver.
  void OnNetworkChanged(net::NetworkChangeNotifier::ConnectionType type)
      override;

  // Overridden from OAuth2TokenServiceDelegate.
  const net::BackoffEntry* BackoffEntry() const override;

  // Returns the account's refresh token used for testing purposes.
  std::string GetRefreshTokenForTest(const std::string& account_id) const;

 private:
  friend class MutableProfileOAuth2TokenServiceDelegateTest;

  class RevokeServerRefreshToken;

  class AccountStatus : public SigninErrorController::AuthStatusProvider {
   public:
    AccountStatus(SigninErrorController* signin_error_controller,
                  const std::string& account_id,
                  const std::string& refresh_token);
    ~AccountStatus() override;

    const std::string& refresh_token() const { return refresh_token_; }
    void set_refresh_token(const std::string& token) { refresh_token_ = token; }

    void SetLastAuthError(const GoogleServiceAuthError& error);

    // SigninErrorController::AuthStatusProvider implementation.
    std::string GetAccountId() const override;
    GoogleServiceAuthError GetAuthStatus() const override;

   private:
    SigninErrorController* signin_error_controller_;
    std::string account_id_;
    std::string refresh_token_;
    GoogleServiceAuthError last_auth_error_;

    DISALLOW_COPY_AND_ASSIGN(AccountStatus);
  };

  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           PersistenceDBUpgrade);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           FetchPersistentError);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           PersistenceLoadCredentials);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           GetAccounts);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           RetryBackoff);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           CanonicalizeAccountId);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           CanonAndNonCanonAccountId);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           ShutdownService);

  // WebDataServiceConsumer implementation:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // Loads credentials into in memory stucture.
  void LoadAllCredentialsIntoMemory(
      const std::map<std::string, std::string>& db_tokens);

  // Persists credentials for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void PersistCredentials(const std::string& account_id,
                          const std::string& refresh_token);

  // Clears credentials persisted for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void ClearPersistedCredentials(const std::string& account_id);

  // Revokes the refresh token on the server.
  void RevokeCredentialsOnServer(const std::string& refresh_token);

  // Cancels any outstanding fetch for tokens from the web database.
  void CancelWebTokenFetch();

  std::string GetRefreshToken(const std::string& account_id) const;

  // Maps the |account_id| of accounts known to ProfileOAuth2TokenService
  // to information about the account.
  typedef std::map<std::string, linked_ptr<AccountStatus>> AccountStatusMap;
  // In memory refresh token store mapping account_id to refresh_token.
  AccountStatusMap refresh_tokens_;

  // Handle to the request reading tokens from database.
  WebDataServiceBase::Handle web_data_service_request_;

  // The primary account id of this service's profile during the loading of
  // credentials.  This member is empty otherwise.
  std::string loading_primary_account_id_;

  // The state of the load credentials operation.
  LoadCredentialsState load_credentials_state_;

  ScopedVector<RevokeServerRefreshToken> server_revokes_;

  // Used to verify that certain methods are called only on the thread on which
  // this instance was created.
  base::ThreadChecker thread_checker_;

  // Used to rate-limit network token requests so as to not overload the server.
  net::BackoffEntry::Policy backoff_policy_;
  net::BackoffEntry backoff_entry_;
  GoogleServiceAuthError backoff_error_;

  SigninClient* client_;
  SigninErrorController* signin_error_controller_;
  AccountTrackerService* account_tracker_service_;

  DISALLOW_COPY_AND_ASSIGN(MutableProfileOAuth2TokenServiceDelegate);
};

#endif  // CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
