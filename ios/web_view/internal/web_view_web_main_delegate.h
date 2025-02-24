// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_

#include <memory>
#include "base/macros.h"
#include "ios/web/public/app/web_main_delegate.h"

@protocol CWVDelegate;

namespace ios_web_view {
class WebViewWebClient;

// WebView implementation of WebMainDelegate.
class WebViewWebMainDelegate : public web::WebMainDelegate {
 public:
  explicit WebViewWebMainDelegate(id<CWVDelegate> delegate);
  ~WebViewWebMainDelegate() override;

  // WebMainDelegate implementation.
  void BasicStartupComplete() override;

 private:
  // This object's delegate.
  __weak id<CWVDelegate> delegate_;

  // The content and web clients registered by this object.
  std::unique_ptr<WebViewWebClient> web_client_;

  DISALLOW_COPY_AND_ASSIGN(WebViewWebMainDelegate);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_
