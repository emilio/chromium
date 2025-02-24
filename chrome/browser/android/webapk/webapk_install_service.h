// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

struct ShortcutInfo;
class SkBitmap;

// Service which talks to Chrome WebAPK server and Google Play to generate a
// WebAPK on the server, download it, and install it.
class WebApkInstallService : public KeyedService {
 public:
  using FinishCallback = WebApkInstaller::FinishCallback;

  static WebApkInstallService* Get(content::BrowserContext* browser_context);

  explicit WebApkInstallService(content::BrowserContext* browser_context);
  ~WebApkInstallService() override;

  // Returns whether an install for |web_manifest_url| is in progress.
  bool IsInstallInProgress(const GURL& web_manifest_url);

  // Talks to the Chrome WebAPK server to generate a WebAPK on the server and to
  // Google Play to install the downloaded WebAPK. Calls |callback| once the
  // install completed or failed.
  void InstallAsync(const ShortcutInfo& shortcut_info,
                    const SkBitmap& shortcut_icon,
                    const FinishCallback& finish_callback);

  // Talks to the Chrome WebAPK server to update a WebAPK on the server and to
  // the Google Play server to install the downloaded WebAPK. Calls |callback|
  // after the request to install the WebAPK is sent to Google Play.
  void UpdateAsync(
      const ShortcutInfo& shortcut_info,
      const SkBitmap& shortcut_icon,
      const std::string& webapk_package,
      int webapk_version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const FinishCallback& finish_callback);

 private:
  // Called once the install/update completed or failed.
  void OnFinishedInstall(const GURL& web_manifest_url,
                         const FinishCallback& finish_callback,
                         bool success,
                         bool relax_updates,
                         const std::string& webapk_package_name);

  content::BrowserContext* browser_context_;

  // In progress installs.
  std::set<GURL> installs_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstallService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallService);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_H_
