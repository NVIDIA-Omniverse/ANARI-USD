// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeConnection_h
#define UsdBridgeConnection_h

#include "../UsdBridgeData.h"

#include <string>
#include <fstream>
#include <sstream>

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
    UsdBridgeLogCallback logCallback, void* logUserData) = 0;
  virtual void Shutdown() = 0;

  virtual int MaxSessionNr() const = 0;

  virtual bool CreateFolder(const char* dirName, bool mayExist) const = 0;
  virtual bool RemoveFolder(const char* dirName) const = 0;
  virtual bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary = true) const = 0;
  virtual bool RemoveFile(const char* filePath) const = 0;
  virtual bool LockFile(const char* filePath) const = 0;
  virtual bool UnlockFile(const char* filePath) const = 0;

  virtual std::ostream* GetStream(const char* filePath, bool binary = true) = 0;
  virtual void FlushStream() = 0;

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
    UsdBridgeLogCallback logCallback, void* logUserData) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool mayExist) const override;
  bool RemoveFolder(const char* dirName) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary = true) const override;
  bool RemoveFile(const char* filePath) const override;
  bool LockFile(const char* filePath) const override;
  bool UnlockFile(const char* filePath) const override;

  std::ostream* GetStream(const char* filePath, bool binary = true) override;
  void FlushStream() override;

  bool ProcessUpdates() override;

protected:

  std::fstream LocalStream;
};

class UsdBridgeRemoteConnection : public UsdBridgeConnection
{
public:
  UsdBridgeRemoteConnection();
  ~UsdBridgeRemoteConnection() override;

  const char* GetBaseUrl() const override;
  const char* GetUrl(const char* path) const override;

  bool Initialize(const UsdBridgeConnectionSettings& settings,
    UsdBridgeLogCallback logCallback, void* logUserData) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool mayExist) const override;
  bool RemoveFolder(const char* dirName) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary = true) const override;
  bool RemoveFile(const char* filePath) const override;
  bool LockFile(const char* filePath) const override;
  bool UnlockFile(const char* filePath) const override;

  std::ostream* GetStream(const char* filePath, bool binary = true) override;
  void FlushStream() override;

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
    UsdBridgeLogCallback logCallback, void* logUserData) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool mayExist) const override;
  bool RemoveFolder(const char* dirName) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary = true) const override;
  bool RemoveFile(const char* filePath) const override;
  bool LockFile(const char* filePath) const override;
  bool UnlockFile(const char* filePath) const override;

  std::ostream* GetStream(const char* filePath, bool binary = true) override;
  void FlushStream() override;

  bool ProcessUpdates() override;

protected:

  std::stringstream DummyStream;
};


#endif
