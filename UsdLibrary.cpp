// Copyright 2023 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdDevice.h"
#include "anari/backend/LibraryImpl.h"

#ifndef USDDevice_INTERFACE
#define USDDevice_INTERFACE
#endif

namespace anari {
namespace usd {

const char **query_extensions();

struct UsdLibrary : public anari::LibraryImpl {
  UsdLibrary(void *lib, ANARIStatusCallback defaultStatusCB,
             const void *statusCBPtr);

  ANARIDevice newDevice(const char *subtype) override;
  const char **getDeviceExtensions(const char *deviceType) override;
};

// Definitions ////////////////////////////////////////////////////////////////

UsdLibrary::UsdLibrary(void *lib, ANARIStatusCallback defaultStatusCB,
                       const void *statusCBPtr)
    : anari::LibraryImpl(lib, defaultStatusCB, statusCBPtr) {}

ANARIDevice UsdLibrary::newDevice(const char * /*subtype*/) {
  return (ANARIDevice) new UsdDevice(this_library());
}

const char **UsdLibrary::getDeviceExtensions(const char * /*deviceType*/) {
  return query_extensions();
}

} // namespace usd
} // namespace anari

// Define library entrypoint //////////////////////////////////////////////////

extern "C" USDDevice_INTERFACE
ANARI_DEFINE_LIBRARY_ENTRYPOINT(usd, handle, scb, scbPtr) {
  return (ANARILibrary) new anari::usd::UsdLibrary(handle, scb, scbPtr);
}
