// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/mime_util.h"
#include "media/base/mime_util_internal.h"
#include "media/media_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace media {
namespace internal {

// MIME type for use with IsCodecSupportedOnAndroid() test; type is ignored in
// all cases except for when paired with the Opus codec.
const char kTestMimeType[] = "foo/foo";

// Helper method for creating a multi-value vector of |kTestStates| if
// |test_all_values| is true or if false, a single value vector containing
// |single_value|.
static std::vector<bool> CreateTestVector(bool test_all_values,
                                          bool single_value) {
  const bool kTestStates[] = {true, false};
  if (test_all_values)
    return std::vector<bool>(kTestStates, kTestStates + arraysize(kTestStates));
  return std::vector<bool>(1, single_value);
}

// Helper method for running IsCodecSupportedOnAndroid() tests that will
// iterate over all possible field values for a MimeUtil::PlatformInfo struct.
//
// To request a field be varied, set its value to true in the |states_to_vary|
// struct.  If false, the only value tested will be the field value from
// |test_states|.
//
// |test_func| should have the signature <void(const MimeUtil::PlatformInfo&,
// MimeUtil::Codec)>.
template <typename TestCallback>
static void RunCodecSupportTest(const MimeUtil::PlatformInfo& states_to_vary,
                                const MimeUtil::PlatformInfo& test_states,
                                TestCallback test_func) {
#define MAKE_TEST_VECTOR(name)      \
  std::vector<bool> name##_states = \
      CreateTestVector(states_to_vary.name, test_states.name)

  // Stuff states to test into vectors for easy for_each() iteration.
  MAKE_TEST_VECTOR(has_platform_decoders);
  MAKE_TEST_VECTOR(has_platform_vp8_decoder);
  MAKE_TEST_VECTOR(has_platform_vp9_decoder);
  MAKE_TEST_VECTOR(supports_opus);
#undef MAKE_TEST_VECTOR

  MimeUtil::PlatformInfo info;

#define RUN_TEST_VECTOR(name)                   \
  size_t name##_index = 0;                      \
  for (info.name = name##_states[name##_index]; \
       name##_index < name##_states.size(); ++name##_index)

  RUN_TEST_VECTOR(has_platform_decoders) {
    RUN_TEST_VECTOR(has_platform_vp8_decoder) {
      RUN_TEST_VECTOR(has_platform_vp9_decoder) {
        RUN_TEST_VECTOR(supports_opus) {
          for (int codec = MimeUtil::INVALID_CODEC;
               codec <= MimeUtil::LAST_CODEC; ++codec) {
            SCOPED_TRACE(base::StringPrintf(
                "has_platform_decoders=%d, has_platform_vp8_decoder=%d, "
                "supports_opus=%d, "
                "has_platform_vp9_decoder=%d, "
                "codec=%d",
                info.has_platform_decoders, info.has_platform_vp8_decoder,
                info.supports_opus, info.has_platform_vp9_decoder, codec));
            test_func(info, static_cast<MimeUtil::Codec>(codec));
          }
        }
      }
    }
  }
#undef RUN_TEST_VECTOR
}

// Helper method for generating the |states_to_vary| value used by
// RunPlatformCodecTest(). Marks all fields to be varied.
static MimeUtil::PlatformInfo VaryAllFields() {
  MimeUtil::PlatformInfo states_to_vary;
  states_to_vary.has_platform_vp8_decoder = true;
  states_to_vary.has_platform_vp9_decoder = true;
  states_to_vary.supports_opus = true;
  states_to_vary.has_platform_decoders = true;
  return states_to_vary;
}

static bool HasHevcSupport() {
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
#if defined(OS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 21;
#else
  return true;
#endif  // defined(OS_ANDROID)
#else
  return false;
#endif  // BUILDFLAG(ENABLE_HEVC_DEMUXING)
}

// This is to validate MimeUtil::IsCodecSupportedOnPlatform(), which is used
// only on Android platform.
static bool HasDolbyVisionSupport() {
  return false;
}

TEST(MimeUtilTest, CommonMediaMimeType) {
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/webm"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/webm"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/wav"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-wav"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/flac"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/ogg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("application/ogg"));
#if defined(OS_ANDROID)
  EXPECT_FALSE(IsSupportedMediaMimeType("video/ogg"));
#else
  EXPECT_TRUE(IsSupportedMediaMimeType("video/ogg"));
#endif  // OS_ANDROID

#if defined(OS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
  // HLS is supported on Android API level 14 and higher and Chrome supports
  // API levels 15 and higher, so these are expected to be supported.
  bool kHlsSupported = true;
#else
  bool kHlsSupported = false;
#endif

  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("Application/X-MPEGURL"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType(
      "application/vnd.apple.mpegurl"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("audio/mpegurl"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("audio/x-mpegurl"));

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp4"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/mp4"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/x-m4v"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mpeg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/aac"));

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  EXPECT_TRUE(IsSupportedMediaMimeType("video/mp2t"));
#else
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp2t"));
#endif
#else
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mp4"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp4"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/x-m4v"));

  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mp3"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/x-mp3"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mpeg"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/aac"));
#endif  // USE_PROPRIETARY_CODECS
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp3"));

  EXPECT_FALSE(IsSupportedMediaMimeType("video/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("unknown/unknown"));
}

// Note: codecs should only be a list of 2 or fewer; hence the restriction of
// results' length to 2.
TEST(MimeUtilTest, SplitCodecsToVector) {
  const struct {
    const char* const original;
    size_t expected_size;
    const char* const results[2];
  } tests[] = {
    { "\"bogus\"",                  1, { "bogus" }            },
    { "0",                          1, { "0" }                },
    { "avc1.42E01E, mp4a.40.2",     2, { "avc1",   "mp4a" }   },
    { "\"mp4v.20.240, mp4a.40.2\"", 2, { "mp4v",   "mp4a" }   },
    { "mp4v.20.8, samr",            2, { "mp4v",   "samr" }   },
    { "\"theora, vorbis\"",         2, { "theora", "vorbis" } },
    { "",                           0, { }                    },
    { "\"\"",                       0, { }                    },
    { "\"   \"",                    0, { }                    },
    { ",",                          2, { "", "" }             },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::vector<std::string> codecs_out;
    SplitCodecsToVector(tests[i].original, &codecs_out, true);
    ASSERT_EQ(tests[i].expected_size, codecs_out.size());
    for (size_t j = 0; j < tests[i].expected_size; ++j)
      EXPECT_EQ(tests[i].results[j], codecs_out[j]);
  }

  // Test without stripping the codec type.
  std::vector<std::string> codecs_out;
  SplitCodecsToVector("avc1.42E01E, mp4a.40.2", &codecs_out, false);
  ASSERT_EQ(2u, codecs_out.size());
  EXPECT_EQ("avc1.42E01E", codecs_out[0]);
  EXPECT_EQ("mp4a.40.2", codecs_out[1]);
}

// See deeper string parsing testing in video_codecs_unittests.cc.
TEST(MimeUtilTest, ExperimentalMultiPartVp9) {
  base::test::ScopedCommandLine scoped_command_line;

  // Multi-part VP9 string not enabled by default.
  EXPECT_FALSE(IsSupportedMediaFormat("video/webm", {"vp09.00.01.08"}));

  // Should work if enabled.
  EnableNewVp9CodecStringSupport();
  EXPECT_TRUE(IsSupportedMediaFormat("video/webm", {"vp09.00.01.08"}));
}

TEST(IsCodecSupportedOnAndroidTest, EncryptedCodecsFailWithoutPlatformSupport) {
  // Vary all parameters except |has_platform_decoders|.
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();
  states_to_vary.has_platform_decoders = false;

  // Disable platform decoders.
  MimeUtil::PlatformInfo test_states;
  test_states.has_platform_decoders = false;

  // Every codec should fail since platform support is missing and we've
  // requested encrypted codecs.
  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        EXPECT_FALSE(MimeUtil::IsCodecSupportedOnAndroid(codec, kTestMimeType,
                                                         true, info));
      });
}

TEST(IsCodecSupportedOnAndroidTest, EncryptedCodecBehavior) {
  // Vary all parameters except |has_platform_decoders|.
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();
  states_to_vary.has_platform_decoders = false;

  // Enable platform decoders.
  MimeUtil::PlatformInfo test_states;
  test_states.has_platform_decoders = true;

  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        const bool result = MimeUtil::IsCodecSupportedOnAndroid(
            codec, kTestMimeType, true, info);
        switch (codec) {
          // These codecs are never supported by the Android platform.
          case MimeUtil::INVALID_CODEC:
          case MimeUtil::AC3:
          case MimeUtil::EAC3:
          case MimeUtil::MPEG2_AAC:
          case MimeUtil::THEORA:
            EXPECT_FALSE(result);
            break;

          // These codecs are always available with platform decoder support.
          case MimeUtil::PCM:
          case MimeUtil::MP3:
          case MimeUtil::MPEG4_AAC:
          case MimeUtil::VORBIS:
          case MimeUtil::FLAC:
          case MimeUtil::H264:
            EXPECT_TRUE(result);
            break;

          // The remaining codecs are not available on all platforms even when
          // a platform decoder is available.
          case MimeUtil::OPUS:
            EXPECT_EQ(info.supports_opus, result);
            break;

          case MimeUtil::VP8:
            EXPECT_EQ(info.has_platform_vp8_decoder, result);
            break;

          case MimeUtil::VP9:
            EXPECT_EQ(info.has_platform_vp9_decoder, result);
            break;

          case MimeUtil::HEVC:
            EXPECT_EQ(HasHevcSupport(), result);
            break;

          case MimeUtil::DOLBY_VISION:
            EXPECT_EQ(HasDolbyVisionSupport(), result);
            break;
        }
      });
}

TEST(IsCodecSupportedOnAndroidTest, ClearCodecBehavior) {
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();

  MimeUtil::PlatformInfo test_states;

  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        const bool result = MimeUtil::IsCodecSupportedOnAndroid(
            codec, kTestMimeType, false, info);
        switch (codec) {
          // These codecs are never supported by the Android platform.
          case MimeUtil::INVALID_CODEC:
          case MimeUtil::AC3:
          case MimeUtil::EAC3:
          case MimeUtil::THEORA:
            EXPECT_FALSE(result);
            break;

          // These codecs are always supported with the unified pipeline.
          case MimeUtil::FLAC:
          case MimeUtil::PCM:
          case MimeUtil::MPEG2_AAC:
          case MimeUtil::MP3:
          case MimeUtil::MPEG4_AAC:
          case MimeUtil::OPUS:
          case MimeUtil::VORBIS:
          case MimeUtil::VP8:
          case MimeUtil::VP9:
            EXPECT_TRUE(result);
            break;

          // These codecs are only supported if platform decoders are supported.
          case MimeUtil::H264:
            EXPECT_EQ(info.has_platform_decoders, result);
            break;

          case MimeUtil::HEVC:
            EXPECT_EQ(HasHevcSupport() && info.has_platform_decoders, result);
            break;

          case MimeUtil::DOLBY_VISION:
            EXPECT_EQ(HasDolbyVisionSupport(), result);
            break;
        }
      });
}

TEST(IsCodecSupportedOnAndroidTest, OpusOggSupport) {
  // Vary all parameters; thus use default initial state.
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();
  MimeUtil::PlatformInfo test_states;

  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        EXPECT_TRUE(MimeUtil::IsCodecSupportedOnAndroid(
            MimeUtil::OPUS, "audio/ogg", false, info));
      });
}

TEST(IsCodecSupportedOnAndroidTest, HLSDoesNotSupportMPEG2AAC) {
  // Vary all parameters; thus use default initial state.
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();
  MimeUtil::PlatformInfo test_states;

  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        EXPECT_FALSE(MimeUtil::IsCodecSupportedOnAndroid(
            MimeUtil::MPEG2_AAC, "application/x-mpegurl", false, info));
        EXPECT_FALSE(MimeUtil::IsCodecSupportedOnAndroid(
            MimeUtil::MPEG2_AAC, "application/vnd.apple.mpegurl", false, info));
        EXPECT_FALSE(MimeUtil::IsCodecSupportedOnAndroid(
            MimeUtil::MPEG2_AAC, "audio/mpegurl", false, info));
        EXPECT_FALSE(MimeUtil::IsCodecSupportedOnAndroid(
            MimeUtil::MPEG2_AAC, "audio/x-mpegurl", false, info));
      });
}

}  // namespace internal
}  // namespace media
