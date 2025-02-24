// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_util.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

class Static {
 public:
  Static() {
    TRACE_EVENT0("startup", "extensions::FeatureProvider::Static");
    base::Time begin_time = base::Time::Now();

    ExtensionsClient* client = ExtensionsClient::Get();
    feature_providers_["api"] = client->CreateFeatureProvider("api");
    feature_providers_["manifest"] = client->CreateFeatureProvider("manifest");
    feature_providers_["permission"] =
        client->CreateFeatureProvider("permission");
    feature_providers_["behavior"] = client->CreateFeatureProvider("behavior");

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line->GetSwitchValueASCII(::switches::kProcessType);

    // Measure time only for browser process. This method gets called by the
    // browser process on startup, as well as on renderer and extension
    // processes throughout the execution of the browser. We are more
    // interested in how long this takes as a startup cost, so we are
    // just measuring the time in the browser process.
    if (process_type == std::string()) {
      UMA_HISTOGRAM_TIMES("Extensions.FeatureProviderStaticInitTime",
                          base::Time::Now() - begin_time);
    }
  }

  FeatureProvider* GetFeatures(const std::string& name) const {
    auto it = feature_providers_.find(name);
    if (it == feature_providers_.end())
      CRASH_WITH_MINIDUMP("FeatureProvider \"" + name + "\" not found");
    return it->second.get();
  }

 private:
  std::map<std::string, std::unique_ptr<FeatureProvider>> feature_providers_;

  DISALLOW_COPY_AND_ASSIGN(Static);
};

base::LazyInstance<Static>::Leaky g_static = LAZY_INSTANCE_INITIALIZER;

const Feature* GetFeatureFromProviderByName(const std::string& provider_name,
                                            const std::string& feature_name) {
  const Feature* feature =
      FeatureProvider::GetByName(provider_name)->GetFeature(feature_name);
  // We should always refer to existing features, but we can't CHECK here
  // due to flaky JSONReader fails, see: crbug.com/176381, crbug.com/602936
  DCHECK(feature) << "Feature \"" << feature_name << "\" not found in "
                  << "FeatureProvider \"" << provider_name << "\"";
  return feature;
}

}  // namespace

FeatureProvider::FeatureProvider() {}
FeatureProvider::~FeatureProvider() {}

// static
const FeatureProvider* FeatureProvider::GetByName(const std::string& name) {
  return g_static.Get().GetFeatures(name);
}

// static
const FeatureProvider* FeatureProvider::GetAPIFeatures() {
  return GetByName("api");
}

// static
const FeatureProvider* FeatureProvider::GetManifestFeatures() {
  return GetByName("manifest");
}

// static
const FeatureProvider* FeatureProvider::GetPermissionFeatures() {
  return GetByName("permission");
}

// static
const FeatureProvider* FeatureProvider::GetBehaviorFeatures() {
  return GetByName("behavior");
}

// static
const Feature* FeatureProvider::GetAPIFeature(const std::string& name) {
  return GetFeatureFromProviderByName("api", name);
}

// static
const Feature* FeatureProvider::GetManifestFeature(const std::string& name) {
  return GetFeatureFromProviderByName("manifest", name);
}

// static
const Feature* FeatureProvider::GetPermissionFeature(const std::string& name) {
  return GetFeatureFromProviderByName("permission", name);
}

// static
const Feature* FeatureProvider::GetBehaviorFeature(const std::string& name) {
  return GetFeatureFromProviderByName("behavior", name);
}

Feature* FeatureProvider::GetFeature(const std::string& name) const {
  FeatureMap::const_iterator iter = features_.find(name);
  if (iter != features_.end())
    return iter->second.get();
  else
    return nullptr;
}

Feature* FeatureProvider::GetParent(Feature* feature) const {
  CHECK(feature);
  if (feature->no_parent())
    return nullptr;

  std::vector<std::string> split = base::SplitString(
      feature->name(), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2)
    return nullptr;
  split.pop_back();
  return GetFeature(base::JoinString(split, "."));
}

// Children of a given API are named starting with parent.name()+".", which
// means they'll be contiguous in the features_ std::map.
std::vector<Feature*> FeatureProvider::GetChildren(
    const Feature& parent) const {
  std::string prefix = parent.name() + ".";
  const FeatureMap::const_iterator first_child = features_.lower_bound(prefix);

  // All children have names before (parent.name() + ('.'+1)).
  ++prefix.back();
  const FeatureMap::const_iterator after_children =
      features_.lower_bound(prefix);

  std::vector<Feature*> result;
  result.reserve(std::distance(first_child, after_children));
  for (FeatureMap::const_iterator it = first_child; it != after_children;
       ++it) {
    result.push_back(it->second.get());
  }
  return result;
}

const FeatureMap& FeatureProvider::GetAllFeatures() const {
  return features_;
}

void FeatureProvider::AddFeature(base::StringPiece name,
                                 std::unique_ptr<Feature> feature) {
  features_[name.as_string()] = std::move(feature);
}

void FeatureProvider::AddFeature(base::StringPiece name, Feature* feature) {
  features_[name.as_string()] = std::unique_ptr<Feature>(feature);
}

}  // namespace extensions
