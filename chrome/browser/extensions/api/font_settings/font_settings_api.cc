// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font Settings Extension API implementation.

#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/font_settings_utils.h"
#include "chrome/common/extensions/api/font_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"

namespace extensions {

namespace fonts = api::font_settings;
using options::FontSettingsUtilities;

namespace {

const char kFontIdKey[] = "fontId";
const char kGenericFamilyKey[] = "genericFamily";
const char kLevelOfControlKey[] = "levelOfControl";
const char kDisplayNameKey[] = "displayName";
const char kPixelSizeKey[] = "pixelSize";
const char kScriptKey[] = "script";

const char kSetFromIncognitoError[] =
    "Can't modify regular settings from an incognito context.";

// Format for font name preference paths.
const char kWebKitFontPrefFormat[] = "webkit.webprefs.fonts.%s.%s";

// Gets the font name preference path for |generic_family| and |script|. If
// |script| is NULL, uses prefs::kWebKitCommonScript.
std::string GetFontNamePrefPath(fonts::GenericFamily generic_family_enum,
                                fonts::ScriptCode script_enum) {
  std::string script = fonts::ToString(script_enum);
  if (script.empty())
    script = prefs::kWebKitCommonScript;
  std::string generic_family = fonts::ToString(generic_family_enum);
  return base::StringPrintf(kWebKitFontPrefFormat,
                            generic_family.c_str(),
                            script.c_str());
}

// Registers |obs| to observe per-script font prefs under the path |map_name|.
void RegisterFontFamilyMapObserver(
    PrefChangeRegistrar* registrar,
    const char* map_name,
    const PrefChangeRegistrar::NamedChangeCallback& callback) {
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    registrar->Add(base::StringPrintf("%s.%s", map_name, script), callback);
  }
}

}  // namespace

FontSettingsEventRouter::FontSettingsEventRouter(
    Profile* profile) : profile_(profile) {
  TRACE_EVENT0("browser,startup", "FontSettingsEventRouter::ctor")
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.FontSettingsEventRouterCtorTime");

  registrar_.Init(profile_->GetPrefs());

  AddPrefToObserve(prefs::kWebKitDefaultFixedFontSize,
                   events::FONT_SETTINGS_ON_DEFAULT_FIXED_FONT_SIZE_CHANGED,
                   fonts::OnDefaultFixedFontSizeChanged::kEventName,
                   kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitDefaultFontSize,
                   events::FONT_SETTINGS_ON_DEFAULT_FONT_SIZE_CHANGED,
                   fonts::OnDefaultFontSizeChanged::kEventName, kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitMinimumFontSize,
                   events::FONT_SETTINGS_ON_MINIMUM_FONT_SIZE_CHANGED,
                   fonts::OnMinimumFontSizeChanged::kEventName, kPixelSizeKey);

  PrefChangeRegistrar::NamedChangeCallback callback =
      base::Bind(&FontSettingsEventRouter::OnFontFamilyMapPrefChanged,
                 base::Unretained(this));
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitStandardFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitSerifFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitSansSerifFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitFixedFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitCursiveFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitFantasyFontFamilyMap, callback);
  RegisterFontFamilyMapObserver(&registrar_,
                                prefs::kWebKitPictographFontFamilyMap,
                                callback);
}

FontSettingsEventRouter::~FontSettingsEventRouter() {}

void FontSettingsEventRouter::AddPrefToObserve(
    const char* pref_name,
    events::HistogramValue histogram_value,
    const char* event_name,
    const char* key) {
  registrar_.Add(
      pref_name,
      base::Bind(&FontSettingsEventRouter::OnFontPrefChanged,
                 base::Unretained(this), histogram_value, event_name, key));
}

void FontSettingsEventRouter::OnFontFamilyMapPrefChanged(
    const std::string& pref_name) {
  std::string generic_family;
  std::string script;
  if (pref_names_util::ParseFontNamePrefPath(pref_name, &generic_family,
                                             &script)) {
    OnFontNamePrefChanged(pref_name, generic_family, script);
    return;
  }

  NOTREACHED();
}

void FontSettingsEventRouter::OnFontNamePrefChanged(
    const std::string& pref_name,
    const std::string& generic_family,
    const std::string& script) {
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      pref_name);
  CHECK(pref);

  std::string font_name;
  if (!pref->GetValue()->GetAsString(&font_name)) {
    NOTREACHED();
    return;
  }
  font_name = FontSettingsUtilities::MaybeGetLocalizedFontName(font_name);

  base::ListValue args;
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString(kFontIdKey, font_name);
  dict->SetString(kGenericFamilyKey, generic_family);
  dict->SetString(kScriptKey, script);
  args.Append(std::move(dict));

  extensions::preference_helpers::DispatchEventToExtensions(
      profile_, events::FONT_SETTINGS_ON_FONT_CHANGED,
      fonts::OnFontChanged::kEventName, &args, APIPermission::kFontSettings,
      false, pref_name);
}

void FontSettingsEventRouter::OnFontPrefChanged(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    const std::string& key,
    const std::string& pref_name) {
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      pref_name);
  CHECK(pref);

  base::ListValue args;
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->Set(key, pref->GetValue()->DeepCopy());
  args.Append(std::move(dict));

  extensions::preference_helpers::DispatchEventToExtensions(
      profile_, histogram_value, event_name, &args,
      APIPermission::kFontSettings, false, pref_name);
}

FontSettingsAPI::FontSettingsAPI(content::BrowserContext* context)
    : font_settings_event_router_(
          new FontSettingsEventRouter(Profile::FromBrowserContext(context))) {}

FontSettingsAPI::~FontSettingsAPI() {
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<FontSettingsAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<FontSettingsAPI>*
FontSettingsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

ExtensionFunction::ResponseAction FontSettingsClearFontFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  std::unique_ptr<fonts::ClearFont::Params> params(
      fonts::ClearFont::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  // Ensure |pref_path| really is for a registered per-script font pref.
  EXTENSION_FUNCTION_VALIDATE(profile->GetPrefs()->FindPreference(pref_path));

  PreferenceAPI::Get(profile)->RemoveExtensionControlledPref(
      extension_id(), pref_path, kExtensionPrefsScopeRegular);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FontSettingsGetFontFunction::Run() {
  std::unique_ptr<fonts::GetFont::Params> params(
      fonts::GetFont::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(pref_path);

  std::string font_name;
  EXTENSION_FUNCTION_VALIDATE(
      pref && pref->GetValue()->GetAsString(&font_name));
  font_name = FontSettingsUtilities::MaybeGetLocalizedFontName(font_name);

  // We don't support incognito-specific font prefs, so don't consider them when
  // getting level of control.
  const bool kIncognito = false;
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(profile, extension_id(),
                                                        pref_path, kIncognito);

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetString(kFontIdKey, font_name);
  result->SetString(kLevelOfControlKey, level_of_control);
  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction FontSettingsSetFontFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  std::unique_ptr<fonts::SetFont::Params> params(
      fonts::SetFont::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  // Ensure |pref_path| really is for a registered font pref.
  EXTENSION_FUNCTION_VALIDATE(profile->GetPrefs()->FindPreference(pref_path));

  PreferenceAPI::Get(profile)->SetExtensionControlledPref(
      extension_id(), pref_path, kExtensionPrefsScopeRegular,
      new base::Value(params->details.font_id));
  return RespondNow(NoArguments());
}

bool FontSettingsGetFontListFunction::RunAsync() {
  content::GetFontListAsync(
      Bind(&FontSettingsGetFontListFunction::FontListHasLoaded, this));
  return true;
}

void FontSettingsGetFontListFunction::FontListHasLoaded(
    std::unique_ptr<base::ListValue> list) {
  bool success = CopyFontsToResult(list.get());
  SendResponse(success);
}

bool FontSettingsGetFontListFunction::CopyFontsToResult(
    base::ListValue* fonts) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (base::ListValue::iterator it = fonts->begin();
       it != fonts->end(); ++it) {
    base::ListValue* font_list_value;
    if (!(*it)->GetAsList(&font_list_value)) {
      NOTREACHED();
      return false;
    }

    std::string name;
    if (!font_list_value->GetString(0, &name)) {
      NOTREACHED();
      return false;
    }

    std::string localized_name;
    if (!font_list_value->GetString(1, &localized_name)) {
      NOTREACHED();
      return false;
    }

    std::unique_ptr<base::DictionaryValue> font_name(
        new base::DictionaryValue());
    font_name->Set(kFontIdKey, new base::Value(name));
    font_name->Set(kDisplayNameKey, new base::Value(localized_name));
    result->Append(std::move(font_name));
  }

  SetResult(std::move(result));
  return true;
}

ExtensionFunction::ResponseAction ClearFontPrefExtensionFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  PreferenceAPI::Get(profile)->RemoveExtensionControlledPref(
      extension_id(), GetPrefName(), kExtensionPrefsScopeRegular);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction GetFontPrefExtensionFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref = prefs->FindPreference(GetPrefName());
  EXTENSION_FUNCTION_VALIDATE(pref);

  // We don't support incognito-specific font prefs, so don't consider them when
  // getting level of control.
  const bool kIncognito = false;

  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(
          profile, extension_id(), GetPrefName(), kIncognito);

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->Set(GetKey(), pref->GetValue()->DeepCopy());
  result->SetString(kLevelOfControlKey, level_of_control);
  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction SetFontPrefExtensionFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  base::DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));

  base::Value* value;
  EXTENSION_FUNCTION_VALIDATE(details->Get(GetKey(), &value));

  PreferenceAPI::Get(profile)->SetExtensionControlledPref(
      extension_id(), GetPrefName(), kExtensionPrefsScopeRegular,
      value->DeepCopy());
  return RespondNow(NoArguments());
}

const char* FontSettingsClearDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsGetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsGetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsSetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsClearDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsGetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsGetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsSetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsClearMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsGetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsGetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsSetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

}  // namespace extensions
