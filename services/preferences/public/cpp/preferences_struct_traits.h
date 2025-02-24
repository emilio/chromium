// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREFERENCES_STRUCT_TRAITS_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREFERENCES_STRUCT_TRAITS_H_

#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "services/preferences/public/interfaces/preferences.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<::prefs::mojom::PrefStoreType,
                  ::PrefValueStore::PrefStoreType> {
  static prefs::mojom::PrefStoreType ToMojom(
      PrefValueStore::PrefStoreType input);

  static bool FromMojom(prefs::mojom::PrefStoreType input,
                        PrefValueStore::PrefStoreType* output);
};

}  // namespace mojo

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREFERENCES_STRUCT_TRAITS_H_
