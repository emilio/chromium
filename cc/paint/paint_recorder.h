// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORDER_H_
#define CC_PAINT_PAINT_RECORDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace cc {

class CC_PAINT_EXPORT PaintRecorder {
 public:
  PaintRecorder();
  ~PaintRecorder();

  ALWAYS_INLINE PaintCanvas* beginRecording(const SkRect& bounds) {
    uint32_t record_flags = 0;
    canvas_.emplace(recorder_.beginRecording(bounds, nullptr, record_flags));
    return getRecordingCanvas();
  }

  ALWAYS_INLINE PaintCanvas* beginRecording(
      SkScalar width,
      SkScalar height,
      SkBBHFactory* rtree_factory = nullptr) {
    uint32_t record_flags = 0;
    canvas_.emplace(
        recorder_.beginRecording(width, height, rtree_factory, record_flags));
    return getRecordingCanvas();
  }

  // Only valid between between and finish recording.
  ALWAYS_INLINE PaintCanvas* getRecordingCanvas() {
    return canvas_.has_value() ? &canvas_.value() : nullptr;
  }

  ALWAYS_INLINE sk_sp<PaintRecord> finishRecordingAsPicture(
      uint32_t end_flags = 0) {
    sk_sp<SkPicture> picture = recorder_.finishRecordingAsPicture(end_flags);
    // Some users (e.g. printing) use the existence of the recording canvas
    // to know if recording is finished, so reset it here.
    canvas_.reset();
    return sk_ref_sp(static_cast<PaintRecord*>(picture.get()));
  }

  ALWAYS_INLINE sk_sp<PaintRecord> finishRecordingAsPictureWithCull(
      const SkRect& cull_rect,
      uint32_t end_flags = 0) {
    sk_sp<SkPicture> picture =
        recorder_.finishRecordingAsPictureWithCull(cull_rect, end_flags);
    canvas_.reset();
    return sk_ref_sp(static_cast<PaintRecord*>(picture.get()));
  }

 private:
  SkPictureRecorder recorder_;
  base::Optional<PaintCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(PaintRecorder);
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORDER_H_
