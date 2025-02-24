// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_config_adapter.h"

#include "base/logging.h"
#include "chromecast/media/base/media_codec_support.h"
#include "media/base/channel_layout.h"

namespace chromecast {
namespace media {

namespace {

// Converts ::media::AudioCodec to chromecast::media::AudioCodec. Any unknown or
// unsupported codec will be converted to chromecast::media::kCodecUnknown.
AudioCodec ToAudioCodec(const ::media::AudioCodec audio_codec) {
  switch (audio_codec) {
    case ::media::kCodecAAC:
      return kCodecAAC;
    case ::media::kCodecMP3:
      return kCodecMP3;
    case ::media::kCodecPCM:
      return kCodecPCM;
    case ::media::kCodecPCM_S16BE:
      return kCodecPCM_S16BE;
    case ::media::kCodecVorbis:
      return kCodecVorbis;
    case ::media::kCodecOpus:
      return kCodecOpus;
    case ::media::kCodecFLAC:
      return kCodecFLAC;
    case ::media::kCodecEAC3:
      return kCodecEAC3;
    case ::media::kCodecAC3:
      return kCodecAC3;
    default:
      LOG(ERROR) << "Unsupported audio codec " << audio_codec;
  }
  return kAudioCodecUnknown;
}

SampleFormat ToSampleFormat(const ::media::SampleFormat sample_format) {
  switch (sample_format) {
    case ::media::kUnknownSampleFormat:
    case ::media::kSampleFormatAc3:
    case ::media::kSampleFormatEac3:
      return kUnknownSampleFormat;
    case ::media::kSampleFormatU8:
      return kSampleFormatU8;
    case ::media::kSampleFormatS16:
      return kSampleFormatS16;
    case ::media::kSampleFormatS24:
      return kSampleFormatS24;
    case ::media::kSampleFormatS32:
      return kSampleFormatS32;
    case ::media::kSampleFormatF32:
      return kSampleFormatF32;
    case ::media::kSampleFormatPlanarS16:
      return kSampleFormatPlanarS16;
    case ::media::kSampleFormatPlanarF32:
      return kSampleFormatPlanarF32;
    case ::media::kSampleFormatPlanarS32:
      return kSampleFormatPlanarS32;
  }
  NOTREACHED();
  return kUnknownSampleFormat;
}

::media::ChannelLayout ToMediaChannelLayout(int channel_number) {
  switch (channel_number) {
    case 1:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_MONO;
    case 2:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
    default:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

::media::SampleFormat ToMediaSampleFormat(const SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return ::media::kUnknownSampleFormat;
    case kSampleFormatU8:
      return ::media::kSampleFormatU8;
    case kSampleFormatS16:
      return ::media::kSampleFormatS16;
    case kSampleFormatS24:
      return ::media::kSampleFormatS24;
    case kSampleFormatS32:
      return ::media::kSampleFormatS32;
    case kSampleFormatF32:
      return ::media::kSampleFormatF32;
    case kSampleFormatPlanarS16:
      return ::media::kSampleFormatPlanarS16;
    case kSampleFormatPlanarF32:
      return ::media::kSampleFormatPlanarF32;
    case kSampleFormatPlanarS32:
      return ::media::kSampleFormatPlanarS32;
    default:
      NOTREACHED();
      return ::media::kUnknownSampleFormat;
  }
}

::media::AudioCodec ToMediaAudioCodec(
    const chromecast::media::AudioCodec codec) {
  switch (codec) {
    case kAudioCodecUnknown:
      return ::media::kUnknownAudioCodec;
    case kCodecAAC:
      return ::media::kCodecAAC;
    case kCodecMP3:
      return ::media::kCodecMP3;
    case kCodecPCM:
      return ::media::kCodecPCM;
    case kCodecPCM_S16BE:
      return ::media::kCodecPCM_S16BE;
    case kCodecVorbis:
      return ::media::kCodecVorbis;
    case kCodecOpus:
      return ::media::kCodecOpus;
    case kCodecFLAC:
      return ::media::kCodecFLAC;
    case kCodecEAC3:
      return ::media::kCodecEAC3;
    case kCodecAC3:
      return ::media::kCodecAC3;
    default:
      return ::media::kUnknownAudioCodec;
  }
}

::media::EncryptionScheme::CipherMode ToMediaCipherMode(
    EncryptionScheme::CipherMode mode) {
  switch (mode) {
    case EncryptionScheme::CIPHER_MODE_UNENCRYPTED:
      return ::media::EncryptionScheme::CIPHER_MODE_UNENCRYPTED;
    case EncryptionScheme::CIPHER_MODE_AES_CTR:
      return ::media::EncryptionScheme::CIPHER_MODE_AES_CTR;
    case EncryptionScheme::CIPHER_MODE_AES_CBC:
      return ::media::EncryptionScheme::CIPHER_MODE_AES_CBC;
    default:
      NOTREACHED();
      return ::media::EncryptionScheme::CIPHER_MODE_UNENCRYPTED;
  }
}

EncryptionScheme::CipherMode ToCipherMode(
    ::media::EncryptionScheme::CipherMode mode) {
  switch (mode) {
    case ::media::EncryptionScheme::CIPHER_MODE_UNENCRYPTED:
      return EncryptionScheme::CIPHER_MODE_UNENCRYPTED;
    case ::media::EncryptionScheme::CIPHER_MODE_AES_CTR:
      return EncryptionScheme::CIPHER_MODE_AES_CTR;
    case ::media::EncryptionScheme::CIPHER_MODE_AES_CBC:
      return EncryptionScheme::CIPHER_MODE_AES_CBC;
    default:
      NOTREACHED();
      return EncryptionScheme::CIPHER_MODE_UNENCRYPTED;
  }
}

EncryptionScheme::Pattern ToPatternSpec(
    const ::media::EncryptionScheme::Pattern& pattern) {
  return EncryptionScheme::Pattern(
      pattern.encrypt_blocks(), pattern.skip_blocks());
}

::media::EncryptionScheme::Pattern ToMediaPatternSpec(
    const EncryptionScheme::Pattern& pattern) {
  return ::media::EncryptionScheme::Pattern(
      pattern.encrypt_blocks, pattern.skip_blocks);
}

EncryptionScheme ToEncryptionScheme(
    const ::media::EncryptionScheme& scheme) {
  return EncryptionScheme(
    ToCipherMode(scheme.mode()),
    ToPatternSpec(scheme.pattern()));
}

::media::EncryptionScheme ToMediaEncryptionScheme(
    const EncryptionScheme& scheme) {
  return ::media::EncryptionScheme(
    ToMediaCipherMode(scheme.mode),
    ToMediaPatternSpec(scheme.pattern));
}

}  // namespace

// static
AudioConfig DecoderConfigAdapter::ToCastAudioConfig(
    StreamId id,
    const ::media::AudioDecoderConfig& config) {
  AudioConfig audio_config;
  if (!config.IsValidConfig())
    return audio_config;

  audio_config.id = id;
  audio_config.codec = ToAudioCodec(config.codec());
  audio_config.sample_format = ToSampleFormat(config.sample_format());
  audio_config.bytes_per_channel = config.bytes_per_channel();
  audio_config.channel_number =
      ::media::ChannelLayoutToChannelCount(config.channel_layout()),
  audio_config.samples_per_second = config.samples_per_second();
  audio_config.extra_data = config.extra_data();
  audio_config.encryption_scheme = ToEncryptionScheme(
      config.encryption_scheme());
  return audio_config;
}

// static
::media::AudioDecoderConfig DecoderConfigAdapter::ToMediaAudioDecoderConfig(
    const AudioConfig& config) {
  return ::media::AudioDecoderConfig(
      ToMediaAudioCodec(config.codec),
      ToMediaSampleFormat(config.sample_format),
      ToMediaChannelLayout(config.channel_number), config.samples_per_second,
      config.extra_data,
      ToMediaEncryptionScheme(config.encryption_scheme));
}

// static
VideoConfig DecoderConfigAdapter::ToCastVideoConfig(
    StreamId id,
    const ::media::VideoDecoderConfig& config) {
  VideoConfig video_config;
  if (!config.IsValidConfig()) {
    return video_config;
  }

  video_config.id = id;
  video_config.codec = ToCastVideoCodec(config.codec(), config.profile());
  video_config.profile = ToCastVideoProfile(config.profile());
  video_config.extra_data = config.extra_data();
  video_config.encryption_scheme = ToEncryptionScheme(
      config.encryption_scheme());

  // TODO(servolk): gfx::ColorSpace currently doesn't provide getters for color
  // space components. We'll need to way to fix this. crbug.com/649758
  // video_config.primaries =
  //     static_cast<PrimaryID>(config.color_space_info().primaries());
  // video_config.transfer =
  //     static_cast<TransferID>(config.color_space_info().transfer());
  // video_config.matrix =
  //     static_cast<MatrixID>(config.color_space_info().matrix());
  // video_config.range =
  //     static_cast<RangeID>(config.color_space_info().range());

  base::Optional<::media::HDRMetadata> hdr_metadata = config.hdr_metadata();
  if (hdr_metadata) {
    video_config.have_hdr_metadata = true;
    video_config.hdr_metadata.max_cll = hdr_metadata->max_cll;
    video_config.hdr_metadata.max_fall = hdr_metadata->max_fall;

    const auto& mm1 = hdr_metadata->mastering_metadata;
    auto& mm2 = video_config.hdr_metadata.mastering_metadata;
    mm2.primary_r_chromaticity_x = mm1.primary_r_chromaticity_x;
    mm2.primary_r_chromaticity_y = mm1.primary_r_chromaticity_y;
    mm2.primary_g_chromaticity_x = mm1.primary_g_chromaticity_x;
    mm2.primary_g_chromaticity_y = mm1.primary_g_chromaticity_y;
    mm2.primary_b_chromaticity_x = mm1.primary_b_chromaticity_x;
    mm2.primary_b_chromaticity_y = mm1.primary_b_chromaticity_y;
    mm2.white_point_chromaticity_x = mm1.white_point_chromaticity_x;
    mm2.white_point_chromaticity_y = mm1.white_point_chromaticity_y;
    mm2.luminance_max = mm1.luminance_max;
    mm2.luminance_min = mm1.luminance_min;
  }

  return video_config;
}

}  // namespace media
}  // namespace chromecast
