// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_controller.h"

#include <unordered_set>

#import "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using favicon::PostReply;
using testing::_;

#pragma mark - ReadingListCollectionViewControllerTest

class ReadingListCollectionViewControllerTest : public testing::Test {
 public:
  ReadingListCollectionViewControllerTest() {}
  ~ReadingListCollectionViewControllerTest() override {}

  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  std::unique_ptr<favicon::LargeIconService> large_icon_service_;

  base::scoped_nsobject<ReadingListCollectionViewController>
      reading_list_view_controller_;
  id mock_delegate_;

  // TODO(crbug.com/625617) When offline url can be opened, use a mock for the
  // readinglistdownloadservice.
  void SetUp() override {
    testing::Test::SetUp();

    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillRepeatedly(PostReply<5>(favicon_base::FaviconRawBitmapResult()));

    reading_list_model_.reset(new ReadingListModelImpl(nullptr, nullptr));
    large_icon_service_.reset(new favicon::LargeIconService(
        &mock_favicon_service_, base::ThreadTaskRunnerHandle::Get()));
    reading_list_view_controller_.reset(
        [[ReadingListCollectionViewController alloc]
                         initWithModel:reading_list_model_.get()
                      largeIconService:large_icon_service_.get()
            readingListDownloadService:nil
                               toolbar:nil]);

    mock_delegate_ = [OCMockObject
        niceMockForProtocol:@protocol(
                                ReadingListCollectionViewControllerDelegate)];
    [reading_list_view_controller_ setDelegate:mock_delegate_];
  }

 private:
  web::TestWebThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(ReadingListCollectionViewControllerTest);
};

// Tests that reading list items are displayed.
TEST_F(ReadingListCollectionViewControllerTest, DisplaysItems) {
  // Prefill some items.
  reading_list_model_->AddEntry(GURL("https://chromium.org"), "news",
                                reading_list::ADDED_VIA_CURRENT_APP);
  reading_list_model_->AddEntry(GURL("https://mail.chromium.org"), "mail",
                                reading_list::ADDED_VIA_CURRENT_APP);
  reading_list_model_->AddEntry(GURL("https://foo.bar"), "Foo",
                                reading_list::ADDED_VIA_CURRENT_APP);
  reading_list_model_->SetReadStatus(GURL("https://foo.bar"), true);

  // Load view.
  [reading_list_view_controller_ view];

  // There are two sections: Read and Unread.
  DCHECK(
      [reading_list_view_controller_.get().collectionView numberOfSections] ==
      2);
  // There are two unread articles.
  DCHECK([reading_list_view_controller_.get().collectionView
             numberOfItemsInSection:0] == 2);
  // There is one read article.
  DCHECK([reading_list_view_controller_.get().collectionView
             numberOfItemsInSection:1] == 1);
}

// Tests that the view controller is dismissed when Done button is pressed.
TEST_F(ReadingListCollectionViewControllerTest, GetsDismissed) {
  // Load view.
  [reading_list_view_controller_ view];

  [[mock_delegate_ expect]
      dismissReadingListCollectionViewController:reading_list_view_controller_
                                                     .get()];

  // Simulate tap on "Done" button.
  UIBarButtonItem* done =
      reading_list_view_controller_.get().navigationItem.rightBarButtonItem;
  [done.target performSelector:done.action];

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that when an item is selected, the article is opened with UrlLoader and
// the view controller is dismissed.
TEST_F(ReadingListCollectionViewControllerTest, OpensItems) {
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:1 inSection:0];

  GURL url("https://chromium.org");
  GURL url2("https://chromium.org/2");
  reading_list_model_->AddEntry(url, "chromium",
                                reading_list::ADDED_VIA_CURRENT_APP);
  reading_list_model_->AddEntry(url2, "chromium - 2",
                                reading_list::ADDED_VIA_CURRENT_APP);

  ReadingListCollectionViewItem* readingListItem =
      base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(
          [[reading_list_view_controller_ collectionViewModel]
              itemAtIndexPath:indexPath]);

  [[mock_delegate_ expect]
      readingListCollectionViewController:reading_list_view_controller_.get()
                                 openItem:readingListItem];

  // Simulate touch on second cell.
  [reading_list_view_controller_
                collectionView:reading_list_view_controller_.get()
                                   .collectionView
      didSelectItemAtIndexPath:indexPath];

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that the ReadingListCollectionView is creating
// ReadingListCollectionViewItem with the correct informations.
TEST_F(ReadingListCollectionViewControllerTest,
       TestItemInitializationUndistilled) {
  // Setup.
  GURL url("https://chromium.org");
  std::string title("Chromium");
  reading_list_model_->AddEntry(url, title,
                                reading_list::ADDED_VIA_CURRENT_APP);
  // Load view.
  [reading_list_view_controller_ view];
  DCHECK([reading_list_view_controller_.get().collectionView
             numberOfItemsInSection:0] == 1);
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  ReadingListCollectionViewItem* readingListItem =
      base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(
          [[reading_list_view_controller_ collectionViewModel]
              itemAtIndexPath:indexPath]);
  EXPECT_EQ(base::SysNSStringToUTF8([readingListItem text]), title);
  EXPECT_EQ([readingListItem url], url);
  EXPECT_EQ(base::SysNSStringToUTF16([readingListItem detailText]),
            url_formatter::FormatUrl(url));
  EXPECT_EQ([readingListItem faviconPageURL], url);
  EXPECT_EQ([readingListItem distillationState], ReadingListEntry::WAITING);
}

// Tests that the ReadingListCollectionView is creating
// ReadingListCollectionViewItem with the correct informations for distilled
// items.
TEST_F(ReadingListCollectionViewControllerTest,
       TestItemInitializationDistilled) {
  // Setup.
  GURL url("https://chromium.org");
  std::string title("Chromium");
  GURL distilled_url("https://chromium.org/distilled");
  base::FilePath distilled_path("/distilled/path");
  reading_list_model_->AddEntry(url, title,
                                reading_list::ADDED_VIA_CURRENT_APP);
  int64_t size = 50;
  reading_list_model_->SetEntryDistilledInfo(url, distilled_path, distilled_url,
                                             size, 100);
  // Load view.
  [reading_list_view_controller_ view];
  DCHECK([reading_list_view_controller_.get().collectionView
             numberOfItemsInSection:0] == 1);
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  ReadingListCollectionViewItem* readingListItem =
      base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(
          [[reading_list_view_controller_ collectionViewModel]
              itemAtIndexPath:indexPath]);
  EXPECT_EQ(base::SysNSStringToUTF8([readingListItem text]), title);
  EXPECT_EQ([readingListItem url], url);
  EXPECT_EQ(base::SysNSStringToUTF16([readingListItem detailText]),
            url_formatter::FormatUrl(url));
  EXPECT_EQ([readingListItem faviconPageURL], distilled_url);
  EXPECT_EQ([readingListItem distillationState], ReadingListEntry::PROCESSED);
}
