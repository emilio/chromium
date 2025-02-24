// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/policy/android_management_client.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/arc_stop_reason.h"

class ArcAppLauncher;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace arc {

class ArcAndroidManagementChecker;
class ArcAuthContext;
class ArcTermsOfServiceNegotiator;
enum class ProvisioningResult : int;

// This class proxies the request from the client to fetch an auth code from
// LSO. It lives on the UI thread.
class ArcSessionManager : public ArcSessionRunner::Observer,
                          public ArcSupportHost::Observer {
 public:
  // Represents each State of ARC session.
  // NOT_INITIALIZED: represents the state that the Profile is not yet ready
  //   so that this service is not yet initialized, or Chrome is being shut
  //   down so that this is destroyed.
  // STOPPED: ARC session is not running, or being terminated.
  // NEGOTIATING_TERMS_OF_SERVICE: Negotiating Google Play Store "Terms of
  //   Service" with a user. There are several ways for the negotiation,
  //   including opt-in flow, which shows "Terms of Service" page on ARC
  //   support app, and OOBE flow, which shows "Terms of Service" page as a
  //   part of Chrome OOBE flow.
  //   If user does not accept the Terms of Service, disables Google Play
  //   Store, which triggers RequestDisable() and the state will be set to
  //   STOPPED, then.
  // CHECKING_ANDROID_MANAGEMENT: Checking Android management status. Note that
  //   the status is checked for each ARC session starting, but this is the
  //   state only for the first boot case (= opt-in case). The second time and
  //   later the management check is running in parallel with ARC session
  //   starting, and in such a case, State is ACTIVE, instead.
  // REMOVING_DATA_DIR: When ARC is disabled, the data directory is removed.
  //   While removing is processed, ARC cannot be started. This is the state.
  // ACTIVE: ARC is running.
  //
  // State transition should be as follows:
  //
  // NOT_INITIALIZED -> STOPPED: when the primary Profile gets ready.
  // ...(any)... -> NOT_INITIALIZED: when the Chrome is being shutdown.
  // ...(any)... -> STOPPED: on error.
  //
  // In the first boot case:
  //   STOPPED -> NEGOTIATING_TERMS_OF_SERVICE: On request to enable.
  //   NEGOTIATING_TERMS_OF_SERVICE -> CHECKING_ANDROID_MANAGEMENT: when a user
  //     accepts "Terms Of Service"
  //   CHECKING_ANDROID_MANAGEMENT -> ACTIVE: when the auth token is
  //     successfully fetched.
  //
  // In the second (or later) boot case:
  //   STOPPED -> ACTIVE: when arc.enabled preference is checked that it is
  //     true. Practically, this is when the primary Profile gets ready.
  //
  // TODO(hidehiko): Fix the state machine, and update the comment including
  // relationship with |enable_requested_|.
  enum class State {
    NOT_INITIALIZED,
    STOPPED,
    NEGOTIATING_TERMS_OF_SERVICE,
    CHECKING_ANDROID_MANAGEMENT,
    REMOVING_DATA_DIR,
    ACTIVE,
  };

  // Observer for those services outside of ARC which want to know ARC events.
  class Observer {
   public:
    // Called to notify that whether Google Play Store is enabled or not, which
    // is represented by "arc.enabled" preference, is updated.
    virtual void OnArcPlayStoreEnabledChanged(bool enabled) {}

    // Called to notify that ARC has been initialized successfully.
    virtual void OnArcInitialStart() {}

    // Called when ARC session is stopped, and is not being restarted
    // automatically.
    virtual void OnArcSessionStopped(ArcStopReason stop_reason) {}

    // Called to notify that Android data has been removed. Used in
    // browser_tests
    virtual void OnArcDataRemoved() {}

   protected:
    virtual ~Observer() = default;
  };

  explicit ArcSessionManager(
      std::unique_ptr<ArcSessionRunner> arc_session_runner);
  ~ArcSessionManager() override;

  static ArcSessionManager* Get();

  // Exposed here for unit_tests validation.
  static bool IsOobeOptInActive();

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void DisableUIForTesting();
  static void EnableCheckAndroidManagementForTesting();

  // Returns true if ARC is allowed to run for the current session.
  // TODO(hidehiko): The name is very close to IsArcAllowedForProfile(), but
  // has different meaning. Clean this up.
  bool IsAllowed() const;

  void Shutdown();

  // Sets the |profile|, and sets up Profile related fields in this instance.
  // IsArcAllowedForProfile() must return true for the given |profile|.
  void SetProfile(Profile* profile);

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }

  State state() const { return state_; }

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies observers that Google Play Store enabled preference is changed.
  // Note: ArcPlayStoreEnabledPreferenceHandler has the main responsibility to
  // notify the event. However, due to life time, it is difficult for non-ARC
  // services to subscribe the handler instance directly. Instead, they can
  // subscribe to ArcSessionManager, and ArcSessionManager proxies the event.
  void NotifyArcPlayStoreEnabledChanged(bool enabled);

  // Returns true if ARC instance is running/stopped, respectively.
  // See ArcSessionRunner::IsRunning()/IsStopped() for details.
  bool IsSessionRunning() const;
  bool IsSessionStopped() const;

  // Called from ARC support platform app when user cancels signing.
  void CancelAuthCode();

  // Requests to enable ARC session. This starts ARC instance, or maybe starts
  // Terms Of Service negotiation if they haven't been accepted yet.
  // If it is already requested to enable, no-op.
  // Currently, enabled/disabled is tied to whether Google Play Store is
  // enabled or disabled. Please see also TODO of
  // SetArcPlayStoreEnabledForProfile().
  void RequestEnable();

  // Requests to disable ARC session. This stops ARC instance, or quits Terms
  // Of Service negotiation if it is the middle of the process (e.g. closing
  // UI for manual negotiation if it is shown).
  // If it is already requested to disable, no-op.
  void RequestDisable();

  // Requests to remove the ARC data.
  // If ARC is stopped, triggers to remove the data. Otherwise, queues to
  // remove the data after ARC stops.
  // A log statement with the removal reason must be added prior to calling
  // this.
  void RequestArcDataRemoval();

  // Called from the Chrome OS metrics provider to record Arc.State
  // periodically.
  void RecordArcState();

  // ArcSupportHost::Observer:
  void OnWindowClosed() override;
  void OnTermsAgreed(bool is_metrics_enabled,
                     bool is_backup_and_restore_enabled,
                     bool is_location_service_enabled) override;
  void OnRetryClicked() override;
  void OnSendFeedbackClicked() override;

  // StopArc(), then restart. Between them data clear may happens.
  // This is a special method to support enterprise device lost case.
  // This can be called only when ARC is running.
  void StopAndEnableArc();

  ArcSupportHost* support_host() { return support_host_.get(); }

  // TODO(hidehiko): Get rid of the getter by migration between ArcAuthContext
  // and ArcAuthInfoFetcher.
  ArcAuthContext* auth_context() { return context_.get(); }

  // On provisioning completion (regardless of whether successfully done or
  // not), this is called with its status. On success, called with
  // ProvisioningResult::SUCCESS, otherwise |result| is the error reason.
  void OnProvisioningFinished(ProvisioningResult result);

  // Returns the time when the sign in process started, or a null time if
  // signing in didn't happen during this session.
  base::Time sign_in_start_time() const { return sign_in_start_time_; }

  // Returns the time when ARC was about to start, or a null time if ARC has not
  // been started yet.
  base::Time arc_start_time() const { return arc_start_time_; }

  // Injectors for testing.
  void SetArcSessionRunnerForTesting(
      std::unique_ptr<ArcSessionRunner> arc_session_runner);
  void SetAttemptUserExitCallbackForTesting(const base::Closure& callback);

  // Returns whether the Play Store app is requested to be launched by this
  // class. Should be used only for tests.
  bool IsPlaystoreLaunchRequestedForTesting() const;

  // Invoking StartArc() only for testing, e.g., to emulate accepting Terms of
  // Service then passing Android management check successfully.
  void StartArcForTesting() { StartArc(); }

 private:
  // RequestEnable() has a check in order not to trigger starting procedure
  // twice. This method can be called to bypass that check when restarting.
  void RequestEnableImpl();

  // Negotiates the terms of service to user, if necessary.
  // Otherwise, move to StartAndroidManagementCheck().
  void MaybeStartTermsOfServiceNegotiation();
  void OnTermsOfServiceNegotiated(bool accepted);

  // Returns true if Terms of Service negotiation is needed. Otherwise false.
  // TODO(crbug.com/698418): Write unittest for this utility after extracting
  //   ToS related code from ArcSessionManager into a dedicated class.
  bool IsArcTermsOfServiceNegotiationNeeded() const;

  void SetState(State state);
  void ShutdownSession();
  void OnArcSignInTimeout();

  // Starts Android management check. This is for first boot case (= Opt-in
  // or OOBE flow case). In secondary or later ARC enabling, the check should
  // run in background.
  void StartAndroidManagementCheck();

  // Called when the Android management check is done in opt-in flow or
  // OOBE flow.
  void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);

  // Starts Android management check in background (in parallel with starting
  // ARC). This is for secondary or later ARC enabling.
  // The reason running them in parallel is for performance. The secondary or
  // later ARC enabling is typically on "logging into Chrome" for the user who
  // already opted in to use Google Play Store. In such a case, network is
  // typically not yet ready. Thus, if we block ARC boot, it delays several
  // seconds, which is not very user friendly.
  void StartBackgroundAndroidManagementCheck();

  // Called when the background Android management check is done. It is
  // triggered when the second or later ARC boot timing.
  void OnBackgroundAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);

  // Requests to starts ARC instance. Also, update the internal state to
  // ACTIVE.
  void StartArc();

  // Requests to stop ARC instnace. This resets two persistent flags:
  // kArcSignedIn and kArcTermsAccepted, so that, in next enabling,
  // it is started from Terms of Service negotiation.
  // TODO(hidehiko): Introduce STOPPING state, and this function should
  // transition to it.
  void StopArc();

  // ArcSessionRunner::Observer:
  void OnSessionStopped(ArcStopReason reason, bool restarting) override;

  // Starts to remove ARC data, if it is requested via RequestArcDataRemoval().
  // On completion, OnArcDataRemoved() is called.
  // If not requested, just skipping the data removal, and moves to
  // MaybeReenableArc() directly.
  void MaybeStartArcDataRemoval();
  void OnArcDataRemoved(bool success);

  // On ARC session stopped and/or data removal completion, this is called
  // so that, if necessary, ARC session is restarted.
  // TODO(hidehiko): This can be removed after the racy state machine
  // is fixed.
  void MaybeReenableArc();

  std::unique_ptr<ArcSessionRunner> arc_session_runner_;

  // Unowned pointer. Keeps current profile.
  Profile* profile_ = nullptr;

  // Whether ArcSessionManager is requested to enable (starting to run ARC
  // instance) or not.
  bool enable_requested_ = false;

  // Internal state machine. See also State enum class.
  State state_ = State::NOT_INITIALIZED;
  base::ObserverList<Observer> observer_list_;
  std::unique_ptr<ArcAppLauncher> playstore_launcher_;
  bool reenable_arc_ = false;
  bool provisioning_reported_ = false;
  base::OneShotTimer arc_sign_in_timer_;

  std::unique_ptr<ArcSupportHost> support_host_;

  std::unique_ptr<ArcTermsOfServiceNegotiator> terms_of_service_negotiator_;

  std::unique_ptr<ArcAuthContext> context_;
  std::unique_ptr<ArcAndroidManagementChecker> android_management_checker_;

  // The time when the sign in process started.
  base::Time sign_in_start_time_;
  // The time when ARC was about to start.
  base::Time arc_start_time_;
  base::Closure attempt_user_exit_callback_;

  // Must be the last member.
  base::WeakPtrFactory<ArcSessionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManager);
};

// Outputs the stringified |state| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os,
                         const ArcSessionManager::State& state);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_
