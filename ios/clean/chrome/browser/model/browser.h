// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_MODEL_BROWSER_H_
#define IOS_CLEAN_CHROME_BROWSER_MODEL_BROWSER_H_

#include "base/macros.h"
#include "ios/shared/chrome/browser/tabs/web_state_list.h"

namespace ios {
class ChromeBrowserState;
}

// Browser holds the state backing a collection of Tabs and the attached
// UI elements (Tab strip, ...).
class Browser {
 public:
  explicit Browser(ios::ChromeBrowserState* browser_state);
  ~Browser();

  WebStateList& web_state_list() { return web_state_list_; }
  const WebStateList& web_state_list() const { return web_state_list_; }

  ios::ChromeBrowserState* browser_state() const { return browser_state_; }

 private:
  WebStateList web_state_list_;
  ios::ChromeBrowserState* browser_state_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_MODEL_BROWSER_H_
