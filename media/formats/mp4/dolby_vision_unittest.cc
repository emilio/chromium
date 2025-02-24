// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/dolby_vision.h"

#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

class DolbyVisionConfigurationTest : public testing::Test {};

TEST_F(DolbyVisionConfigurationTest, Profile0Level1ELTrackTest) {
  DolbyVisionConfiguration dv_config;
  uint8_t data[] = {0x00, 0x00, 0x00, 0x0E};

  dv_config.Parse(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 0);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE0);
  EXPECT_EQ(dv_config.dv_level, 1);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 1);
  EXPECT_EQ(dv_config.bl_present_flag, 0);
}

TEST_F(DolbyVisionConfigurationTest, Profile4Level2ELTrackWithBLTest) {
  DolbyVisionConfiguration dv_config;
  uint8_t data[] = {0x00, 0x00, 0x08, 0x16};

  dv_config.Parse(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 0);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE4);
  EXPECT_EQ(dv_config.dv_level, 2);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 1);
  EXPECT_EQ(dv_config.bl_present_flag, 0);
}

TEST_F(DolbyVisionConfigurationTest, Profile5Test) {
  DolbyVisionConfiguration dv_config;
  uint8_t data[] = {0x00, 0x00, 0x0A, 0x17};

  dv_config.Parse(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 0);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE5);
  EXPECT_EQ(dv_config.dv_level, 2);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 1);
  EXPECT_EQ(dv_config.bl_present_flag, 1);
}

TEST_F(DolbyVisionConfigurationTest, ParseNotEnoughData) {
  DolbyVisionConfiguration dv_config;
  uint8_t data[] = {0x00, 0x00, 0x0C};

  EXPECT_FALSE(dv_config.Parse(data, sizeof(data)));
}

}  // namespace mp4
}  // namespace media
