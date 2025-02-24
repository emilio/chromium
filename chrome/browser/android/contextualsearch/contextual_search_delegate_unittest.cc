// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/android/proto/client_discourse_context.pb.h"
#include "chrome/common/chrome_switches.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/escape.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ListValue;

namespace {

const char kSomeSpecificBasePage[] = "http://some.specific.host.name.com/";
const char kDiscourseContextHeaderName[] = "X-Additional-Discourse-Context";

}  // namespace

class ContextualSearchDelegateTest : public testing::Test {
 public:
  ContextualSearchDelegateTest() {}
  ~ContextualSearchDelegateTest() override {}

 protected:
  void SetUp() override {
    request_context_ =
        new net::TestURLRequestContextGetter(io_message_loop_.task_runner());
    template_url_service_.reset(CreateTemplateURLService());
    delegate_.reset(new ContextualSearchDelegate(
        request_context_.get(), template_url_service_.get(),
        base::Bind(
            &ContextualSearchDelegateTest::recordSearchTermResolutionResponse,
            base::Unretained(this)),
        base::Bind(&ContextualSearchDelegateTest::recordSurroundingText,
                   base::Unretained(this)),
        base::Bind(&ContextualSearchDelegateTest::recordIcingSelectionAvailable,
                   base::Unretained(this))));
  }

  void TearDown() override {
    fetcher_ = NULL;
    is_invalid_ = true;
    response_code_ = -1;
    search_term_ = "invalid";
    mid_ = "";
    display_text_ = "unknown";
    context_language_ = "";
  }

  TemplateURLService* CreateTemplateURLService() {
    // Set a default search provider that supports Contextual Search.
    TemplateURLData data;
    data.SetURL("https://foobar.com/url?bar={searchTerms}");
    data.contextual_search_url = "https://foobar.com/_/contextualsearch?"
        "{google:contextualSearchVersion}{google:contextualSearchContextData}";
    TemplateURLService* template_url_service = new TemplateURLService(NULL, 0);
    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
    return template_url_service;
  }

  void CreateDefaultSearchContextAndRequestSearchTerm() {
    base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
    CreateSearchContextAndRequestSearchTerm("Barack Obama", surrounding, 0, 6);
  }

  void CreateSearchContextAndRequestSearchTerm(
      const std::string& selected_text,
      const base::string16& surrounding_text,
      int start_offset,
      int end_offset) {
    test_context_ = new ContextualSearchContext(
        selected_text, std::string(), GURL(kSomeSpecificBasePage), "utf-8");
    // ContextualSearchDelegate class takes ownership of the context.
    delegate_->set_context_for_testing(test_context_);

    test_context_->start_offset = start_offset;
    test_context_->end_offset = end_offset;
    test_context_->surrounding_text = surrounding_text;
    delegate_->ContinueSearchTermResolutionRequest();
    fetcher_ = test_factory_.GetFetcherByID(
        ContextualSearchDelegate::kContextualSearchURLFetcherID);
    ASSERT_TRUE(fetcher_);
    ASSERT_TRUE(fetcher());
  }

  // Allows using the vertical bar "|" as a quote character, which makes
  // test cases more readable versus the escaped double quote that is otherwise
  // needed for JSON literals.
  std::string escapeBarQuoted(std::string bar_quoted) {
    std::replace(bar_quoted.begin(), bar_quoted.end(), '|', '\"');
    return bar_quoted;
  }

  void CreateDefaultSearchWithAdditionalJsonData(
      const std::string additional_json_data) {
    CreateDefaultSearchContextAndRequestSearchTerm();
    fetcher()->set_response_code(200);
    std::string response =
        escapeBarQuoted("{|search_term|:|obama|" + additional_json_data + "}");
    fetcher()->SetResponseString(response);
    fetcher()->delegate()->OnURLFetchComplete(fetcher());

    EXPECT_FALSE(is_invalid());
    EXPECT_EQ(200, response_code());
    EXPECT_EQ("obama", search_term());
  }

  void SetResponseStringAndFetch(const std::string& selected_text,
                                 const std::string& mentions_start,
                                 const std::string& mentions_end) {
    fetcher()->set_response_code(200);
      fetcher()->SetResponseString(
          ")]}'\n"
          "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
          "\"info_text\":\"44th U.S. President\","
          "\"display_text\":\"Barack Obama\","
          "\"mentions\":[" + mentions_start + ","+ mentions_end + "],"
          "\"selected_text\":\"" + selected_text + "\","
          "\"resolved_term\":\"barack obama\"}");
      fetcher()->delegate()->OnURLFetchComplete(fetcher());
  }

  void SetSurroundingContext(const base::string16& surrounding_text,
                             int start_offset,
                             int end_offset) {
    test_context_ = new ContextualSearchContext(
        "Bogus", std::string(), GURL(kSomeSpecificBasePage), "utf-8");
    test_context_->surrounding_text = surrounding_text;
    test_context_->start_offset = start_offset;
    test_context_->end_offset = end_offset;
    // ContextualSearchDelegate class takes ownership of the context.
    delegate_->set_context_for_testing(test_context_);
  }

  // Gets the Client Discourse Context proto from the request header.
  discourse_context::ClientDiscourseContext GetDiscourseContextFromRequest() {
    discourse_context::ClientDiscourseContext cdc;
    // Make sure we can get the actual raw headers from the fake fetcher.
    net::HttpRequestHeaders fetch_headers;
    fetcher()->GetExtraRequestHeaders(&fetch_headers);
    if (fetch_headers.HasHeader(kDiscourseContextHeaderName)) {
      std::string actual_header_value;
      fetch_headers.GetHeader(kDiscourseContextHeaderName,
                              &actual_header_value);

      // Unescape, since the server memoizer expects a web-safe encoding.
      std::string unescaped_header = actual_header_value;
      std::replace(unescaped_header.begin(), unescaped_header.end(), '-', '+');
      std::replace(unescaped_header.begin(), unescaped_header.end(), '_', '/');

      // Base64 decode the header.
      std::string decoded_header;
      if (base::Base64Decode(unescaped_header, &decoded_header)) {
        cdc.ParseFromString(decoded_header);
      }
    }
    return cdc;
  }

  // Gets the base-page URL from the request, or an empty string if not present.
  std::string getBasePageUrlFromRequest() {
    std::string result;
    discourse_context::ClientDiscourseContext cdc =
        GetDiscourseContextFromRequest();
    if (cdc.display_size() > 0) {
      const discourse_context::Display& first_display = cdc.display(0);
      result = first_display.uri();
    }
    return result;
  }

  net::TestURLFetcher* fetcher() { return fetcher_; }
  bool is_invalid() { return is_invalid_; }
  int response_code() { return response_code_; }
  std::string search_term() { return search_term_; }
  std::string display_text() { return display_text_; }
  std::string alternate_term() { return alternate_term_; }
  std::string mid() { return mid_; }
  std::string caption() { return caption_; }
  std::string thumbnail_url() { return thumbnail_url_; }
  bool do_prevent_preload() { return prevent_preload_; }
  std::string after_text() { return after_text_; }
  int start_adjust() { return start_adjust_; }
  int end_adjust() { return end_adjust_; }
  std::string context_language() { return context_language_; }

  // The delegate under test.
  std::unique_ptr<ContextualSearchDelegate> delegate_;

 private:
  void recordSearchTermResolutionResponse(
      const ResolvedSearchTerm& resolved_search_term) {
    is_invalid_ = resolved_search_term.is_invalid;
    response_code_ = resolved_search_term.response_code;
    search_term_ = resolved_search_term.search_term;
    display_text_ = resolved_search_term.display_text;
    alternate_term_ = resolved_search_term.alternate_term;
    mid_ = resolved_search_term.mid;
    thumbnail_url_ = resolved_search_term.thumbnail_url;
    caption_ = resolved_search_term.caption;
    prevent_preload_ = resolved_search_term.prevent_preload;
    start_adjust_ = resolved_search_term.selection_start_adjust;
    end_adjust_ = resolved_search_term.selection_end_adjust;
    context_language_ = resolved_search_term.context_language;
  }

  void recordSurroundingText(const std::string& after_text) {
    after_text_ = after_text;
  }

  void recordIcingSelectionAvailable(const std::string& encoding,
                                     const base::string16& surrounding_text,
                                     size_t start_offset,
                                     size_t end_offset) {
    // unused.
  }

  bool is_invalid_;
  int response_code_;
  std::string search_term_;
  std::string display_text_;
  std::string alternate_term_;
  std::string mid_;
  std::string thumbnail_url_;
  std::string caption_;
  bool prevent_preload_;
  int start_adjust_;
  int end_adjust_;
  std::string after_text_;
  std::string context_language_;

  base::MessageLoopForIO io_message_loop_;
  net::TestURLFetcherFactory test_factory_;
  net::TestURLFetcher* fetcher_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

  // Will be owned by the delegate.
  ContextualSearchContext* test_context_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchDelegateTest);
};

TEST_F(ContextualSearchDelegateTest, NormalFetchWithXssiEscape) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  fetcher()->set_response_code(200);
  fetcher()->SetResponseString(
      ")]}'\n"
      "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
      "\"info_text\":\"44th U.S. President\","
      "\"display_text\":\"Barack Obama\", \"mentions\":[0,15],"
      "\"selected_text\":\"obama\", \"resolved_term\":\"barack obama\"}");
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("Barack Obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, NormalFetchWithoutXssiEscape) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  fetcher()->set_response_code(200);
  fetcher()->SetResponseString(
      "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
      "\"info_text\":\"44th U.S. President\","
      "\"display_text\":\"Barack Obama\", \"mentions\":[0,15],"
      "\"selected_text\":\"obama\", \"resolved_term\":\"barack obama\"}");
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("Barack Obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, ResponseWithNoDisplayText) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  fetcher()->set_response_code(200);
  fetcher()->SetResponseString(
      "{\"mid\":\"/m/02mjmr\",\"search_term\":\"obama\","
      "\"mentions\":[0,15]}");
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, ResponseWithPreventPreload) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  fetcher()->set_response_code(200);
  fetcher()->SetResponseString(
      "{\"mid\":\"/m/02mjmr\",\"search_term\":\"obama\","
      "\"mentions\":[0,15],\"prevent_preload\":\"1\"}");
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_TRUE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, NonJsonResponse) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  fetcher()->set_response_code(200);
  fetcher()->SetResponseString("Non-JSON Response");
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("", search_term());
  EXPECT_EQ("", display_text());
  EXPECT_EQ("", mid());
  EXPECT_FALSE(do_prevent_preload());
}

TEST_F(ContextualSearchDelegateTest, InvalidResponse) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  fetcher()->set_response_code(net::URLFetcher::RESPONSE_CODE_INVALID);
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_FALSE(do_prevent_preload());
  EXPECT_TRUE(is_invalid());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionToEnd) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Barack";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 6);
  SetResponseStringAndFetch(selected_text, "0", "12");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(6, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionToStart) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Obama";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 7, 12);
  SetResponseStringAndFetch(selected_text, "0", "12");

  EXPECT_EQ(-7, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionBothDirections) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 7, 9);
  SetResponseStringAndFetch(selected_text, "0", "12");

  EXPECT_EQ(-7, start_adjust());
  EXPECT_EQ(3, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidRange) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 7, 9);
  SetResponseStringAndFetch(selected_text, "0", "200");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidDistantStart) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding,
                                          0xffffffff, 0xffffffff - 2);
  SetResponseStringAndFetch(selected_text, "0", "12");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidNoOverlap) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 12);
  SetResponseStringAndFetch(selected_text, "12", "14");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionInvalidDistantEndAndRange) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding,
                                          0xffffffff, 0xffffffff - 2);
  SetResponseStringAndFetch(selected_text, "0", "268435455");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ExpandSelectionLargeNumbers) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Ob";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding,
                                          268435450, 268435455);
  SetResponseStringAndFetch(selected_text, "268435440", "268435455");

  EXPECT_EQ(-10, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ContractSelectionValid) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Barack Obama just";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 17);
  SetResponseStringAndFetch(selected_text, "0", "12");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(-5, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, ContractSelectionInvalid) {
  base::string16 surrounding = base::UTF8ToUTF16("Barack Obama just spoke.");
  std::string selected_text = "Barack Obama just";
  CreateSearchContextAndRequestSearchTerm(selected_text, surrounding, 0, 17);
  SetResponseStringAndFetch(selected_text, "5", "5");

  EXPECT_EQ(0, start_adjust());
  EXPECT_EQ(0, end_adjust());
}

TEST_F(ContextualSearchDelegateTest, SurroundingTextHighMaximum) {
  base::string16 surrounding = base::ASCIIToUTF16("aa bb Bogus dd ee");
  SetSurroundingContext(surrounding, 6, 11);
  delegate_->SendSurroundingText(30);  // High maximum # of surrounding chars.
  EXPECT_EQ("dd ee", after_text());
}

TEST_F(ContextualSearchDelegateTest, SurroundingTextLowMaximum) {
  base::string16 surrounding = base::ASCIIToUTF16("aa bb Bogus dd ee");
  SetSurroundingContext(surrounding, 6, 11);
  delegate_->SendSurroundingText(3);  // Low maximum # of surrounding chars.
  // Whitespaces are trimmed.
  EXPECT_EQ("dd", after_text());
}

TEST_F(ContextualSearchDelegateTest, SurroundingTextNoBeforeText) {
  base::string16 surrounding = base::ASCIIToUTF16("Bogus ee ff gg");
  SetSurroundingContext(surrounding, 0, 5);
  delegate_->SendSurroundingText(5);
  EXPECT_EQ("ee f", after_text());
}

TEST_F(ContextualSearchDelegateTest, SurroundingTextNoAfterText) {
  base::string16 surrounding = base::ASCIIToUTF16("aa bb Bogus");
  SetSurroundingContext(surrounding, 6, 11);
  delegate_->SendSurroundingText(5);
  EXPECT_EQ("", after_text());
}

TEST_F(ContextualSearchDelegateTest, ExtractMentionsStartEnd) {
  ListValue mentions_list;
  mentions_list.AppendInteger(1);
  mentions_list.AppendInteger(2);
  int start = 0;
  int end = 0;
  delegate_->ExtractMentionsStartEnd(mentions_list, &start, &end);
  EXPECT_EQ(1, start);
  EXPECT_EQ(2, end);
}

TEST_F(ContextualSearchDelegateTest, SurroundingTextForIcing) {
  base::string16 sample = base::ASCIIToUTF16("this is Barack Obama in office.");
  int limit_each_side = 3;
  size_t start = 8;
  size_t end = 20;
  base::string16 result =
      delegate_->SurroundingTextForIcing(sample, limit_each_side, &start, &end);
  EXPECT_EQ(static_cast<size_t>(3), start);
  EXPECT_EQ(static_cast<size_t>(15), end);
  EXPECT_EQ(base::ASCIIToUTF16("is Barack Obama in"), result);
}

TEST_F(ContextualSearchDelegateTest, SurroundingTextForIcingNegativeLimit) {
  base::string16 sample = base::ASCIIToUTF16("this is Barack Obama in office.");
  int limit_each_side = -2;
  size_t start = 8;
  size_t end = 20;
  base::string16 result =
      delegate_->SurroundingTextForIcing(sample, limit_each_side, &start, &end);
  EXPECT_EQ(static_cast<size_t>(0), start);
  EXPECT_EQ(static_cast<size_t>(12), end);
  EXPECT_EQ(base::ASCIIToUTF16("Barack Obama"), result);
}

TEST_F(ContextualSearchDelegateTest, DecodeSearchTermFromJsonResponse) {
  std::string json_with_escape =
      ")]}'\n"
      "{\"mid\":\"/m/02mjmr\", \"search_term\":\"obama\","
      "\"info_text\":\"44th U.S. President\","
      "\"display_text\":\"Barack Obama\", \"mentions\":[0,15],"
      "\"selected_text\":\"obama\", \"resolved_term\":\"barack obama\"}";
  std::string search_term;
  std::string display_text;
  std::string alternate_term;
  std::string mid;
  std::string prevent_preload;
  int mention_start;
  int mention_end;
  std::string context_language;
  std::string thumbnail_url;
  std::string caption;
  std::string quick_action_uri;
  QuickActionCategory quick_action_category = QUICK_ACTION_CATEGORY_NONE;

  delegate_->DecodeSearchTermFromJsonResponse(
      json_with_escape, &search_term, &display_text, &alternate_term,
      &mid, &prevent_preload, &mention_start, &mention_end, &context_language,
      &thumbnail_url, &caption, &quick_action_uri, &quick_action_category);

  EXPECT_EQ("obama", search_term);
  EXPECT_EQ("Barack Obama", display_text);
  EXPECT_EQ("barack obama", alternate_term);
  EXPECT_EQ("/m/02mjmr", mid);
  EXPECT_EQ("", prevent_preload);
  EXPECT_EQ("", context_language);
  EXPECT_EQ("", thumbnail_url);
  EXPECT_EQ("", caption);
  EXPECT_EQ("", quick_action_uri);
  EXPECT_EQ(QUICK_ACTION_CATEGORY_NONE, quick_action_category);
}

TEST_F(ContextualSearchDelegateTest, ResponseWithLanguage) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  fetcher()->set_response_code(200);
  fetcher()->SetResponseString(
      "{\"mid\":\"/m/02mjmr\",\"search_term\":\"obama\","
      "\"mentions\":[0,15],\"prevent_preload\":\"1\", "
      "\"lang\":\"de\"}");
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_FALSE(is_invalid());
  EXPECT_EQ(200, response_code());
  EXPECT_EQ("obama", search_term());
  EXPECT_EQ("obama", display_text());
  EXPECT_EQ("/m/02mjmr", mid());
  EXPECT_TRUE(do_prevent_preload());
  EXPECT_EQ("de", context_language());
}

TEST_F(ContextualSearchDelegateTest, HeaderContainsBasePageUrl) {
  CreateDefaultSearchContextAndRequestSearchTerm();
  EXPECT_EQ(kSomeSpecificBasePage, getBasePageUrlFromRequest());
}

// Missing all Contextual Cards data.
TEST_F(ContextualSearchDelegateTest, ContextualCardsResponseWithNoData) {
  CreateDefaultSearchWithAdditionalJsonData("");
  EXPECT_EQ("", caption());
  EXPECT_EQ("", thumbnail_url());
}

// Test just the root level caption.
TEST_F(ContextualSearchDelegateTest, ContextualCardsResponseWithCaption) {
  CreateDefaultSearchWithAdditionalJsonData(", |caption|:|aCaption|");
  EXPECT_EQ("aCaption", caption());
  EXPECT_EQ("", thumbnail_url());
}

// Test just the root level thumbnail.
TEST_F(ContextualSearchDelegateTest, ContextualCardsResponseWithThumbnail) {
  CreateDefaultSearchWithAdditionalJsonData(
      ", |thumbnail|:|https://t0.gstatic.com/images?q=tbn:ANd9|");
  EXPECT_EQ("", caption());
  EXPECT_EQ("https://t0.gstatic.com/images?q=tbn:ANd9", thumbnail_url());
}
