// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_ANDROID_OFFLINE_CONTENT_AGGREGATOR_BRIDGE_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_ANDROID_OFFLINE_CONTENT_AGGREGATOR_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/supports_user_data.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

namespace offline_items_collection {

struct ContentId;
struct OfflineItem;

namespace android {

// A helper class responsible for bridging an OfflineContentAggregator from C++
// to Java.  This class attaches as a piece of SupportsUserData::Data to the
// OfflineContentAggregator and can only be created through the
// GetForOfflineContentAggregator() method.
// This class creates and contains a strong reference to it's Java counterpart,
// so the Java bridge will live as long as this class lives.  For more
// information on the Java counterpart see OfflineContentAggregatorBridge.java.
class OfflineContentAggregatorBridge : public OfflineContentProvider::Observer,
                                       public base::SupportsUserData::Data {
 public:
  // Helper method to initialize the JNI hooks between Java and C++.
  static bool Register(JNIEnv* env);

  // Returns an OfflineContentAggregatorBridge for |aggregator|.  There will be
  // only one bridge per OfflineContentAggregator.
  static OfflineContentAggregatorBridge* GetForOfflineContentAggregator(
      OfflineContentAggregator* aggregator);

  ~OfflineContentAggregatorBridge() override;

  // Methods called from Java via JNI.
  jboolean AreItemsAvailable(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& jobj);
  void OpenItem(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& jobj,
                const base::android::JavaParamRef<jstring>& j_namespace,
                const base::android::JavaParamRef<jstring>& j_id);
  void RemoveItem(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jobj,
                  const base::android::JavaParamRef<jstring>& j_namespace,
                  const base::android::JavaParamRef<jstring>& j_id);
  void CancelDownload(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jobj,
                      const base::android::JavaParamRef<jstring>& j_namespace,
                      const base::android::JavaParamRef<jstring>& j_id);
  void PauseDownload(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jobj,
                     const base::android::JavaParamRef<jstring>& j_namespace,
                     const base::android::JavaParamRef<jstring>& j_id);
  void ResumeDownload(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jobj,
                      const base::android::JavaParamRef<jstring>& j_namespace,
                      const base::android::JavaParamRef<jstring>& j_id);
  base::android::ScopedJavaLocalRef<jobject> GetItemById(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& j_namespace,
      const base::android::JavaParamRef<jstring>& j_id);
  base::android::ScopedJavaLocalRef<jobject> GetAllItems(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

 private:
  OfflineContentAggregatorBridge(OfflineContentAggregator* aggregator);

  // OfflineContentProvider::Observer implementation.
  void OnItemsAvailable(OfflineContentProvider* provider) override;
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item) override;

  // A reference to the Java counterpart of this class.  See
  // OfflineContentAggregatorBridge.java.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  OfflineContentAggregator* const aggregator_;

  DISALLOW_COPY_AND_ASSIGN(OfflineContentAggregatorBridge);
};

}  // namespace android
}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_ANDROID_OFFLINE_CONTENT_AGGREGATOR_BRIDGE_H_
