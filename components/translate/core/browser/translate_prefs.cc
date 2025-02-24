// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <set>

#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_experiment.h"
#include "components/translate/core/common/translate_util.h"

namespace translate {

const char TranslatePrefs::kPrefLanguageProfile[] = "language_profile";
const char TranslatePrefs::kPrefTranslateSiteBlacklist[] =
    "translate_site_blacklist";
const char TranslatePrefs::kPrefTranslateWhitelists[] = "translate_whitelists";
const char TranslatePrefs::kPrefTranslateDeniedCount[] =
    "translate_denied_count_for_language";
const char TranslatePrefs::kPrefTranslateIgnoredCount[] =
    "translate_ignored_count_for_language";
const char TranslatePrefs::kPrefTranslateAcceptedCount[] =
    "translate_accepted_count";
const char TranslatePrefs::kPrefTranslateBlockedLanguages[] =
    "translate_blocked_languages";
const char TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage[] =
    "translate_last_denied_time_for_language";
const char TranslatePrefs::kPrefTranslateTooOftenDeniedForLanguage[] =
    "translate_too_often_denied_for_language";

const char kTranslateUI2016Q2TrialName[] = "TranslateUI2016Q2";
const char kAlwaysTranslateOfferThreshold[] =
    "always_translate_offer_threshold";

// For reading ULP prefs.
const char kConfidence[] = "confidence";
const char kLanguage[] = "language";
const char kPreference[] = "preference";
const char kProbability[] = "probability";
const char kReading[] = "reading";

// The below properties used to be used but now are deprecated. Don't use them
// since an old profile might have some values there.
//
// * translate_last_denied_time
// * translate_too_often_denied
// * translate_language_blacklist

namespace {

// Expands language codes to make these more suitable for Accept-Language.
// Example: ['en-US', 'ja', 'en-CA'] => ['en-US', 'en', 'ja', 'en-CA'].
// 'en' won't appear twice as this function eliminates duplicates.
void ExpandLanguageCodes(const std::vector<std::string>& languages,
                         std::vector<std::string>* expanded_languages) {
  DCHECK(expanded_languages);
  DCHECK(expanded_languages->empty());

  // used to eliminate duplicates.
  std::set<std::string> seen;

  for (std::vector<std::string>::const_iterator it = languages.begin();
       it != languages.end(); ++it) {
    const std::string& language = *it;
    if (seen.find(language) == seen.end()) {
      expanded_languages->push_back(language);
      seen.insert(language);
    }

    std::vector<std::string> tokens = base::SplitString(
        language, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (tokens.size() == 0)
      continue;
    const std::string& main_part = tokens[0];
    if (seen.find(main_part) == seen.end()) {
      expanded_languages->push_back(main_part);
      seen.insert(main_part);
    }
  }
}

}  // namespace

const base::Feature kTranslateUI2016Q2{"TranslateUI2016Q2",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

DenialTimeUpdate::DenialTimeUpdate(PrefService* prefs,
                                   const std::string& language,
                                   size_t max_denial_count)
    : denial_time_dict_update_(
          prefs,
          TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage),
      language_(language),
      max_denial_count_(max_denial_count),
      time_list_(nullptr) {}

DenialTimeUpdate::~DenialTimeUpdate() {}

// Gets the list of timestamps when translation was denied.
base::ListValue* DenialTimeUpdate::GetDenialTimes() {
  if (time_list_)
    return time_list_;

  // Any consumer of GetDenialTimes _will_ write to them, so let's get an
  // update started.
  base::DictionaryValue* denial_time_dict = denial_time_dict_update_.Get();
  DCHECK(denial_time_dict);

  base::Value* denial_value = nullptr;
  bool has_value = denial_time_dict->Get(language_, &denial_value);
  bool has_list = has_value && denial_value->GetAsList(&time_list_);

  if (!has_list) {
    time_list_ = new base::ListValue();
    double oldest_denial_time = 0;
    bool has_old_style =
        has_value && denial_value->GetAsDouble(&oldest_denial_time);
    if (has_old_style)
      time_list_->AppendDouble(oldest_denial_time);
    denial_time_dict->Set(language_, base::WrapUnique(time_list_));
  }
  return time_list_;
}

base::Time DenialTimeUpdate::GetOldestDenialTime() {
  double oldest_time;
  bool result = GetDenialTimes()->GetDouble(0, &oldest_time);
  if (!result)
    return base::Time();
  return base::Time::FromJsTime(oldest_time);
}

void DenialTimeUpdate::AddDenialTime(base::Time denial_time) {
  DCHECK(GetDenialTimes());
  GetDenialTimes()->AppendDouble(denial_time.ToJsTime());

  while (GetDenialTimes()->GetSize() >= max_denial_count_)
    GetDenialTimes()->Remove(0, nullptr);
}

TranslatePrefs::TranslatePrefs(PrefService* user_prefs,
                               const char* accept_languages_pref,
                               const char* preferred_languages_pref)
    : accept_languages_pref_(accept_languages_pref), prefs_(user_prefs) {
#if defined(OS_CHROMEOS)
  preferred_languages_pref_ = preferred_languages_pref;
#else
  DCHECK(!preferred_languages_pref);
#endif
}

void TranslatePrefs::SetCountry(const std::string& country) {
  country_ = country;
}

std::string TranslatePrefs::GetCountry() const {
  return country_;
}

void TranslatePrefs::ResetToDefaults() {
  ClearBlockedLanguages();
  ClearBlacklistedSites();
  ClearWhitelistedLanguagePairs();

  std::vector<std::string> languages;
  GetLanguageList(&languages);
  for (std::vector<std::string>::const_iterator it = languages.begin();
       it != languages.end(); ++it) {
    const std::string& language = *it;
    ResetTranslationAcceptedCount(language);
    ResetTranslationDeniedCount(language);
    ResetTranslationIgnoredCount(language);
  }

  prefs_->ClearPref(kPrefTranslateLastDeniedTimeForLanguage);
  prefs_->ClearPref(kPrefTranslateTooOftenDeniedForLanguage);
}

bool TranslatePrefs::IsBlockedLanguage(
    const std::string& original_language) const {
  return IsValueBlacklisted(kPrefTranslateBlockedLanguages, original_language);
}

void TranslatePrefs::BlockLanguage(const std::string& original_language) {
  BlacklistValue(kPrefTranslateBlockedLanguages, original_language);

  // Add the language to the language list at chrome://settings/languages.
  std::string language = original_language;
  translate::ToChromeLanguageSynonym(&language);

  std::vector<std::string> languages;
  GetLanguageList(&languages);

  if (std::find(languages.begin(), languages.end(), language) ==
      languages.end()) {
    languages.push_back(language);
    UpdateLanguageList(languages);
  }
}

void TranslatePrefs::UnblockLanguage(const std::string& original_language) {
  RemoveValueFromBlacklist(kPrefTranslateBlockedLanguages, original_language);
}

bool TranslatePrefs::IsSiteBlacklisted(const std::string& site) const {
  return IsValueBlacklisted(kPrefTranslateSiteBlacklist, site);
}

void TranslatePrefs::BlacklistSite(const std::string& site) {
  BlacklistValue(kPrefTranslateSiteBlacklist, site);
}

void TranslatePrefs::RemoveSiteFromBlacklist(const std::string& site) {
  RemoveValueFromBlacklist(kPrefTranslateSiteBlacklist, site);
}

bool TranslatePrefs::IsLanguagePairWhitelisted(
    const std::string& original_language,
    const std::string& target_language) {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateWhitelists);
  if (dict && !dict->empty()) {
    std::string auto_target_lang;
    if (dict->GetString(original_language, &auto_target_lang) &&
        auto_target_lang == target_language)
      return true;
  }
  return false;
}

void TranslatePrefs::WhitelistLanguagePair(const std::string& original_language,
                                           const std::string& target_language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  base::DictionaryValue* dict = update.Get();
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  dict->SetString(original_language, target_language);
}

void TranslatePrefs::RemoveLanguagePairFromWhitelist(
    const std::string& original_language,
    const std::string& target_language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  base::DictionaryValue* dict = update.Get();
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  dict->Remove(original_language, NULL);
}

bool TranslatePrefs::HasBlockedLanguages() const {
  return !IsListEmpty(kPrefTranslateBlockedLanguages);
}

void TranslatePrefs::ClearBlockedLanguages() {
  prefs_->ClearPref(kPrefTranslateBlockedLanguages);
}

bool TranslatePrefs::HasBlacklistedSites() const {
  return !IsListEmpty(kPrefTranslateSiteBlacklist);
}

void TranslatePrefs::ClearBlacklistedSites() {
  prefs_->ClearPref(kPrefTranslateSiteBlacklist);
}

bool TranslatePrefs::HasWhitelistedLanguagePairs() const {
  return !IsDictionaryEmpty(kPrefTranslateWhitelists);
}

void TranslatePrefs::ClearWhitelistedLanguagePairs() {
  prefs_->ClearPref(kPrefTranslateWhitelists);
}

int TranslatePrefs::GetTranslationDeniedCount(
    const std::string& language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateDeniedCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationDeniedCount(
    const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  base::DictionaryValue* dict = update.Get();

  int count = 0;
  dict->GetInteger(language, &count);
  dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationDeniedCount(const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  update.Get()->SetInteger(language, 0);
}

int TranslatePrefs::GetTranslationIgnoredCount(
    const std::string& language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateIgnoredCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationIgnoredCount(
    const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  base::DictionaryValue* dict = update.Get();

  int count = 0;
  dict->GetInteger(language, &count);
  dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationIgnoredCount(const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateIgnoredCount);
  update.Get()->SetInteger(language, 0);
}

int TranslatePrefs::GetTranslationAcceptedCount(
    const std::string& language) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateAcceptedCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationAcceptedCount(
    const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  base::DictionaryValue* dict = update.Get();
  int count = 0;
  dict->GetInteger(language, &count);
  dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationAcceptedCount(
    const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  update.Get()->SetInteger(language, 0);
}

void TranslatePrefs::UpdateLastDeniedTime(const std::string& language) {
  if (IsTooOftenDenied(language))
    return;

  DenialTimeUpdate update(prefs_, language, 2);
  base::Time now = base::Time::Now();
  base::Time oldest_denial_time = update.GetOldestDenialTime();
  update.AddDenialTime(now);

  if (oldest_denial_time.is_null())
    return;

  if (now - oldest_denial_time <= base::TimeDelta::FromDays(1)) {
    DictionaryPrefUpdate update(prefs_,
                                kPrefTranslateTooOftenDeniedForLanguage);
    update.Get()->SetBoolean(language, true);
  }
}

bool TranslatePrefs::IsTooOftenDenied(const std::string& language) const {
  if (base::FeatureList::IsEnabled(kTranslateUI2016Q2)) {
    // In the new logic, we only hide the bubble if user denied it more than
    // 3 times or the user ignored it more than 10 times.
    return (GetTranslationDeniedCount(language) > 3) ||
           (GetTranslationIgnoredCount(language) > 10);
  } else {
    const base::DictionaryValue* dict =
        prefs_->GetDictionary(kPrefTranslateTooOftenDeniedForLanguage);
    bool result = false;
    return dict->GetBoolean(language, &result) ? result : false;
  }
}

void TranslatePrefs::ResetDenialState() {
  prefs_->ClearPref(kPrefTranslateLastDeniedTimeForLanguage);
  prefs_->ClearPref(kPrefTranslateTooOftenDeniedForLanguage);
}

void TranslatePrefs::GetLanguageList(
    std::vector<std::string>* languages) const {
  DCHECK(languages);
  DCHECK(languages->empty());

#if defined(OS_CHROMEOS)
  const char* key = preferred_languages_pref_.c_str();
#else
  const char* key = accept_languages_pref_.c_str();
#endif

  *languages = base::SplitString(prefs_->GetString(key), ",",
                                 base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

void TranslatePrefs::UpdateLanguageList(
    const std::vector<std::string>& languages) {
#if defined(OS_CHROMEOS)
  std::string languages_str = base::JoinString(languages, ",");
  prefs_->SetString(preferred_languages_pref_.c_str(), languages_str);
#endif

  // Save the same language list as accept languages preference as well, but we
  // need to expand the language list, to make it more acceptable. For instance,
  // some web sites don't understand 'en-US' but 'en'. See crosbug.com/9884.
  std::vector<std::string> accept_languages;
  ExpandLanguageCodes(languages, &accept_languages);
  std::string accept_languages_str = base::JoinString(accept_languages, ",");
  prefs_->SetString(accept_languages_pref_.c_str(), accept_languages_str);
}

bool TranslatePrefs::CanTranslateLanguage(
    TranslateAcceptLanguages* accept_languages,
    const std::string& language) {
  bool can_be_accept_language =
      TranslateAcceptLanguages::CanBeAcceptLanguage(language);
  bool is_accept_language = accept_languages->IsAcceptLanguage(language);

  // For the translate language experiment, blocklists can be overridden.
  const std::string& app_locale =
      TranslateDownloadManager::GetInstance()->application_locale();
  std::string ui_lang = TranslateDownloadManager::GetLanguageCode(app_locale);
  if (TranslateExperiment::ShouldOverrideBlocking(ui_lang, language))
    return true;

  // Don't translate any user black-listed languages. Checking
  // |is_accept_language| is necessary because if the user eliminates the
  // language from the preference, it is natural to forget whether or not
  // the language should be translated. Checking |cannot_be_accept_language|
  // is also necessary because some minor languages can't be selected in the
  // language preference even though the language is available in Translate
  // server.
  if (IsBlockedLanguage(language) &&
      (is_accept_language || !can_be_accept_language))
    return false;

  return true;
}

bool TranslatePrefs::ShouldAutoTranslate(const std::string& original_language,
                                         std::string* target_language) {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateWhitelists);
  if (dict && dict->GetString(original_language, target_language)) {
    DCHECK(!target_language->empty());
    return !target_language->empty();
  }
  return false;
}

// static
void TranslatePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPrefTranslateSiteBlacklist,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateWhitelists,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateDeniedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateIgnoredCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateAcceptedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(kPrefTranslateBlockedLanguages,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(kPrefTranslateLastDeniedTimeForLanguage);
  registry->RegisterDictionaryPref(
      kPrefTranslateTooOftenDeniedForLanguage,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefLanguageProfile,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

// static
void TranslatePrefs::MigrateUserPrefs(PrefService* user_prefs,
                                      const char* accept_languages_pref) {
  // Old format of kPrefTranslateWhitelists
  // - original language -> list of target langs to auto-translate
  // - list of langs is in order of being enabled i.e. last in list is the
  //   most recent language that user enabled via
  //   Always translate |source_lang| to |target_lang|"
  // - this results in a one-to-n relationship between source lang and target
  //   langs.
  // New format:
  // - original language -> one target language to auto-translate
  // - each time that the user enables the "Always translate..." option, that
  //   target lang overwrites the previous one.
  // - this results in a one-to-one relationship between source lang and target
  //   lang
  // - we replace old list of target langs with the last target lang in list,
  //   assuming the last (i.e. most recent) target lang is what user wants to
  //   keep auto-translated.
  DictionaryPrefUpdate update(user_prefs, kPrefTranslateWhitelists);
  base::DictionaryValue* dict = update.Get();
  if (dict && !dict->empty()) {
    base::DictionaryValue::Iterator iter(*dict);
    while (!iter.IsAtEnd()) {
      const base::ListValue* list = NULL;
      if (!iter.value().GetAsList(&list) || !list)
        break;  // Dictionary has either been migrated or new format.
      std::string key = iter.key();
      // Advance the iterator before removing the current element.
      iter.Advance();
      std::string target_lang;
      if (list->empty() ||
          !list->GetString(list->GetSize() - 1, &target_lang) ||
          target_lang.empty()) {
        dict->Remove(key, NULL);
      } else {
        dict->SetString(key, target_lang);
      }
    }
  }
}

bool TranslatePrefs::IsValueInList(const base::ListValue* list,
                                   const std::string& in_value) const {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string value;
    if (list->GetString(i, &value) && value == in_value)
      return true;
  }
  return false;
}

bool TranslatePrefs::IsValueBlacklisted(const char* pref_id,
                                        const std::string& value) const {
  const base::ListValue* blacklist = prefs_->GetList(pref_id);
  return (blacklist && !blacklist->empty() && IsValueInList(blacklist, value));
}

void TranslatePrefs::BlacklistValue(const char* pref_id,
                                    const std::string& value) {
  {
    ListPrefUpdate update(prefs_, pref_id);
    base::ListValue* blacklist = update.Get();
    if (!blacklist) {
      NOTREACHED() << "Unregistered translate blacklist pref";
      return;
    }
    blacklist->AppendString(value);
  }
}

void TranslatePrefs::RemoveValueFromBlacklist(const char* pref_id,
                                              const std::string& value) {
  ListPrefUpdate update(prefs_, pref_id);
  base::ListValue* blacklist = update.Get();
  if (!blacklist) {
    NOTREACHED() << "Unregistered translate blacklist pref";
    return;
  }
  base::Value string_value(value);
  blacklist->Remove(string_value, NULL);
}

bool TranslatePrefs::IsListEmpty(const char* pref_id) const {
  const base::ListValue* blacklist = prefs_->GetList(pref_id);
  return (blacklist == NULL || blacklist->empty());
}

bool TranslatePrefs::IsDictionaryEmpty(const char* pref_id) const {
  const base::DictionaryValue* dict = prefs_->GetDictionary(pref_id);
  return (dict == NULL || dict->empty());
}

double TranslatePrefs::GetReadingFromUserLanguageProfile(
    LanguageAndProbabilityList* out_value) const {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kPrefLanguageProfile);
  const base::DictionaryValue* entries = nullptr;

  // Return 0.0 if no ULP prefs.
  if (!dict)
    return 0.0;

  // Return 0.0 if no such list.
  if (!dict->GetDictionary(kReading, &entries))
    return 0.0;

  double confidence = 0.0;
  // Return 0.0 if cannot find confidence.
  if (!entries->GetDouble(kConfidence, &confidence))
    return 0.0;

  const base::ListValue* preference = nullptr;
  // Return the confidence if there are no item on the 'preference' field.
  if (!entries->GetList(kPreference, &preference))
    return confidence;

  // Use a map to fold the probability of all the same normalized language
  // code together.
  std::map<std::string, double> probability_map;
  // Iterate through the preference.
  for (const auto& entry : *preference) {
    const base::DictionaryValue* item = nullptr;
    std::string language;
    double probability = 0.0;
    if (entry->GetAsDictionary(&item) &&
        item->GetString(kLanguage, &language) &&
        item->GetDouble(kProbability, &probability)) {
      // Normalize the the language code known and supported by
      // Translate.
      translate::ToTranslateLanguageSynonym(&language);
      // Discard if the normalized version is unsupported.
      if (TranslateDownloadManager::IsSupportedLanguage(language)) {
        probability_map[language] += probability;
      }
    }
  }
  for (const auto& it : probability_map)
    out_value->push_back(it);
  std::sort(out_value->begin(), out_value->end(),
            [](const LanguageAndProbability& left,
               const LanguageAndProbability& right) {
              return left.second > right.second;
            });
  return confidence;
}

}  // namespace translate
