// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_CRASH_KEYS_H_
#define ANDROID_WEBVIEW_COMMON_CRASH_KEYS_H_

#include <stddef.h>
#include <string>

namespace android_webview {
namespace crash_keys {

// Registers all of the potential crash keys that can be sent to the crash
// reporting server. Returns the size of the union of all keys.
size_t RegisterWebViewCrashKeys();
void InitCrashKeysForWebViewTesting();
void SetCrashKeyValue(const std::string& key, const std::string& value);

extern const char* const kWebViewCrashKeyWhiteList[];

// Crash Key Name Constants ////////////////////////////////////////////////////

// GPU information.
extern const char kGPUDriverVersion[];
extern const char kGPUPixelShaderVersion[];
extern const char kGPUVertexShaderVersion[];
extern const char kGPUVendor[];
extern const char kGPURenderer[];


}  // namespace crash_keys
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_CRASH_KEYS_H_
