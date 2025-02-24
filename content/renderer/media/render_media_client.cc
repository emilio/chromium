// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_client.h"

#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"

namespace content {

void RenderMediaClient::Initialize() {
  GetInstance();
}

RenderMediaClient::RenderMediaClient()
    : has_updated_(false),
      is_update_needed_(true),
      tick_clock_(new base::DefaultTickClock()) {
  media::SetMediaClient(this);
}

RenderMediaClient::~RenderMediaClient() {
}

void RenderMediaClient::AddKeySystemsInfoForUMA(
    std::vector<media::KeySystemInfoForUMA>* key_systems_info_for_uma) {
  DVLOG(2) << __func__;
#if defined(WIDEVINE_CDM_AVAILABLE)
  key_systems_info_for_uma->push_back(media::KeySystemInfoForUMA(
      kWidevineKeySystem, kWidevineKeySystemNameForUMA));
#endif  // WIDEVINE_CDM_AVAILABLE
}

bool RenderMediaClient::IsKeySystemsUpdateNeeded() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Always needs update if we have never updated, regardless the
  // |last_update_time_ticks_|'s initial value.
  if (!has_updated_) {
    DCHECK(is_update_needed_);
    return true;
  }

  if (!is_update_needed_)
    return false;

  // The update could be expensive. For example, it could involve a sync IPC to
  // the browser process. Use a minimum update interval to avoid unnecessarily
  // frequent update.
  static const int kMinUpdateIntervalInMilliseconds = 1000;
  if ((tick_clock_->NowTicks() - last_update_time_ticks_).InMilliseconds() <
      kMinUpdateIntervalInMilliseconds) {
    return false;
  }

  return true;
}

void RenderMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>*
        key_systems_properties) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  GetContentClient()->renderer()->AddSupportedKeySystems(
      key_systems_properties);

  has_updated_ = true;
  last_update_time_ticks_ = tick_clock_->NowTicks();

  // Check whether all potentially supported key systems are supported. If so,
  // no need to update again.
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  for (const auto& properties : *key_systems_properties) {
    if (properties->GetKeySystemName() == kWidevineKeySystem)
      is_update_needed_ = false;
  }
#else
  is_update_needed_ = false;
#endif
}

void RenderMediaClient::RecordRapporURL(const std::string& metric,
                                        const GURL& url) {
  GetContentClient()->renderer()->RecordRapporURL(metric, url);
}

bool IsTransferFunctionSupported(gfx::ColorSpace::TransferID eotf) {
  switch (eotf) {
    // Transfers supported without color management.
    case gfx::ColorSpace::TransferID::GAMMA22:
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::SMPTE170M:
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
    case gfx::ColorSpace::TransferID::IEC61966_2_1:
      return true;
    default:
      // TODO(hubbe): wire up support for HDR transfers.
      return false;
  }
}

bool RenderMediaClient::IsSupportedVideoConfig(
    const media::VideoConfig& config) {
  // TODO(chcunningham): Query decoders for codec profile support.
  switch (config.codec) {
    case media::kCodecVP9:
      // Color management required for HDR to not look terrible.
      return IsTransferFunctionSupported(config.eotf);

    case media::kCodecH264:
    case media::kCodecVP8:
    case media::kCodecTheora:
      return true;

    case media::kUnknownVideoCodec:
    case media::kCodecVC1:
    case media::kCodecMPEG2:
    case media::kCodecMPEG4:
    case media::kCodecHEVC:
    case media::kCodecDolbyVision:
      return false;
  }

  NOTREACHED();
  return false;
}

void RenderMediaClient::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  tick_clock_.swap(tick_clock);
}

// static
RenderMediaClient* RenderMediaClient::GetInstance() {
  static RenderMediaClient* client = new RenderMediaClient();
  return client;
}

}  // namespace content
