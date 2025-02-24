// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_web_client.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/ios/ios_util.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/task_scheduler_util/browser/initialization.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_about_rewriter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ios_chrome_main_parts.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"
#import "ios/chrome/browser/ui/chrome_web_view_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/audio_session_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#include "ios/web/public/browser_url_rewriter.h"
#include "ios/web/public/user_agent.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {
// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}
}

ChromeWebClient::ChromeWebClient() {}

ChromeWebClient::~ChromeWebClient() {}

web::WebMainParts* ChromeWebClient::CreateWebMainParts() {
  return new IOSChromeMainParts(*base::CommandLine::ForCurrentProcess());
}

void ChromeWebClient::PreWebViewCreation() const {
  // Initialize the audio session to allow a web page's audio to continue
  // playing after the app is backgrounded.
  VoiceSearchProvider* voice_provider =
      ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
  if (voice_provider) {
    AudioSessionController* audio_controller =
        voice_provider->GetAudioSessionController();
    if (audio_controller) {
      audio_controller->InitializeSessionIfNecessary();
    }
  }
}

void ChromeWebClient::AddAdditionalSchemes(
    std::vector<url::SchemeWithType>* additional_standard_schemes) const {
  url::SchemeWithType scheme = {kChromeUIScheme, url::SCHEME_WITHOUT_PORT};
  additional_standard_schemes->push_back(scheme);
}

std::string ChromeWebClient::GetAcceptLangs(web::BrowserState* state) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(state);
  return chrome_browser_state->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

std::string ChromeWebClient::GetApplicationLocale() const {
  DCHECK(GetApplicationContext());
  return GetApplicationContext()->GetApplicationLocale();
}

bool ChromeWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kChromeUIScheme);
}

base::string16 ChromeWebClient::GetPluginNotSupportedText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
}

std::string ChromeWebClient::GetProduct() const {
  std::string product("CriOS/");
  product += version_info::GetVersionNumber();
  return product;
}

std::string ChromeWebClient::GetUserAgent(web::UserAgentType type) const {
  // The user agent should not be requested for app-specific URLs.
  DCHECK_NE(type, web::UserAgentType::NONE);

  // Using desktop user agent overrides a command-line user agent, so that
  // request desktop site can still work when using an overridden UA.
  if (type == web::UserAgentType::DESKTOP)
    return base::SysNSStringToUTF8(ChromeWebView::kDesktopUserAgent);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent))
      return user_agent;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  return web::BuildUserAgentFromProduct(GetProduct());
}

base::string16 ChromeWebClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

void ChromeWebClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

void ChromeWebClient::PostBrowserURLRewriterCreation(
    web::BrowserURLRewriter* rewriter) {
  rewriter->AddURLRewriter(&WillHandleWebBrowserAboutURL);
}

NSString* ChromeWebClient::GetEarlyPageScript() const {
  return GetPageScript(@"print");
}

void ChromeWebClient::AllowCertificateError(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  IOSSSLErrorHandler::HandleSSLError(web_state, cert_error, info, request_url,
                                     overridable, callback);
}

void ChromeWebClient::GetTaskSchedulerInitializationParams(
    std::vector<base::SchedulerWorkerPoolParams>* params_vector,
    base::TaskScheduler::WorkerPoolIndexForTraitsCallback*
        index_to_traits_callback) {
  DCHECK(params_vector);
  DCHECK(index_to_traits_callback);
  // If this call fails, web will fall back to the default params.
  *params_vector =
      task_scheduler_util::GetBrowserWorkerPoolParamsFromVariations();
  *index_to_traits_callback =
      base::Bind(&task_scheduler_util::BrowserWorkerPoolIndexForTraits);
}

void ChromeWebClient::PerformExperimentalTaskSchedulerRedirections() {
  task_scheduler_util::MaybePerformBrowserTaskSchedulerRedirection();
}
