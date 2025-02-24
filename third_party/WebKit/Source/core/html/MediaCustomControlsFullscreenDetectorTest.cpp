// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/MediaCustomControlsFullscreenDetector.h"

#include "core/EventTypeNames.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntRect.h"
#include "public/platform/WebMediaPlayer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

struct TestParam {
  String description;
  IntRect targetRect;
  bool expectedResult;
};

}  // anonymous namespace

class MediaCustomControlsFullscreenDetectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_pageHolder = DummyPageHolder::create();
    m_newPageHolder = DummyPageHolder::create();

    m_video = HTMLVideoElement::create(m_pageHolder->document());
    document().body()->appendChild(&videoElement());
  }

  HTMLVideoElement& videoElement() const { return *m_video; }
  MediaCustomControlsFullscreenDetector& fullscreenDetector() const {
    return *videoElement().m_customControlsFullscreenDetector;
  }

  Document& document() const { return m_pageHolder->document(); }
  Document& newDocument() const { return m_newPageHolder->document(); }

  bool checkEventListenerRegistered(EventTarget& target,
                                    const AtomicString& eventType,
                                    EventListener* listener) {
    EventListenerVector* listeners = target.getEventListeners(eventType);
    if (!listeners)
      return false;

    for (const auto& registeredListener : *listeners) {
      if (registeredListener.listener() == listener)
        return true;
    }
    return false;
  }

  static bool computeIsDominantVideo(const IntRect& targetRect,
                                     const IntRect& rootRect,
                                     const IntRect& intersectionRect) {
    return MediaCustomControlsFullscreenDetector::
        computeIsDominantVideoForTests(targetRect, rootRect, intersectionRect);
  }

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  std::unique_ptr<DummyPageHolder> m_newPageHolder;
  Persistent<HTMLVideoElement> m_video;
};

TEST_F(MediaCustomControlsFullscreenDetectorTest, computeIsDominantVideo) {
  // TestWithParam cannot be applied here as IntRect needs the memory allocator
  // to be initialized, but the array of parameters is statically initialized,
  // which is before the memory allocation initialization.
  TestParam testParams[] = {
      {"xCompleteFill", {0, 0, 100, 50}, true},
      {"yCompleteFill", {0, 0, 50, 100}, true},
      {"xyCompleteFill", {0, 0, 100, 100}, true},
      {"xIncompleteFillTooSmall", {0, 0, 84, 50}, false},
      {"yIncompleteFillTooSmall", {0, 0, 50, 84}, false},
      {"xIncompleteFillJustRight", {0, 0, 86, 50}, true},
      {"yIncompleteFillJustRight", {0, 0, 50, 86}, true},
      {"xVisibleProportionTooSmall", {-26, 0, 100, 100}, false},
      {"yVisibleProportionTooSmall", {0, -26, 100, 100}, false},
      {"xVisibleProportionJustRight", {-24, 0, 100, 100}, true},
      {"yVisibleProportionJustRight", {0, -24, 100, 100}, true},
  };

  IntRect rootRect(0, 0, 100, 100);

  for (const TestParam& testParam : testParams) {
    const IntRect& targetRect = testParam.targetRect;
    IntRect intersectionRect = intersection(targetRect, rootRect);
    EXPECT_EQ(testParam.expectedResult,
              computeIsDominantVideo(targetRect, rootRect, intersectionRect))
        << testParam.description << " failed";
  }
}

TEST_F(MediaCustomControlsFullscreenDetectorTest, documentMove) {
  EXPECT_TRUE(checkEventListenerRegistered(
      document(), EventTypeNames::fullscreenchange, &fullscreenDetector()));
  EXPECT_TRUE(checkEventListenerRegistered(
      document(), EventTypeNames::webkitfullscreenchange,
      &fullscreenDetector()));
  EXPECT_FALSE(checkEventListenerRegistered(
      newDocument(), EventTypeNames::fullscreenchange, &fullscreenDetector()));
  EXPECT_FALSE(checkEventListenerRegistered(
      newDocument(), EventTypeNames::webkitfullscreenchange,
      &fullscreenDetector()));

  newDocument().body()->appendChild(&videoElement());

  EXPECT_FALSE(checkEventListenerRegistered(
      document(), EventTypeNames::fullscreenchange, &fullscreenDetector()));
  EXPECT_FALSE(checkEventListenerRegistered(
      document(), EventTypeNames::webkitfullscreenchange,
      &fullscreenDetector()));
  EXPECT_TRUE(checkEventListenerRegistered(
      newDocument(), EventTypeNames::fullscreenchange, &fullscreenDetector()));
  EXPECT_TRUE(checkEventListenerRegistered(
      newDocument(), EventTypeNames::webkitfullscreenchange,
      &fullscreenDetector()));
}

}  // namespace blink
