// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_url_generator.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "google_apis/drive/drive_switches.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

namespace google_apis {

namespace {

// Hard coded URLs for communication with a google drive server.
// TODO(yamaguchi): Make a utility function to compose some of these URLs by a
// version and a resource name.
const char kDriveV2AboutUrl[] = "drive/v2/about";
const char kDriveV2AppsUrl[] = "drive/v2/apps";
const char kDriveV2ChangelistUrl[] = "drive/v2/changes";
const char kDriveV2BetaChangelistUrl[] = "drive/v2beta/changes";
const char kDriveV2FilesUrl[] = "drive/v2/files";
const char kDriveV2BetaFilesUrl[] = "drive/v2beta/files";
const char kDriveV2FileUrlPrefix[] = "drive/v2/files/";
const char kDriveV2BetaFileUrlPrefix[] = "drive/v2beta/files/";
const char kDriveV2ChildrenUrlFormat[] = "drive/v2/files/%s/children";
const char kDriveV2ChildrenUrlForRemovalFormat[] =
    "drive/v2/files/%s/children/%s";
const char kDriveV2FileCopyUrlFormat[] = "drive/v2/files/%s/copy";
const char kDriveV2FileDeleteUrlFormat[] = "drive/v2/files/%s";
const char kDriveV2FileTrashUrlFormat[] = "drive/v2/files/%s/trash";
const char kDriveV2UploadNewFileUrl[] = "upload/drive/v2/files";
const char kDriveV2UploadExistingFileUrlPrefix[] = "upload/drive/v2/files/";
const char kDriveV2BatchUploadUrl[] = "upload/drive";
const char kDriveV2PermissionsUrlFormat[] = "drive/v2/files/%s/permissions";
const char kDriveV2DownloadUrlFormat[] = "drive/v2/files/%s?alt=media";
const char kDriveV2ThumbnailUrlFormat[] = "d/%s=w%d-h%d";
const char kDriveV2ThumbnailUrlWithCropFormat[] = "d/%s=w%d-h%d-c";

const char kIncludeTeamDriveItems[] = "includeTeamDriveItems";
const char kSupportsTeamDrives[] = "supportsTeamDrives";

// apps.delete and file.authorize API is exposed through a special endpoint
// v2internal that is accessible only by the official API key for Chrome.
const char kDriveV2InternalAppsUrl[] = "drive/v2internal/apps";
const char kDriveV2AppsDeleteUrlFormat[] = "drive/v2internal/apps/%s";
const char kDriveV2FilesAuthorizeUrlFormat[] =
    "drive/v2internal/files/%s/authorize?appId=%s";
const char kDriveV2InternalFileUrlPrefix[] = "drive/v2internal/files/";

GURL AddResumableUploadParam(const GURL& url) {
  return net::AppendOrReplaceQueryParameter(url, "uploadType", "resumable");
}

GURL AddMultipartUploadParam(const GURL& url) {
  return net::AppendOrReplaceQueryParameter(url, "uploadType", "multipart");
}

}  // namespace

DriveApiUrlGenerator::DriveApiUrlGenerator(
    const GURL& base_url, const GURL& base_thumbnail_url,
    TeamDrivesIntegrationStatus team_drives_integration)
    : base_url_(base_url),
      base_thumbnail_url_(base_thumbnail_url),
      enable_team_drives_(
          team_drives_integration == TEAM_DRIVES_INTEGRATION_ENABLED) {
  // Do nothing.
}

DriveApiUrlGenerator::DriveApiUrlGenerator(const DriveApiUrlGenerator& src) =
    default;

DriveApiUrlGenerator::~DriveApiUrlGenerator() {
  // Do nothing.
}

const char DriveApiUrlGenerator::kBaseUrlForProduction[] =
    "https://www.googleapis.com";

const char DriveApiUrlGenerator::kBaseThumbnailUrlForProduction[] =
    "https://lh3.googleusercontent.com";

GURL DriveApiUrlGenerator::GetAboutGetUrl() const {
  return base_url_.Resolve(kDriveV2AboutUrl);
}

GURL DriveApiUrlGenerator::GetAppsListUrl(bool use_internal_endpoint) const {
  return base_url_.Resolve(use_internal_endpoint ?
      kDriveV2InternalAppsUrl : kDriveV2AppsUrl);
}

GURL DriveApiUrlGenerator::GetAppsDeleteUrl(const std::string& app_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2AppsDeleteUrlFormat, net::EscapePath(app_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFilesGetUrl(const std::string& file_id,
                                          bool use_internal_endpoint,
                                          const GURL& embed_origin) const {
  const char* url_prefix = nullptr;
  if (use_internal_endpoint)
    url_prefix = kDriveV2InternalFileUrlPrefix;
  else if (enable_team_drives_)
    url_prefix = kDriveV2BetaFileUrlPrefix;
  else
    url_prefix = kDriveV2FileUrlPrefix;

  GURL url = base_url_.Resolve(url_prefix + net::EscapePath(file_id));

  if (enable_team_drives_)
    url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");

  if (!embed_origin.is_empty()) {
    // Construct a valid serialized embed origin from an url, according to
    // WD-html5-20110525. Such string has to be built manually, since
    // GURL::spec() always adds the trailing slash. Moreover, ports are
    // currently not supported.
    DCHECK(!embed_origin.has_port());
    DCHECK(!embed_origin.has_path() || embed_origin.path() == "/");
    const std::string serialized_embed_origin =
        embed_origin.scheme() + "://" + embed_origin.host();
    url = net::AppendOrReplaceQueryParameter(
        url, "embedOrigin", serialized_embed_origin);
  }
  return url;
}

GURL DriveApiUrlGenerator::GetFilesAuthorizeUrl(
    const std::string& file_id,
    const std::string& app_id) const {
  return base_url_.Resolve(base::StringPrintf(kDriveV2FilesAuthorizeUrlFormat,
                                              net::EscapePath(file_id).c_str(),
                                              net::EscapePath(app_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFilesInsertUrl(
    const std::string& visibility) const {
  GURL url =  base_url_.Resolve(kDriveV2FilesUrl);

  if (!visibility.empty())
    url = net::AppendOrReplaceQueryParameter(url, "visibility", visibility);

  return url;
}

GURL DriveApiUrlGenerator::GetFilesPatchUrl(const std::string& file_id,
                                            bool set_modified_date,
                                            bool update_viewed_date) const {
  GURL url =
      base_url_.Resolve(kDriveV2FileUrlPrefix + net::EscapePath(file_id));

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  // updateViewedDate is "true" by default.
  if (!update_viewed_date)
    url = net::AppendOrReplaceQueryParameter(url, "updateViewedDate", "false");

  return url;
}

GURL DriveApiUrlGenerator::GetFilesCopyUrl(
    const std::string& file_id,
    const std::string& visibility) const {
  GURL url =  base_url_.Resolve(base::StringPrintf(
      kDriveV2FileCopyUrlFormat, net::EscapePath(file_id).c_str()));

  if (!visibility.empty())
    url = net::AppendOrReplaceQueryParameter(url, "visibility", visibility);

  return url;
}

GURL DriveApiUrlGenerator::GetFilesListUrl(int max_results,
                                           const std::string& page_token,
                                           const std::string& q) const {
  GURL url;
  if (enable_team_drives_) {
    url = base_url_.Resolve(kDriveV2BetaFilesUrl);
    url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
    url = net::AppendOrReplaceQueryParameter(url, kIncludeTeamDriveItems,
                                             "true");
  } else {
    url = base_url_.Resolve(kDriveV2FilesUrl);
  }
  // maxResults is 100 by default.
  if (max_results != 100) {
    url = net::AppendOrReplaceQueryParameter(
        url, "maxResults", base::IntToString(max_results));
  }

  if (!page_token.empty())
    url = net::AppendOrReplaceQueryParameter(url, "pageToken", page_token);

  if (!q.empty())
    url = net::AppendOrReplaceQueryParameter(url, "q", q);

  return url;
}

GURL DriveApiUrlGenerator::GetFilesDeleteUrl(const std::string& file_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2FileDeleteUrlFormat, net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFilesTrashUrl(const std::string& file_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2FileTrashUrlFormat, net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetChangesListUrl(bool include_deleted,
                                             int max_results,
                                             const std::string& page_token,
                                             int64_t start_change_id) const {
  DCHECK_GE(start_change_id, 0);

  GURL url;
  if (enable_team_drives_) {
    url = base_url_.Resolve(kDriveV2BetaChangelistUrl);
    url = net::AppendOrReplaceQueryParameter(url, kSupportsTeamDrives, "true");
    url = net::AppendOrReplaceQueryParameter(url, kIncludeTeamDriveItems,
                                             "true");
  } else {
    url = base_url_.Resolve(kDriveV2ChangelistUrl);
  }
  // includeDeleted is "true" by default.
  if (!include_deleted)
    url = net::AppendOrReplaceQueryParameter(url, "includeDeleted", "false");

  // maxResults is "100" by default.
  if (max_results != 100) {
    url = net::AppendOrReplaceQueryParameter(
        url, "maxResults", base::IntToString(max_results));
  }

  if (!page_token.empty())
    url = net::AppendOrReplaceQueryParameter(url, "pageToken", page_token);

  if (start_change_id > 0)
    url = net::AppendOrReplaceQueryParameter(
        url, "startChangeId", base::Int64ToString(start_change_id));

  return url;
}

GURL DriveApiUrlGenerator::GetChildrenInsertUrl(
    const std::string& file_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2ChildrenUrlFormat, net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetChildrenDeleteUrl(
    const std::string& child_id, const std::string& folder_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2ChildrenUrlForRemovalFormat,
                         net::EscapePath(folder_id).c_str(),
                         net::EscapePath(child_id).c_str()));
}

GURL DriveApiUrlGenerator::GetInitiateUploadNewFileUrl(
    bool set_modified_date) const {
  GURL url = AddResumableUploadParam(
      base_url_.Resolve(kDriveV2UploadNewFileUrl));

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GetInitiateUploadExistingFileUrl(
    const std::string& resource_id,
    bool set_modified_date) const {
  GURL url = base_url_.Resolve(
      kDriveV2UploadExistingFileUrlPrefix +
      net::EscapePath(resource_id));
  url = AddResumableUploadParam(url);

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GetMultipartUploadNewFileUrl(
    bool set_modified_date) const {
  GURL url = AddMultipartUploadParam(
      base_url_.Resolve(kDriveV2UploadNewFileUrl));

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GetMultipartUploadExistingFileUrl(
    const std::string& resource_id,
    bool set_modified_date) const {
  GURL url = base_url_.Resolve(
      kDriveV2UploadExistingFileUrlPrefix +
      net::EscapePath(resource_id));
  url = AddMultipartUploadParam(url);

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GenerateDownloadFileUrl(
    const std::string& resource_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2DownloadUrlFormat, net::EscapePath(resource_id).c_str()));
}

GURL DriveApiUrlGenerator::GetPermissionsInsertUrl(
    const std::string& resource_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2PermissionsUrlFormat,
                         net::EscapePath(resource_id).c_str()));
}

GURL DriveApiUrlGenerator::GetThumbnailUrl(const std::string& resource_id,
                                           int width,
                                           int height,
                                           bool crop) const {
  return base_thumbnail_url_.Resolve(
    base::StringPrintf(
        crop ? kDriveV2ThumbnailUrlWithCropFormat : kDriveV2ThumbnailUrlFormat,
        net::EscapePath(resource_id).c_str(), width, height));
}

GURL DriveApiUrlGenerator::GetBatchUploadUrl() const {
  return base_url_.Resolve(kDriveV2BatchUploadUrl);
}

}  // namespace google_apis
