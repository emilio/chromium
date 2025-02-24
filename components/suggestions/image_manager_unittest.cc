// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/image_manager.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/suggestions/image_encoder.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

using image_fetcher::ImageFetcher;
using image_fetcher::ImageFetcherDelegate;

namespace suggestions {

const char kTestUrl1[] = "http://go.com/";
const char kTestUrl2[] = "http://goal.com/";
const char kTestImagePath[] = "files/image_decoding/droids.png";
const char kInvalidImagePath[] = "files/DOESNOTEXIST";

using leveldb_proto::test::FakeDB;

typedef base::hash_map<std::string, ImageData> EntryMap;

void AddEntry(const ImageData& d, EntryMap* map) { (*map)[d.url()] = d; }

class MockImageFetcher : public ImageFetcher {
 public:
  MockImageFetcher() {}
  virtual ~MockImageFetcher() {}
  MOCK_METHOD3(StartOrQueueNetworkRequest,
               void(const std::string&, const GURL&,
                    base::Callback<void(const std::string&,
                                        const gfx::Image&)>));
  MOCK_METHOD1(SetImageFetcherDelegate, void(ImageFetcherDelegate*));
  MOCK_METHOD1(SetDataUseServiceName, void(DataUseServiceName));
  MOCK_METHOD1(SetDesiredImageFrameSize, void(const gfx::Size&));
  MOCK_METHOD0(GetImageDecoder, image_fetcher::ImageDecoder*());
};

class ImageManagerTest : public testing::Test {
 public:
  ImageManagerTest()
      : mock_image_fetcher_(NULL),
        num_callback_null_called_(0),
        num_callback_valid_called_(0) {}

  void SetUp() override {
    fake_db_ = new FakeDB<ImageData>(&db_model_);
    image_manager_.reset(CreateImageManager(fake_db_));
  }

  void TearDown() override {
    fake_db_ = NULL;
    db_model_.clear();
    image_manager_.reset();
  }

  void InitializeDefaultImageMapAndDatabase(ImageManager* image_manager,
                                            FakeDB<ImageData>* fake_db) {
    CHECK(image_manager);
    CHECK(fake_db);

    suggestions::SuggestionsProfile suggestions_profile;
    suggestions::ChromeSuggestion* suggestion =
        suggestions_profile.add_suggestions();
    suggestion->set_url(kTestUrl1);
    suggestion->set_thumbnail(kTestImagePath);

    image_manager->Initialize(suggestions_profile);

    // Initialize empty database.
    fake_db->InitCallback(true);
    fake_db->LoadCallback(true);
  }

  ImageData GetSampleImageData(const std::string& url) {
    // Create test bitmap.
    SkBitmap bm;
    // Being careful with the Bitmap. There are memory-related issue in
    // crbug.com/101781.
    bm.allocN32Pixels(4, 4);
    bm.eraseColor(SK_ColorRED);
    ImageData data;
    data.set_url(url);
    std::vector<unsigned char> encoded;
    EXPECT_TRUE(EncodeSkBitmapToJPEG(bm, &encoded));
    data.set_data(std::string(encoded.begin(), encoded.end()));
    return data;
  }

  void OnImageAvailable(base::RunLoop* loop, const GURL& url,
                        const gfx::Image& image) {
    if (!image.IsEmpty()) {
      num_callback_valid_called_++;
    } else {
      num_callback_null_called_++;
    }
    loop->Quit();
  }

  ImageManager* CreateImageManager(FakeDB<ImageData>* fake_db) {
    mock_image_fetcher_ = new StrictMock<MockImageFetcher>();
    EXPECT_CALL(*mock_image_fetcher_, SetImageFetcherDelegate(_));
    return new ImageManager(base::WrapUnique(mock_image_fetcher_),
                            base::WrapUnique(fake_db),
                            FakeDB<ImageData>::DirectoryForTestDB(),
                            base::ThreadTaskRunnerHandle::Get());
  }

  EntryMap db_model_;
  // Owned by the ImageManager under test.
  FakeDB<ImageData>* fake_db_;

  MockImageFetcher* mock_image_fetcher_;

  int num_callback_null_called_;
  int num_callback_valid_called_;

  base::MessageLoop message_loop_;

  // Under test.
  std::unique_ptr<ImageManager> image_manager_;
};

TEST_F(ImageManagerTest, InitializeTest) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  suggestion->set_thumbnail(kTestImagePath);

  image_manager_->Initialize(suggestions_profile);

  GURL output;
  EXPECT_TRUE(image_manager_->GetImageURL(GURL(kTestUrl1), &output));
  EXPECT_EQ(GURL(kTestImagePath), output);

  EXPECT_FALSE(image_manager_->GetImageURL(GURL("http://b.com"), &output));
}

TEST_F(ImageManagerTest, AddImageURL) {
  InitializeDefaultImageMapAndDatabase(image_manager_.get(), fake_db_);

  GURL output;
  // Try a URL the ImageManager doesn't know about.
  ASSERT_FALSE(image_manager_->GetImageURL(GURL(kTestUrl2), &output));

  // Explicitly add the URL and try again.
  image_manager_->AddImageURL(GURL(kTestUrl2), GURL(kTestImagePath));
  EXPECT_TRUE(image_manager_->GetImageURL(GURL(kTestUrl2), &output));
  EXPECT_EQ(GURL(kTestImagePath), output);
}

TEST_F(ImageManagerTest, GetImageForURLNetwork) {
  InitializeDefaultImageMapAndDatabase(image_manager_.get(), fake_db_);

  // We expect the fetcher to go to network and call the callback.
  EXPECT_CALL(*mock_image_fetcher_, StartOrQueueNetworkRequest(_, _, _));

  // Fetch existing URL.
  base::RunLoop run_loop;
  image_manager_->GetImageForURL(GURL(kTestUrl1),
                                 base::Bind(&ImageManagerTest::OnImageAvailable,
                                            base::Unretained(this), &run_loop));

  // Will not go to network and use the fetcher since URL is invalid.
  // Fetch non-existing URL.
  image_manager_->GetImageForURL(GURL(kTestUrl2),
                                 base::Bind(&ImageManagerTest::OnImageAvailable,
                                            base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(1, num_callback_null_called_);
}

TEST_F(ImageManagerTest, GetImageForURLNetworkCacheHit) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  // The URL we set is invalid, to show that it will fail from network.
  suggestion->set_thumbnail(kInvalidImagePath);

  // Create the ImageManager with an added entry in the database.
  AddEntry(GetSampleImageData(kTestUrl1), &db_model_);
  FakeDB<ImageData>* fake_db = new FakeDB<ImageData>(&db_model_);
  image_manager_.reset(CreateImageManager(fake_db));
  image_manager_->Initialize(suggestions_profile);
  fake_db->InitCallback(true);
  fake_db->LoadCallback(true);
  // Expect something in the cache.
  auto encoded_image =
      image_manager_->GetEncodedImageFromCache(GURL(kTestUrl1));
  EXPECT_TRUE(encoded_image);

  base::RunLoop run_loop;
  image_manager_->GetImageForURL(GURL(kTestUrl1),
                                 base::Bind(&ImageManagerTest::OnImageAvailable,
                                            base::Unretained(this), &run_loop));
  run_loop.Run();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);
}

}  // namespace suggestions
