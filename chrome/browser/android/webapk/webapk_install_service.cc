// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_install_service.h"

#include "base/bind.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service_factory.h"
#include "chrome/browser/android/webapk/webapk_installer.h"

// static
WebApkInstallService* WebApkInstallService::Get(
    content::BrowserContext* context) {
  return WebApkInstallServiceFactory::GetForBrowserContext(context);
}

WebApkInstallService::WebApkInstallService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      weak_ptr_factory_(this) {}

WebApkInstallService::~WebApkInstallService() {}

bool WebApkInstallService::IsInstallInProgress(const GURL& web_manifest_url) {
  return installs_.count(web_manifest_url);
}

void WebApkInstallService::InstallAsync(const ShortcutInfo& shortcut_info,
                                        const SkBitmap& shortcut_icon,
                                        const FinishCallback& finish_callback) {
  DCHECK(!IsInstallInProgress(shortcut_info.manifest_url));

  installs_.insert(shortcut_info.manifest_url);

  WebApkInstaller::InstallAsync(
      browser_context_, shortcut_info, shortcut_icon,
      base::Bind(&WebApkInstallService::OnFinishedInstall,
                 weak_ptr_factory_.GetWeakPtr(), shortcut_info.manifest_url,
                 finish_callback));
}

void WebApkInstallService::UpdateAsync(
    const ShortcutInfo& shortcut_info,
    const SkBitmap& shortcut_icon,
    const std::string& webapk_package,
    int webapk_version,
    const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    const FinishCallback& finish_callback) {
  DCHECK(!IsInstallInProgress(shortcut_info.manifest_url));

  installs_.insert(shortcut_info.manifest_url);

  WebApkInstaller::UpdateAsync(
      browser_context_, shortcut_info, shortcut_icon, webapk_package,
      webapk_version, icon_url_to_murmur2_hash, is_manifest_stale,
      base::Bind(&WebApkInstallService::OnFinishedInstall,
                 weak_ptr_factory_.GetWeakPtr(), shortcut_info.manifest_url,
                 finish_callback));
}

void WebApkInstallService::OnFinishedInstall(
    const GURL& web_manifest_url,
    const FinishCallback& finish_callback,
    bool success,
    bool relax_updates,
    const std::string& webapk_package_name) {
  finish_callback.Run(success, relax_updates, webapk_package_name);
  installs_.erase(web_manifest_url);
}
