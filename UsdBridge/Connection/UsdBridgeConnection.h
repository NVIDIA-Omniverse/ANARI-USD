// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeConnection_h
#define UsdBridgeConnection_h

#include "UsdBridgeData.h"

#include <string>

class UsdBridgeRemoteConnectionInternals;

struct UsdBridgeConnectionSettings
{
  std::string HostName;
  std::string WorkingDirectory;
};

class UsdBridgeConnection
{
public:
  UsdBridgeConnection() {}
  virtual ~UsdBridgeConnection() {};

  virtual const char* GetBaseUrl() const = 0;
  virtual const char* GetUrl(const char* path) const = 0;

  virtual bool Initialize(const UsdBridgeConnectionSettings& settings,
    const UsdBridgeLogObject& logObj) = 0;
  virtual void Shutdown() = 0;

  virtual int MaxSessionNr() const = 0;

  virtual bool CreateFolder(const char* dirName, bool isRelative, bool mayExist) const = 0;
  virtual bool RemoveFolder(const char* dirName, bool isRelative) const = 0;
  virtual bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary = true) const = 0;
  virtual bool RemoveFile(const char* filePath, bool isRelative) const = 0;

  virtual bool ProcessUpdates() = 0;

  static UsdBridgeLogCallback LogCallback;
  static void* LogUserData;

  UsdBridgeConnectionSettings Settings;

protected:

  mutable std::string TempUrl;
};


class UsdBridgeLocalConnection : public UsdBridgeConnection
{
public:
  UsdBridgeLocalConnection();
  ~UsdBridgeLocalConnection() override;

  const char* GetBaseUrl() const override;
  const char* GetUrl(const char* path) const override;

  bool Initialize(const UsdBridgeConnectionSettings& settings,
    const UsdBridgeLogObject& logObj) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool isRelative, bool mayExist) const override;
  bool RemoveFolder(const char* dirName, bool isRelative) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary = true) const override;
  bool RemoveFile(const char* filePath, bool isRelative) const override;

  bool ProcessUpdates() override;

protected:
};

class UsdBridgeRemoteConnection : public UsdBridgeConnection
{
public:
  UsdBridgeRemoteConnection();
  ~UsdBridgeRemoteConnection() override;

  const char* GetBaseUrl() const override;
  const char* GetUrl(const char* path) const override;

  bool Initialize(const UsdBridgeConnectionSettings& settings,
    const UsdBridgeLogObject& logObj) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool isRelative, bool mayExist) const override;
  bool RemoveFolder(const char* dirName, bool isRelative) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary = true) const override;
  bool RemoveFile(const char* filePath, bool isRelative) const override;

  bool ProcessUpdates() override;

  static void SetConnectionLogLevel(int logLevel);
  static int GetConnectionLogLevelMax();

  static int NumConnInstances;
  static int NumInitializedConnInstances;

protected:

  bool CheckWritePermissions();

  UsdBridgeRemoteConnectionInternals* Internals;
  bool ConnectionInitialized = false;

  static int ConnectionLogLevel;
};

class UsdBridgeVoidConnection : public UsdBridgeConnection
{
public:
  UsdBridgeVoidConnection();
  ~UsdBridgeVoidConnection() override;

  const char* GetBaseUrl() const override;
  const char* GetUrl(const char* path) const override;

  bool Initialize(const UsdBridgeConnectionSettings& settings,
    const UsdBridgeLogObject& logObj) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool isRelative, bool mayExist) const override;
  bool RemoveFolder(const char* dirName, bool isRelative) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary = true) const override;
  bool RemoveFile(const char* filePath, bool isRelative) const override;

  bool ProcessUpdates() override;

protected:
};


#endif
