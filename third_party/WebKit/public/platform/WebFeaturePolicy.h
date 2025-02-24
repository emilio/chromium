// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFeaturePolicy_h
#define WebFeaturePolicy_h

#include "WebCommon.h"
#include "WebSecurityOrigin.h"
#include "WebString.h"
#include "WebVector.h"

namespace blink {

// These values map to the features which can be controlled by Feature Policy.
// TODO(iclelland): Link to the spec where the behaviour for each of these is
// defined.
enum class WebFeaturePolicyFeature {
  NotFound = 0,
  // Controls access to document.cookie attribute.
  DocumentCookie,
  // Contols access to document.domain attribute.
  DocumentDomain,
  // Controls access to document.write and document.writeln methods.
  DocumentWrite,
  // Controls whether Element.requestFullscreen is allowed.
  Fullscreen,
  // Controls access to Geolocation interface.
  Geolocation,
  // Controls access to requestMIDIAccess method.
  MidiFeature,
  // Controls access to Notification interface.
  Notifications,
  // Controls access to PaymentRequest interface.
  Payment,
  // Controls access to PushManager interface.
  Push,
  // Controls whether synchronous script elements will run.
  SyncScript,
  // Controls use of synchronous XMLHTTPRequest API.
  SyncXHR,
  // Controls access to NavigatorUserMedia interface.
  Usermedia,
  // Controls access to navigator.vibrate method.
  Vibrate,
  // Controls access to RTCPeerConnection interface.
  WebRTC,
  LAST_FEATURE = WebRTC
};

struct BLINK_PLATFORM_EXPORT WebParsedFeaturePolicyDeclaration {
  WebParsedFeaturePolicyDeclaration() : matchesAllOrigins(false) {}
  WebString featureName;
  bool matchesAllOrigins;
  WebVector<WebSecurityOrigin> origins;
};

// Used in Blink code to represent parsed headers. Used for IPC between renderer
// and browser.
using WebParsedFeaturePolicyHeader =
    WebVector<WebParsedFeaturePolicyDeclaration>;

// Composed full policy for a document. Stored in SecurityContext for each
// document. This is essentially an opaque handle to an object in the embedder.
class BLINK_PLATFORM_EXPORT WebFeaturePolicy {
 public:
  virtual ~WebFeaturePolicy() {}

  // Returns whether or not the given feature is enabled for the origin of the
  // document that owns the policy.
  virtual bool IsFeatureEnabled(blink::WebFeaturePolicyFeature) const = 0;
};

}  // namespace blink

#endif  // WebFeaturePolicy_h
