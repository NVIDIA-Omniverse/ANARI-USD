// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeConnection.h"

#include <fstream>
#include <sstream>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <cstring>

#ifdef WIN32
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
#include <filesystem>
namespace fs = std::filesystem;
#else
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#else
#if (__GNUC__ < 8 || __cplusplus < 201703L)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif

#define UsdBridgeLogMacro(level, message) \
  { std::stringstream logStream; \
    logStream << message; \
    std::string logString = logStream.str(); \
    try \
    { \
      UsdBridgeConnection::LogCallback(level, UsdBridgeConnection::LogUserData, logString.c_str()); \
    } \
    catch (...) {} \
  }

#define CONNECT_CATCH(a)                                                       \
  catch (const std::bad_alloc &)                                               \
  {                                                                            \
    UsdBridgeLogMacro(UsdBridgeLogLevel::ERR,"USDBRIDGECONNECTION BAD ALLOC\n");                    \
    return a;                                                                  \
  }                                                                            \
  catch (const std::exception &e)                                              \
  {                                                                            \
    UsdBridgeLogMacro(UsdBridgeLogLevel::ERR,"USDBRIDGECONNECTION ERROR: " << e.what());            \
    return a;                                                                  \
  }                                                                            \
  catch (...)                                                                  \
  {                                                                            \
    UsdBridgeLogMacro(UsdBridgeLogLevel::ERR,"USDBRIDGECONNECTION UNKNOWN EXCEPTION\n");            \
    return a;                                                                  \
  }

UsdBridgeLogCallback UsdBridgeConnection::LogCallback = nullptr;
void* UsdBridgeConnection::LogUserData = nullptr;

const char* UsdBridgeConnection::GetBaseUrl() const
{
  return Settings.WorkingDirectory.c_str();
}

const char* UsdBridgeConnection::GetUrl(const char* path) const
{
  TempUrl = Settings.WorkingDirectory + path;
  return TempUrl.c_str();
}

bool UsdBridgeConnection::Initialize(const UsdBridgeConnectionSettings& settings,
  UsdBridgeLogCallback logCallback, void* logUserData)
{
  Settings = settings;

  UsdBridgeConnection::LogCallback = logCallback;
  UsdBridgeConnection::LogUserData = logUserData;

  return true;
}

void UsdBridgeConnection::Shutdown()
{
}

int UsdBridgeConnection::MaxSessionNr() const
{
  int sessionNr = 0;
  try
  {
    for (;; ++sessionNr)
    {
      TempUrl = Settings.WorkingDirectory + "Session_" + std::to_string(sessionNr);
      if (!fs::exists(TempUrl))
        break;
    }
  }
  CONNECT_CATCH(-1)

  return sessionNr - 1;
}

bool UsdBridgeConnection::CreateFolder(const char* dirName, bool isRelative, bool mayExist) const
{
  try
  {
    const char* dirUrl = isRelative ? GetUrl(dirName) : dirName;
    if (!mayExist || !fs::exists(dirUrl))
      return fs::create_directory(dirUrl);
  }
  CONNECT_CATCH(false)

  return true;
}

bool UsdBridgeConnection::RemoveFolder(const char* dirName, bool isRelative) const
{
  bool success = false;
  try
  {
    const char* dirUrl = isRelative ? GetUrl(dirName) : dirName;
    if(fs::exists(dirUrl))
      success = fs::remove_all(dirUrl);
  }
  CONNECT_CATCH(false)

  return success;
}

bool UsdBridgeConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary) const
{
  try
  {
    const char* fileUrl = isRelative ? GetUrl(filePath) : filePath;
    std::ofstream file(fileUrl, std::ios_base::out
      | std::ios_base::trunc
      | (binary ? std::ios_base::binary : std::ios_base::out));
    if (file.is_open())
    {
      file.write(data, dataSize);
      file.close();
      return true;
    }
  }
  CONNECT_CATCH(false)

  return false;
}

bool UsdBridgeConnection::RemoveFile(const char* filePath, bool isRelative) const
{
  bool success = false;
  try
  {
    const char* fileUrl = isRelative ? GetUrl(filePath) : filePath;
    if (fs::exists(fileUrl))
      success = fs::remove(fileUrl);
  }
  CONNECT_CATCH(false)

  return success;
}

bool UsdBridgeConnection::ProcessUpdates()
{
  return true;
}

class UsdBridgeRemoteConnectionInternals
{
public:
  UsdBridgeRemoteConnectionInternals()
    : RemoteStream(std::ios::in | std::ios::out)
  {}
  ~UsdBridgeRemoteConnectionInternals()
  {}

  std::ostream* ResetStream(const char * filePath, bool binary)
  {
    StreamFilePath = filePath;
    StreamIsBinary = binary;
    RemoteStream.clear();
    return &RemoteStream;
  }

  // To facility the Omniverse path methods
  static constexpr size_t MaxBaseUrlSize = 4096;
  char BaseUrlBuffer[MaxBaseUrlSize];
  char TempUrlBuffer[MaxBaseUrlSize];

  // Status callback handle
  uint32_t StatusCallbackHandle = 0;

  // Stream
  const char* StreamFilePath;
  bool StreamIsBinary;
  std::stringstream RemoteStream;
};

int UsdBridgeRemoteConnection::NumConnInstances = 0;
int UsdBridgeRemoteConnection::NumInitializedConnInstances = 0;
int UsdBridgeRemoteConnection::ConnectionLogLevel = 0;

UsdBridgeRemoteConnection::UsdBridgeRemoteConnection()
  : Internals(new UsdBridgeRemoteConnectionInternals())
{
}

UsdBridgeRemoteConnection::~UsdBridgeRemoteConnection()
{
  delete Internals;
}

#ifdef OMNIVERSE_CONNECTION_ENABLE

#include <OmniClient.h>

namespace
{
  struct DefaultContext
  {
    OmniClientResult result = eOmniClientResult_Error;
    bool done = false;
  };

  const char* omniResultToString(OmniClientResult result)
  {
    const char* msg = nullptr;
    switch (result)
    {
    case eOmniClientResult_Ok: msg = "eOmniClientResult_Ok"; break;
    case eOmniClientResult_OkLatest: msg = "eOmniClientResult_OkLatest"; break;
    case eOmniClientResult_OkNotYetFound: msg = "eOmniClientResult_OkNotYetFound"; break;
    case eOmniClientResult_Error: msg = "eOmniClientResult_Error"; break;
    case eOmniClientResult_ErrorConnection: msg = "eOmniClientResult_ErrorConnection"; break;
    case eOmniClientResult_ErrorNotSupported: msg = "eOmniClientResult_ErrorNotSupported"; break;
    case eOmniClientResult_ErrorAccessDenied: msg = "eOmniClientResult_ErrorAccessDenied"; break;
    case eOmniClientResult_ErrorNotFound: msg = "eOmniClientResult_ErrorNotFound"; break;
    case eOmniClientResult_ErrorBadVersion: msg = "eOmniClientResult_ErrorBadVersion"; break;
    case eOmniClientResult_ErrorAlreadyExists: msg = "eOmniClientResult_ErrorAlreadyExists"; break;
    case eOmniClientResult_ErrorAccessLost: msg = "eOmniClientResult_ErrorAccessLost"; break;
    default: msg = "Unknown OmniClientResult"; break;
    };
    return msg;
  }

  void OmniClientStatusCB(const char* threadName, const char* component, OmniClientLogLevel level, const char* message) OMNICLIENT_NOEXCEPT
  {
    static std::mutex statusMutex;
    std::unique_lock<std::mutex> lk(statusMutex);

    // This will return "Verbose", "Info", "Warning", "Error"
    //const char* levelString = omniClientGetLogLevelString(level);
    char levelChar = omniClientGetLogLevelChar(level);

    UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "OmniClient status message: ");
    UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "level: " << levelChar << ", thread: " << threadName << ", component: " << component << ", msg: " << message);
  }

  void OmniClientConnectionStatusCB(void* userData, char const* url, OmniClientConnectionStatus status) OMNICLIENT_NOEXCEPT
  {
    (void)userData;
    static std::mutex statusMutex;
    std::unique_lock<std::mutex> lk(statusMutex);

    {
      std::string urlStr;
      if (url) urlStr = url;
      switch (status)
      {
      case eOmniClientConnectionStatus_Connecting: { UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Attempting to connect to " << urlStr); break; }
      case eOmniClientConnectionStatus_Connected: { UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Successfully connected to " << urlStr); break; }
      case eOmniClientConnectionStatus_ConnectError: { UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Error trying to connect (will attempt reconnection) to " << urlStr); break; }
      case eOmniClientConnectionStatus_Disconnected: { UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Disconnected (will attempt reconnection) to " << urlStr); break; }
      case Count_eOmniClientConnectionStatus:
      default:
        break;
      }
    }
  }

  OmniClientResult IsValidServerConnection(const char* serverUrl)
  {
    struct ServerInfoContect : public DefaultContext
    {
      std::string retVersion;
    } context;

    omniClientWait(omniClientGetServerInfo(serverUrl, &context,
      [](void* userData, OmniClientResult result, OmniClientServerInfo const * info) OMNICLIENT_NOEXCEPT
      {
        ServerInfoContect& context = *(ServerInfoContect*)(userData);

        if (context.result != eOmniClientResult_Ok)
        {
          context.result = result;
          if (result == eOmniClientResult_Ok && info->version)
          {
            context.retVersion = info->version;
          }
          context.done = true;
        }
      })
    );

    return context.result;
  }

  void UsdBridgeSetConnectionLogLevel(int logLevel)
  {
    int clampedLevel = eOmniClientLogLevel_Debug + (logLevel < 0 ? 0 : logLevel);
    clampedLevel = (clampedLevel < (int)Count_eOmniClientLogLevel) ? clampedLevel : Count_eOmniClientLogLevel - 1;
    omniClientSetLogLevel((OmniClientLogLevel)clampedLevel);
  }
}

const char* UsdBridgeRemoteConnection::GetBaseUrl() const
{
  return Internals->BaseUrlBuffer;
}

const char* UsdBridgeRemoteConnection::GetUrl(const char* path) const
{
  size_t parsedBufSize = Internals->MaxBaseUrlSize;
  char* combinedUrl = omniClientCombineUrls(Internals->BaseUrlBuffer, path, Internals->TempUrlBuffer, &parsedBufSize);

  return combinedUrl;
}

bool UsdBridgeRemoteConnection::Initialize(const UsdBridgeConnectionSettings& settings,
  UsdBridgeLogCallback logCallback, void* logUserData)
{
  bool initSuccess = UsdBridgeConnection::Initialize(settings, logCallback, logUserData);

  if (NumInitializedConnInstances == 0)
  {
    omniClientSetLogCallback(OmniClientStatusCB);
  }

  if (NumConnInstances == 0)
  {
    UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Establishing connection - Omniverse Client Initialization -");

    initSuccess = omniClientInitialize(kOmniClientVersion);
    UsdBridgeSetConnectionLogLevel(ConnectionLogLevel);
  }
  ++NumConnInstances;

  if (initSuccess && NumInitializedConnInstances == 0)
  {
    Internals->StatusCallbackHandle = omniClientRegisterConnectionStatusCallback(nullptr, OmniClientConnectionStatusCB);
  }

  if (initSuccess)
  {
    // Check for Url validity for its various components
    std::string serverUrl = "omniverse://" + Settings.HostName + "/";
    std::string rawUrl = serverUrl + Settings.WorkingDirectory;
    OmniClientUrl* brokenUrl = omniClientBreakUrl(rawUrl.c_str());

    if (!brokenUrl->scheme)
    {
      UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Illegal Omniverse scheme.");
      initSuccess = false;
    }

    if (!brokenUrl->host || strlen(brokenUrl->host) == 0)
    {
      UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Illegal Omniverse server.");
      initSuccess = false;
    }

    if (!brokenUrl->path || strlen(brokenUrl->path) == 0)
    {
      UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Illegal Omniverse working directory.");
      initSuccess = false;
    }

    char* urlRes = nullptr;
    if (initSuccess)
    {
      size_t parsedBufSize = Internals->MaxBaseUrlSize;
      urlRes = omniClientMakeUrl(brokenUrl, Internals->BaseUrlBuffer, &parsedBufSize);
      if (!urlRes)
      {
        UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Connection Url invalid, exceeds 4096 characters.");
        initSuccess = false;
      }
    }

    if (initSuccess)
    {
      OmniClientResult result = IsValidServerConnection(serverUrl.c_str());
      if (result != eOmniClientResult_Ok)
      {
        UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Server connection cannot be established, errorcode:" << omniResultToString(result));
        initSuccess = false;
      }
    }

    if (initSuccess)
    {
      bool result = this->CheckWritePermissions();
      if (!result)
      {
        UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Omniverse user does not have write permissions to selected output directory.");
        initSuccess = false;
      }
    }

    if (initSuccess)
    {
      UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Server for connection:" << brokenUrl->host);
      if (brokenUrl->port)
      {
        UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Port for connection:" << brokenUrl->port);
      }

      if (NumInitializedConnInstances == 0)
      {
        //omniClientPushBaseUrl(urlRes); // Base urls only apply to *normal* omniClient calls, not usd stage creation
      }

      ++NumInitializedConnInstances;
      ConnectionInitialized = true;
    }

    omniClientFreeUrl(brokenUrl);
  }

  return initSuccess;
}

void UsdBridgeRemoteConnection::Shutdown()
{
  if (ConnectionInitialized && --NumInitializedConnInstances == 0)
  {
    omniClientSetLogCallback(nullptr);
    if (Internals->StatusCallbackHandle)
      omniClientUnregisterCallback(Internals->StatusCallbackHandle);

    omniClientLiveWaitForPendingUpdates();

    //omniClientPopBaseUrl(Internals->BaseUrlBuffer);

    //omniClientShutdown();
  }
}

int UsdBridgeRemoteConnection::MaxSessionNr() const
{
  struct SessionListContext : public DefaultContext
  {
    int sessionNumber = -1;
  } context;
  const char* baseUrl = this->GetBaseUrl();
  omniClientWait(omniClientList(
    baseUrl, &context,
    [](void* userData, OmniClientResult result, uint32_t numEntries, struct OmniClientListEntry const* entries) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(SessionListContext*)(userData);
      {
        if (result == eOmniClientResult_Ok)
        {
          for (uint32_t i = 0; i < numEntries; i++)
          {
            const char* relPath = entries[i].relativePath;
            if (strncmp("Session_", relPath, 8) == 0)
            {
              int pathSessionNr = std::atoi(relPath + 8);

              context.sessionNumber = std::max(context.sessionNumber, pathSessionNr);
            }
          }
        }
        context.done = true;
      }
    })
  );

  return context.sessionNumber;
}

bool UsdBridgeRemoteConnection::CreateFolder(const char* dirName, bool isRelative, bool mayExist) const
{
  DefaultContext context;

  const char* dirUrl = isRelative ? this->GetUrl(dirName) : dirName;
  omniClientWait(omniClientCreateFolder(dirUrl, &context,
    [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest
    || (mayExist && context.result == eOmniClientResult_ErrorAlreadyExists);
}

bool UsdBridgeRemoteConnection::RemoveFolder(const char* dirName, bool isRelative) const
{
  DefaultContext context;

  const char* dirUrl = isRelative ? this->GetUrl(dirName) : dirName;
  omniClientWait(omniClientDelete(dirUrl, &context,
    [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest;
}

bool UsdBridgeRemoteConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary) const
{
  (void)binary;
  UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Copying data to: " << filePath);

  DefaultContext context;

  const char* fileUrl = isRelative ? this->GetUrl(filePath) : filePath;
  OmniClientContent omniContent{ (void*)data, dataSize, nullptr };
  omniClientWait(omniClientWriteFile(fileUrl, &omniContent, &context, [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest;
}

bool UsdBridgeRemoteConnection::RemoveFile(const char* filePath, bool isRelative) const
{
  DefaultContext context;
  UsdBridgeLogMacro(UsdBridgeLogLevel::STATUS, "Removing file: " << filePath);

  const char* fileUrl = isRelative ? this->GetUrl(filePath) : filePath;
  omniClientWait(omniClientDelete(fileUrl, &context, [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest;
}

bool UsdBridgeRemoteConnection::ProcessUpdates()
{
  omniClientLiveProcess();

  return true;
}

bool UsdBridgeRemoteConnection::CheckWritePermissions()
{
  bool success = this->CreateFolder("", true, true);
  return success;
}

void UsdBridgeRemoteConnection::SetConnectionLogLevel(int logLevel)
{
  ConnectionLogLevel = logLevel;
  if (NumConnInstances > 0)
  {
    UsdBridgeSetConnectionLogLevel(logLevel);
  }
}

int UsdBridgeRemoteConnection::GetConnectionLogLevelMax()
{
  return Count_eOmniClientLogLevel - 1;
}

#else

const char* UsdBridgeRemoteConnection::GetBaseUrl() const
{
  return UsdBridgeConnection::GetBaseUrl();
}

const char* UsdBridgeRemoteConnection::GetUrl(const char* path) const
{
  return UsdBridgeConnection::GetUrl(path);
}

bool UsdBridgeRemoteConnection::Initialize(const UsdBridgeConnectionSettings& settings,
  UsdBridgeLogCallback logCallback, void* logUserData)
{
  return UsdBridgeConnection::Initialize(settings, logCallback, logUserData);
}

void UsdBridgeRemoteConnection::Shutdown()
{
  UsdBridgeConnection::Shutdown();
}

int UsdBridgeRemoteConnection::MaxSessionNr() const
{
  return UsdBridgeConnection::MaxSessionNr();
}

bool UsdBridgeRemoteConnection::CreateFolder(const char* dirName, bool isRelative, bool mayExist) const
{
  return UsdBridgeConnection::CreateFolder(dirName, isRelative, mayExist);
}

bool UsdBridgeRemoteConnection::RemoveFolder(const char* dirName, bool isRelative) const
{
  return UsdBridgeConnection::RemoveFolder(dirName, isRelative);
}

bool UsdBridgeRemoteConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary) const
{
  return UsdBridgeConnection::WriteFile(data, dataSize, filePath, isRelative, binary);
}

bool UsdBridgeRemoteConnection::RemoveFile(const char* filePath, bool isRelative) const
{
  return UsdBridgeConnection::RemoveFile(filePath, isRelative);
}

bool UsdBridgeRemoteConnection::ProcessUpdates()
{
  UsdBridgeConnection::ProcessUpdates();

  return true;
}

bool UsdBridgeRemoteConnection::CheckWritePermissions()
{
  return true;
}

void UsdBridgeRemoteConnection::SetConnectionLogLevel(int logLevel)
{
}

int UsdBridgeRemoteConnection::GetConnectionLogLevelMax()
{
  return 0;
}

#endif //OMNIVERSE_CONNECTION_ENABLE


UsdBridgeLocalConnection::UsdBridgeLocalConnection()
{
}

UsdBridgeLocalConnection::~UsdBridgeLocalConnection()
{
}

const char* UsdBridgeLocalConnection::GetBaseUrl() const
{
  return UsdBridgeConnection::GetBaseUrl();
}

const char* UsdBridgeLocalConnection::GetUrl(const char* path) const
{
  return UsdBridgeConnection::GetUrl(path);
}

bool UsdBridgeLocalConnection::Initialize(const UsdBridgeConnectionSettings& settings, UsdBridgeLogCallback logCallback, void* logUserData)
{
  bool initSuccess = UsdBridgeConnection::Initialize(settings, logCallback, logUserData);
  if (initSuccess)
  {
    if (settings.WorkingDirectory.length() == 0)
    {
      UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Local Output Directory not set, cannot initialize output.");
      return false;
    }

    bool workingDirExists = fs::exists(settings.WorkingDirectory);
    if (!workingDirExists)
      workingDirExists = fs::create_directory(settings.WorkingDirectory);

    if (!workingDirExists)
    {
      UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Cannot create Local Output Directory, are permissions set correctly?");
      return false;
    }
    else
    {
      return true;
    }
  }
  return initSuccess;
}

void UsdBridgeLocalConnection::Shutdown()
{
  UsdBridgeConnection::Shutdown();
}

int UsdBridgeLocalConnection::MaxSessionNr() const
{
  return UsdBridgeConnection::MaxSessionNr();
}

bool UsdBridgeLocalConnection::CreateFolder(const char* dirName, bool isRelative, bool mayExist) const
{
  return UsdBridgeConnection::CreateFolder(dirName, isRelative, mayExist);
}

bool UsdBridgeLocalConnection::RemoveFolder(const char* dirName, bool isRelative) const
{
  return UsdBridgeConnection::RemoveFolder(dirName, isRelative);
}

bool UsdBridgeLocalConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary) const
{
  return UsdBridgeConnection::WriteFile(data, dataSize, filePath, isRelative, binary);
}

bool UsdBridgeLocalConnection::RemoveFile(const char* filePath, bool isRelative) const
{
  return UsdBridgeConnection::RemoveFile(filePath, isRelative);
}

bool UsdBridgeLocalConnection::ProcessUpdates()
{
  UsdBridgeConnection::ProcessUpdates();

  return true;
}


UsdBridgeVoidConnection::UsdBridgeVoidConnection()
{
}

UsdBridgeVoidConnection::~UsdBridgeVoidConnection()
{
}

const char* UsdBridgeVoidConnection::GetBaseUrl() const
{
  return UsdBridgeConnection::GetBaseUrl();
}

const char* UsdBridgeVoidConnection::GetUrl(const char* path) const
{
  return UsdBridgeConnection::GetUrl(path);
}

bool UsdBridgeVoidConnection::Initialize(const UsdBridgeConnectionSettings& settings, UsdBridgeLogCallback logCallback, void* logUserData)
{
  bool initialized = UsdBridgeConnection::Initialize(settings, logCallback, logUserData);
  Settings.WorkingDirectory = "./";// Force relative working directory in case of unforeseen usd saves based on GetUrl()
  return initialized;
}

void UsdBridgeVoidConnection::Shutdown()
{
  UsdBridgeConnection::Shutdown();
}

int UsdBridgeVoidConnection::MaxSessionNr() const
{
  return -1;
}

bool UsdBridgeVoidConnection::CreateFolder(const char* dirName, bool isRelative, bool mayExist) const
{
  return true;
}

bool UsdBridgeVoidConnection::RemoveFolder(const char* dirName, bool isRelative) const
{
  return true;
}

bool UsdBridgeVoidConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool isRelative, bool binary) const
{
  return true;
}

bool UsdBridgeVoidConnection::RemoveFile(const char* filePath, bool isRelative) const
{
  return true;
}

bool UsdBridgeVoidConnection::ProcessUpdates()
{
  return true;
}