// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/android/offline_item_bridge.h"

#include "base/android/jni_string.h"
#include "jni/OfflineItemBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace offline_items_collection {
namespace android {

namespace {

// Helper method to unify the OfflineItem conversion argument list to a single
// place.  This is meant to reduce code churn from OfflineItem member
// modification.  The behavior is as follows:
// - The method always returns the new Java OfflineItem instance.
// - If |jlist| is specified (an ArrayList<OfflineItem>), the item is added to
//   that list.  |jlist| can also be null, in which case the item isn't added to
//   anything.
ScopedJavaLocalRef<jobject> createOfflineItemAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const OfflineItem& item) {
  return Java_OfflineItemBridge_createOfflineItemAndMaybeAddToList(
      env, jlist, ConvertUTF8ToJavaString(env, item.id.name_space),
      ConvertUTF8ToJavaString(env, item.id.id),
      ConvertUTF8ToJavaString(env, item.title),
      ConvertUTF8ToJavaString(env, item.description),
      static_cast<jint>(item.filter), item.total_size_bytes,
      item.externally_removed, item.creation_time.ToJavaTime(),
      item.last_accessed_time.ToJavaTime(),
      ConvertUTF8ToJavaString(env, item.page_url.spec()),
      ConvertUTF8ToJavaString(env, item.original_url.spec()),
      item.is_off_the_record, static_cast<jint>(item.state), item.is_resumable,
      item.received_bytes, item.percent_completed, item.time_remaining_ms);
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> OfflineItemBridge::CreateOfflineItem(
    JNIEnv* env,
    const OfflineItem* const item) {
  return item ? createOfflineItemAndMaybeAddToList(env, nullptr, *item)
              : nullptr;
}

// static
ScopedJavaLocalRef<jobject> OfflineItemBridge::CreateOfflineItemList(
    JNIEnv* env,
    const std::vector<OfflineItem>& items) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_OfflineItemBridge_createArrayList(env);

  for (const auto& item : items)
    createOfflineItemAndMaybeAddToList(env, jlist, item);

  return jlist;
}

OfflineItemBridge::OfflineItemBridge() = default;

}  // namespace android
}  // namespace offline_items_collection
