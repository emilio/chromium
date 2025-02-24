// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/browser/google_switches.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {

SearchEngineType GetEngineType(const std::string& url) {
  TemplateURLData data;
  data.SetURL(url);
  return TemplateURL(data).GetEngineType(SearchTermsData());
}

std::string GetHostFromTemplateURLData(const TemplateURLData& data) {
  return TemplateURL(data).url_ref().GetHost(SearchTermsData());
}

}  // namespace

class TemplateURLPrepopulateDataTest : public testing::Test {
 public:
  void SetUp() override {
    TemplateURLPrepopulateData::RegisterProfilePrefs(prefs_.registry());
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

// Verifies the set of prepopulate data doesn't contain entries with duplicate
// ids.
TEST_F(TemplateURLPrepopulateDataTest, UniqueIDs) {
  const int kCountryIds[] = {
      'A'<<8|'D', 'A'<<8|'E', 'A'<<8|'F', 'A'<<8|'G', 'A'<<8|'I',
      'A'<<8|'L', 'A'<<8|'M', 'A'<<8|'N', 'A'<<8|'O', 'A'<<8|'Q',
      'A'<<8|'R', 'A'<<8|'S', 'A'<<8|'T', 'A'<<8|'U', 'A'<<8|'W',
      'A'<<8|'X', 'A'<<8|'Z', 'B'<<8|'A', 'B'<<8|'B', 'B'<<8|'D',
      'B'<<8|'E', 'B'<<8|'F', 'B'<<8|'G', 'B'<<8|'H', 'B'<<8|'I',
      'B'<<8|'J', 'B'<<8|'M', 'B'<<8|'N', 'B'<<8|'O', 'B'<<8|'R',
      'B'<<8|'S', 'B'<<8|'T', 'B'<<8|'V', 'B'<<8|'W', 'B'<<8|'Y',
      'B'<<8|'Z', 'C'<<8|'A', 'C'<<8|'C', 'C'<<8|'D', 'C'<<8|'F',
      'C'<<8|'G', 'C'<<8|'H', 'C'<<8|'I', 'C'<<8|'K', 'C'<<8|'L',
      'C'<<8|'M', 'C'<<8|'N', 'C'<<8|'O', 'C'<<8|'R', 'C'<<8|'U',
      'C'<<8|'V', 'C'<<8|'X', 'C'<<8|'Y', 'C'<<8|'Z', 'D'<<8|'E',
      'D'<<8|'J', 'D'<<8|'K', 'D'<<8|'M', 'D'<<8|'O', 'D'<<8|'Z',
      'E'<<8|'C', 'E'<<8|'E', 'E'<<8|'G', 'E'<<8|'R', 'E'<<8|'S',
      'E'<<8|'T', 'F'<<8|'I', 'F'<<8|'J', 'F'<<8|'K', 'F'<<8|'M',
      'F'<<8|'O', 'F'<<8|'R', 'G'<<8|'A', 'G'<<8|'B', 'G'<<8|'D',
      'G'<<8|'E', 'G'<<8|'F', 'G'<<8|'G', 'G'<<8|'H', 'G'<<8|'I',
      'G'<<8|'L', 'G'<<8|'M', 'G'<<8|'N', 'G'<<8|'P', 'G'<<8|'Q',
      'G'<<8|'R', 'G'<<8|'S', 'G'<<8|'T', 'G'<<8|'U', 'G'<<8|'W',
      'G'<<8|'Y', 'H'<<8|'K', 'H'<<8|'M', 'H'<<8|'N', 'H'<<8|'R',
      'H'<<8|'T', 'H'<<8|'U', 'I'<<8|'D', 'I'<<8|'E', 'I'<<8|'L',
      'I'<<8|'M', 'I'<<8|'N', 'I'<<8|'O', 'I'<<8|'P', 'I'<<8|'Q',
      'I'<<8|'R', 'I'<<8|'S', 'I'<<8|'T', 'J'<<8|'E', 'J'<<8|'M',
      'J'<<8|'O', 'J'<<8|'P', 'K'<<8|'E', 'K'<<8|'G', 'K'<<8|'H',
      'K'<<8|'I', 'K'<<8|'M', 'K'<<8|'N', 'K'<<8|'P', 'K'<<8|'R',
      'K'<<8|'W', 'K'<<8|'Y', 'K'<<8|'Z', 'L'<<8|'A', 'L'<<8|'B',
      'L'<<8|'C', 'L'<<8|'I', 'L'<<8|'K', 'L'<<8|'R', 'L'<<8|'S',
      'L'<<8|'T', 'L'<<8|'U', 'L'<<8|'V', 'L'<<8|'Y', 'M'<<8|'A',
      'M'<<8|'C', 'M'<<8|'D', 'M'<<8|'E', 'M'<<8|'G', 'M'<<8|'H',
      'M'<<8|'K', 'M'<<8|'L', 'M'<<8|'M', 'M'<<8|'N', 'M'<<8|'O',
      'M'<<8|'P', 'M'<<8|'Q', 'M'<<8|'R', 'M'<<8|'S', 'M'<<8|'T',
      'M'<<8|'U', 'M'<<8|'V', 'M'<<8|'W', 'M'<<8|'X', 'M'<<8|'Y',
      'M'<<8|'Z', 'N'<<8|'A', 'N'<<8|'C', 'N'<<8|'E', 'N'<<8|'F',
      'N'<<8|'G', 'N'<<8|'I', 'N'<<8|'L', 'N'<<8|'O', 'N'<<8|'P',
      'N'<<8|'R', 'N'<<8|'U', 'N'<<8|'Z', 'O'<<8|'M', 'P'<<8|'A',
      'P'<<8|'E', 'P'<<8|'F', 'P'<<8|'G', 'P'<<8|'H', 'P'<<8|'K',
      'P'<<8|'L', 'P'<<8|'M', 'P'<<8|'N', 'P'<<8|'R', 'P'<<8|'S',
      'P'<<8|'T', 'P'<<8|'W', 'P'<<8|'Y', 'Q'<<8|'A', 'R'<<8|'E',
      'R'<<8|'O', 'R'<<8|'S', 'R'<<8|'U', 'R'<<8|'W', 'S'<<8|'A',
      'S'<<8|'B', 'S'<<8|'C', 'S'<<8|'D', 'S'<<8|'E', 'S'<<8|'G',
      'S'<<8|'H', 'S'<<8|'I', 'S'<<8|'J', 'S'<<8|'K', 'S'<<8|'L',
      'S'<<8|'M', 'S'<<8|'N', 'S'<<8|'O', 'S'<<8|'R', 'S'<<8|'T',
      'S'<<8|'V', 'S'<<8|'Y', 'S'<<8|'Z', 'T'<<8|'C', 'T'<<8|'D',
      'T'<<8|'F', 'T'<<8|'G', 'T'<<8|'H', 'T'<<8|'J', 'T'<<8|'K',
      'T'<<8|'L', 'T'<<8|'M', 'T'<<8|'N', 'T'<<8|'O', 'T'<<8|'R',
      'T'<<8|'T', 'T'<<8|'V', 'T'<<8|'W', 'T'<<8|'Z', 'U'<<8|'A',
      'U'<<8|'G', 'U'<<8|'M', 'U'<<8|'S', 'U'<<8|'Y', 'U'<<8|'Z',
      'V'<<8|'A', 'V'<<8|'C', 'V'<<8|'E', 'V'<<8|'G', 'V'<<8|'I',
      'V'<<8|'N', 'V'<<8|'U', 'W'<<8|'F', 'W'<<8|'S', 'Y'<<8|'E',
      'Y'<<8|'T', 'Z'<<8|'A', 'Z'<<8|'M', 'Z'<<8|'W', -1 };

  for (size_t i = 0; i < arraysize(kCountryIds); ++i) {
    prefs_.SetInteger(prefs::kCountryIDAtInstall, kCountryIds[i]);
    std::vector<std::unique_ptr<TemplateURLData>> urls =
        TemplateURLPrepopulateData::GetPrepopulatedEngines(&prefs_, nullptr);
    std::set<int> unique_ids;
    for (size_t turl_i = 0; turl_i < urls.size(); ++turl_i) {
      ASSERT_TRUE(unique_ids.find(urls[turl_i]->prepopulate_id) ==
                  unique_ids.end());
      unique_ids.insert(urls[turl_i]->prepopulate_id);
    }
  }
}

// Verifies that default search providers from the preferences file
// override the built-in ones.
TEST_F(TemplateURLPrepopulateDataTest, ProvidersFromPrefs) {
  prefs_.SetUserPref(prefs::kSearchProviderOverridesVersion,
                     new base::Value(1));
  base::ListValue* overrides = new base::ListValue;
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  // Set only the minimal required settings for a search provider configuration.
  entry->SetString("name", "foo");
  entry->SetString("keyword", "fook");
  entry->SetString("search_url", "http://foo.com/s?q={searchTerms}");
  entry->SetString("favicon_url", "http://foi.com/favicon.ico");
  entry->SetString("encoding", "UTF-8");
  entry->SetInteger("id", 1001);
  overrides->Append(entry->CreateDeepCopy());
  prefs_.SetUserPref(prefs::kSearchProviderOverrides, overrides);

  int version = TemplateURLPrepopulateData::GetDataVersion(&prefs_);
  EXPECT_EQ(1, version);

  size_t default_index;
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(&prefs_,
                                                         &default_index);

  ASSERT_EQ(1u, t_urls.size());
  EXPECT_EQ(ASCIIToUTF16("foo"), t_urls[0]->short_name());
  EXPECT_EQ(ASCIIToUTF16("fook"), t_urls[0]->keyword());
  EXPECT_EQ("foo.com", GetHostFromTemplateURLData(*t_urls[0]));
  EXPECT_EQ("foi.com", t_urls[0]->favicon_url.host());
  EXPECT_EQ(1u, t_urls[0]->input_encodings.size());
  EXPECT_EQ(1001, t_urls[0]->prepopulate_id);
  EXPECT_TRUE(t_urls[0]->suggestions_url.empty());
  EXPECT_TRUE(t_urls[0]->instant_url.empty());
  EXPECT_EQ(0u, t_urls[0]->alternate_urls.size());
  EXPECT_TRUE(t_urls[0]->search_terms_replacement_key.empty());
  EXPECT_TRUE(t_urls[0]->safe_for_autoreplace);
  EXPECT_TRUE(t_urls[0]->date_created.is_null());
  EXPECT_TRUE(t_urls[0]->last_modified.is_null());

  // Test the optional settings too.
  entry->SetString("suggest_url", "http://foo.com/suggest?q={searchTerms}");
  entry->SetString("instant_url", "http://foo.com/instant?q={searchTerms}");
  base::ListValue* alternate_urls = new base::ListValue;
  alternate_urls->AppendString("http://foo.com/alternate?q={searchTerms}");
  entry->Set("alternate_urls", alternate_urls);
  entry->SetString("search_terms_replacement_key", "espv");
  overrides = new base::ListValue;
  overrides->Append(entry->CreateDeepCopy());
  prefs_.SetUserPref(prefs::kSearchProviderOverrides, overrides);

  t_urls = TemplateURLPrepopulateData::GetPrepopulatedEngines(
      &prefs_, &default_index);
  ASSERT_EQ(1u, t_urls.size());
  EXPECT_EQ(ASCIIToUTF16("foo"), t_urls[0]->short_name());
  EXPECT_EQ(ASCIIToUTF16("fook"), t_urls[0]->keyword());
  EXPECT_EQ("foo.com", GetHostFromTemplateURLData(*t_urls[0]));
  EXPECT_EQ("foi.com", t_urls[0]->favicon_url.host());
  EXPECT_EQ(1u, t_urls[0]->input_encodings.size());
  EXPECT_EQ(1001, t_urls[0]->prepopulate_id);
  EXPECT_EQ("http://foo.com/suggest?q={searchTerms}",
            t_urls[0]->suggestions_url);
  EXPECT_EQ("http://foo.com/instant?q={searchTerms}",
            t_urls[0]->instant_url);
  ASSERT_EQ(1u, t_urls[0]->alternate_urls.size());
  EXPECT_EQ("http://foo.com/alternate?q={searchTerms}",
            t_urls[0]->alternate_urls[0]);
  EXPECT_EQ("espv", t_urls[0]->search_terms_replacement_key);

  // Test that subsequent providers are loaded even if an intermediate
  // provider has an incomplete configuration.
  overrides = new base::ListValue;
  overrides->Append(entry->CreateDeepCopy());
  entry->SetInteger("id", 1002);
  entry->SetString("name", "bar");
  entry->SetString("keyword", "bark");
  entry->SetString("encoding", std::string());
  overrides->Append(entry->CreateDeepCopy());
  entry->SetInteger("id", 1003);
  entry->SetString("name", "baz");
  entry->SetString("keyword", "bazk");
  entry->SetString("encoding", "UTF-8");
  overrides->Append(entry->CreateDeepCopy());
  prefs_.SetUserPref(prefs::kSearchProviderOverrides, overrides);

  t_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(&prefs_,
                                                         &default_index);
  EXPECT_EQ(2u, t_urls.size());
}

TEST_F(TemplateURLPrepopulateDataTest, ClearProvidersFromPrefs) {
  prefs_.SetUserPref(prefs::kSearchProviderOverridesVersion,
                     new base::Value(1));
  base::ListValue* overrides = new base::ListValue;
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  // Set only the minimal required settings for a search provider configuration.
  entry->SetString("name", "foo");
  entry->SetString("keyword", "fook");
  entry->SetString("search_url", "http://foo.com/s?q={searchTerms}");
  entry->SetString("favicon_url", "http://foi.com/favicon.ico");
  entry->SetString("encoding", "UTF-8");
  entry->SetInteger("id", 1001);
  overrides->Append(std::move(entry));
  prefs_.SetUserPref(prefs::kSearchProviderOverrides, overrides);

  int version = TemplateURLPrepopulateData::GetDataVersion(&prefs_);
  EXPECT_EQ(1, version);

  // This call removes the above search engine.
  TemplateURLPrepopulateData::ClearPrepopulatedEnginesInPrefs(&prefs_);

  version = TemplateURLPrepopulateData::GetDataVersion(&prefs_);
  EXPECT_EQ(TemplateURLPrepopulateData::kCurrentDataVersion, version);

  size_t default_index;
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(&prefs_,
                                                         &default_index);
  ASSERT_FALSE(t_urls.empty());
  for (size_t i = 0; i < t_urls.size(); ++i) {
    EXPECT_NE(ASCIIToUTF16("foo"), t_urls[i]->short_name());
    EXPECT_NE(ASCIIToUTF16("fook"), t_urls[i]->keyword());
    EXPECT_NE("foi.com", t_urls[i]->favicon_url.host());
    EXPECT_NE("foo.com", GetHostFromTemplateURLData(*t_urls[i]));
    EXPECT_NE(1001, t_urls[i]->prepopulate_id);
  }
  // Ensures the default URL is Google and has the optional fields filled.
  EXPECT_EQ(ASCIIToUTF16("Google"), t_urls[default_index]->short_name());
  EXPECT_FALSE(t_urls[default_index]->suggestions_url.empty());
  EXPECT_FALSE(t_urls[default_index]->instant_url.empty());
  EXPECT_FALSE(t_urls[default_index]->image_url.empty());
  EXPECT_FALSE(t_urls[default_index]->new_tab_url.empty());
  EXPECT_FALSE(t_urls[default_index]->contextual_search_url.empty());
  EXPECT_FALSE(t_urls[default_index]->image_url_post_params.empty());
  EXPECT_EQ(SEARCH_ENGINE_GOOGLE,
            TemplateURL(*t_urls[default_index]).GetEngineType(
                SearchTermsData()));
}

// Verifies that built-in search providers are processed correctly.
TEST_F(TemplateURLPrepopulateDataTest, ProvidersFromPrepopulated) {
  // Use United States.
  prefs_.SetInteger(prefs::kCountryIDAtInstall, 'U'<<8|'S');
  size_t default_index;
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(&prefs_,
                                                         &default_index);

  // Ensure all the URLs have the required fields populated.
  ASSERT_FALSE(t_urls.empty());
  for (size_t i = 0; i < t_urls.size(); ++i) {
    ASSERT_FALSE(t_urls[i]->short_name().empty());
    ASSERT_FALSE(t_urls[i]->keyword().empty());
    ASSERT_FALSE(t_urls[i]->favicon_url.host().empty());
    ASSERT_FALSE(GetHostFromTemplateURLData(*t_urls[i]).empty());
    ASSERT_FALSE(t_urls[i]->input_encodings.empty());
    EXPECT_GT(t_urls[i]->prepopulate_id, 0);
    EXPECT_TRUE(t_urls[0]->safe_for_autoreplace);
    EXPECT_TRUE(t_urls[0]->date_created.is_null());
    EXPECT_TRUE(t_urls[0]->last_modified.is_null());
  }

  // Ensures the default URL is Google and has the optional fields filled.
  EXPECT_EQ(ASCIIToUTF16("Google"), t_urls[default_index]->short_name());
  EXPECT_FALSE(t_urls[default_index]->suggestions_url.empty());
  EXPECT_FALSE(t_urls[default_index]->instant_url.empty());
  EXPECT_FALSE(t_urls[default_index]->image_url.empty());
  EXPECT_FALSE(t_urls[default_index]->new_tab_url.empty());
  EXPECT_FALSE(t_urls[default_index]->contextual_search_url.empty());
  EXPECT_FALSE(t_urls[default_index]->image_url_post_params.empty());
  // Expect at least 2 alternate_urls.
  // This caught a bug with static initialization of arrays, so leave this in.
  EXPECT_GT(t_urls[default_index]->alternate_urls.size(), 1u);
  for (size_t i = 0; i < t_urls[default_index]->alternate_urls.size(); ++i)
    EXPECT_FALSE(t_urls[default_index]->alternate_urls[i].empty());
  EXPECT_EQ(SEARCH_ENGINE_GOOGLE,
            TemplateURL(*t_urls[default_index]).GetEngineType(
                SearchTermsData()));
  EXPECT_FALSE(t_urls[default_index]->search_terms_replacement_key.empty());
}

TEST_F(TemplateURLPrepopulateDataTest, GetEngineTypeBasic) {
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType("http://example.com/"));
  EXPECT_EQ(SEARCH_ENGINE_ASK, GetEngineType("http://www.ask.com/"));
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType("http://search.atlas.cz/"));
  EXPECT_EQ(SEARCH_ENGINE_GOOGLE, GetEngineType("http://www.google.com/"));
}

TEST_F(TemplateURLPrepopulateDataTest, GetEngineTypeAdvanced) {
  // Google URLs in different forms.
  const char* kGoogleURLs[] = {
    // Original with google:baseURL:
    "{google:baseURL}search?q={searchTerms}&{google:RLZ}"
    "{google:originalQueryForSuggestion}{google:searchFieldtrialParameter}"
    "sourceid=chrome&ie={inputEncoding}",
    // Custom with google.com and reordered query params:
    "http://google.com/search?{google:RLZ}{google:originalQueryForSuggestion}"
    "{google:searchFieldtrialParameter}"
    "sourceid=chrome&ie={inputEncoding}&q={searchTerms}",
    // Custom with a country TLD and almost no query params:
    "http://www.google.ru/search?q={searchTerms}"
  };
  for (size_t i = 0; i < arraysize(kGoogleURLs); ++i) {
    EXPECT_EQ(SEARCH_ENGINE_GOOGLE, GetEngineType(kGoogleURLs[i]));
  }

  // Non-Google URLs.
  const char* kYahooURLs[] = {
      "http://search.yahoo.com/search?"
      "ei={inputEncoding}&fr=crmas&p={searchTerms}",
      "http://search.yahoo.com/search?p={searchTerms}",
      // Aggressively match types by checking just TLD+1.
      "http://someothersite.yahoo.com/",
  };
  for (size_t i = 0; i < arraysize(kYahooURLs); ++i) {
    EXPECT_EQ(SEARCH_ENGINE_YAHOO, GetEngineType(kYahooURLs[i]));
  }

  // URLs for engines not present in country-specific lists.
  EXPECT_EQ(SEARCH_ENGINE_NIGMA,
            GetEngineType("http://nigma.ru/?s={searchTerms}&arg1=value1"));
  // Also test matching against alternate URLs (and TLD+1 matching).
  EXPECT_EQ(SEARCH_ENGINE_SOFTONIC,
            GetEngineType("http://test.softonic.com.br/?{searchTerms}"));

  // Search URL for which no prepopulated search provider exists.
  EXPECT_EQ(SEARCH_ENGINE_OTHER,
            GetEngineType("http://example.net/search?q={searchTerms}"));
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType("invalid:search:url"));

  // URL that doesn't look Google-related, but matches a Google base URL
  // specified on the command line.
  const std::string foo_url("http://www.foo.com/search?q={searchTerms}");
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType(foo_url));
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/");
  EXPECT_EQ(SEARCH_ENGINE_GOOGLE, GetEngineType(foo_url));
}

TEST_F(TemplateURLPrepopulateDataTest, GetEngineTypeForAllPrepopulatedEngines) {
  using PrepopulatedEngine = TemplateURLPrepopulateData::PrepopulatedEngine;
  const std::vector<const PrepopulatedEngine*> all_engines =
      TemplateURLPrepopulateData::GetAllPrepopulatedEngines();
  for (const PrepopulatedEngine* engine : all_engines) {
    std::unique_ptr<TemplateURLData> data =
        TemplateURLDataFromPrepopulatedEngine(*engine);
    EXPECT_EQ(engine->type,
              TemplateURL(*data).GetEngineType(SearchTermsData()));
  }
}
