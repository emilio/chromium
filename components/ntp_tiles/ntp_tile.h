// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/ntp_tiles/ntp_tile_source.h"
#include "url/gurl.h"

namespace ntp_tiles {

// A suggested site shown on the New Tab Page.
struct NTPTile {
  base::string16 title;
  GURL url;
  NTPTileSource source;

  // Empty unless whitelists are enabled and this site is in a whitelist.
  // However, may be non-empty even if |source| is not |WHITELIST|, if this tile
  // is also available from another, higher-priority source.
  base::FilePath whitelist_icon_path;

  // Only valid for source == SUGGESTIONS_SERVICE (empty otherwise).
  // May point to a local chrome:// URL or to a remote one. May be empty.
  GURL thumbnail_url;
  // This won't be empty, but might 404 etc.
  GURL favicon_url;

  NTPTile();
  NTPTile(const NTPTile&);
  ~NTPTile();
};

bool operator==(const NTPTile& a, const NTPTile& b);
bool operator!=(const NTPTile& a, const NTPTile& b);

using NTPTilesVector = std::vector<NTPTile>;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_H_
