// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_
#define CHROMEOS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class Bus;
class ExportedObject;
class ObjectPath;
}

namespace chromeos {

// CrosDBusService is used to run a D-Bus service inside Chrome for Chrome OS.
// It exports D-Bus methods through service provider classes that implement
// CrosDBusService::ServiceProviderInterface.
class CHROMEOS_EXPORT CrosDBusService {
 public:
  // CrosDBusService consists of service providers that implement this
  // interface.
  class ServiceProviderInterface {
   public:
    // Starts the service provider. |exported_object| is used to export
    // D-Bus methods.
    virtual void Start(
        scoped_refptr<dbus::ExportedObject> exported_object) = 0;

    virtual ~ServiceProviderInterface();
  };

  using ServiceProviderList =
      std::vector<std::unique_ptr<ServiceProviderInterface>>;

  // Creates, starts, and returns a new instance owning |service_name| and
  // exporting |service_providers|'s methods on |object_path|. Static so a stub
  // implementation can be used when not running on a device.
  static std::unique_ptr<CrosDBusService> Create(
      const std::string& service_name,
      const dbus::ObjectPath& object_path,
      ServiceProviderList service_providers);

  virtual ~CrosDBusService();

 protected:
  CrosDBusService();

 private:
  friend class CrosDBusServiceTest;

  // Creates, starts, and returns a real implementation of CrosDBusService that
  // uses |bus|. Called by Create(), but can also be called directly by tests
  // that need a non-stub implementation even when not running on a device.
  static std::unique_ptr<CrosDBusService> CreateRealImpl(
      dbus::Bus* bus,
      const std::string& service_name,
      const dbus::ObjectPath& object_path,
      ServiceProviderList service_providers);

  DISALLOW_COPY_AND_ASSIGN(CrosDBusService);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_
