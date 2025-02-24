/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef DefaultAudioDestinationNode_h
#define DefaultAudioDestinationNode_h

#include <memory>
#include "modules/webaudio/AudioDestinationNode.h"
#include "platform/audio/AudioDestination.h"
#include "public/platform/WebAudioLatencyHint.h"

namespace blink {

class BaseAudioContext;
class ExceptionState;
class WebAudioLatencyHint;

class DefaultAudioDestinationHandler final : public AudioDestinationHandler {
 public:
  static PassRefPtr<DefaultAudioDestinationHandler> create(
      AudioNode&,
      const WebAudioLatencyHint&);
  ~DefaultAudioDestinationHandler() override;

  // AudioHandler
  void dispose() override;
  void initialize() override;
  void uninitialize() override;
  void setChannelCount(unsigned long, ExceptionState&) override;

  // AudioDestinationHandler
  void startRendering() override;
  void stopRendering() override;
  unsigned long maxChannelCount() const override;
  // Returns the rendering callback buffer size.
  size_t callbackBufferSize() const override;
  double sampleRate() const override;
  int framesPerBuffer() const override;

 private:
  explicit DefaultAudioDestinationHandler(AudioNode&,
                                          const WebAudioLatencyHint&);
  void createDestination();

  std::unique_ptr<AudioDestination> m_destination;
  String m_inputDeviceId;
  unsigned m_numberOfInputChannels;
  const WebAudioLatencyHint m_latencyHint;
};

class DefaultAudioDestinationNode final : public AudioDestinationNode {
 public:
  static DefaultAudioDestinationNode* create(BaseAudioContext*,
                                             const WebAudioLatencyHint&);

  size_t callbackBufferSize() const { return handler().callbackBufferSize(); };

 private:
  explicit DefaultAudioDestinationNode(BaseAudioContext&,
                                       const WebAudioLatencyHint&);
};

}  // namespace blink

#endif  // DefaultAudioDestinationNode_h
