
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/peerconnection/RTCLegacyStatsReport.h"

namespace blink {

RTCLegacyStatsReport* RTCLegacyStatsReport::create(const String& id,
                                                   const String& type,
                                                   double timestamp) {
  return new RTCLegacyStatsReport(id, type, timestamp);
}

RTCLegacyStatsReport::RTCLegacyStatsReport(const String& id,
                                           const String& type,
                                           double timestamp)
    : m_id(id), m_type(type), m_timestamp(timestamp) {}

Vector<String> RTCLegacyStatsReport::names() const {
  Vector<String> result;
  for (HashMap<String, String>::const_iterator it = m_stats.begin();
       it != m_stats.end(); ++it) {
    result.push_back(it->key);
  }
  return result;
}

void RTCLegacyStatsReport::addStatistic(const String& name,
                                        const String& value) {
  m_stats.insert(name, value);
}

}  // namespace blink
