// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

/**
 * This class is a Java counterpart to the C++ ContentId
 * (components/offline_items_collection/core/offline_item.h) class.
 *
 * For all member variable descriptions see the C++ class.
 * TODO(dtrainor): Investigate making all class members for this and the C++ counterpart const.
 */
public class ContentId {
    public String namespace;
    public String id;

    public ContentId() {}
    public ContentId(String namespace, String id) {
        this.namespace = namespace;
        this.id = id;
    }
}