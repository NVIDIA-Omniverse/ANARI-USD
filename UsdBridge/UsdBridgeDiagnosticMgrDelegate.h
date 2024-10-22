// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeDiagnosticMgrDelegate_h
#define UsdBridgeDiagnosticMgrDelegate_h

#include "UsdBridgeData.h"

#ifdef USE_USDRT
#include "carb/logging/ILogging.h"
#include "carb/logging/Logger.h"
#endif

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

class UsdBridgeDiagnosticMgrDelegate : public TfDiagnosticMgr::Delegate
{
  public:
    UsdBridgeDiagnosticMgrDelegate(void* logUserData,
      UsdBridgeLogCallback logCallback);

    void IssueError(TfError const& err) override;

    void IssueFatalError(TfCallContext const& context,
      std::string const& msg) override;
 
    void IssueStatus(TfStatus const& status) override;

    void IssueWarning(TfWarning const& warning) override;

    static void SetOutputEnabled(bool enable){ OutputEnabled = enable; }

  protected:

    void LogTfMessage(UsdBridgeLogLevel level, TfDiagnosticBase const& diagBase);

    void LogMessage(UsdBridgeLogLevel level, const std::string& message);

    void* LogUserData;
    UsdBridgeLogCallback LogCallback;
    static bool OutputEnabled;
};

#ifdef USE_USDRT
struct UsdBridgeCarbLogger : public carb::logging::Logger
{
  UsdBridgeCarbLogger();

  ~UsdBridgeCarbLogger();

  static void CarbLogCallback(carb::logging::Logger* logger,
    const char* source,
    int32_t level,
    const char* filename,
    const char* functionName,
    int lineNumber,
    const char* message);

  static void SetCarbLogVerbosity(int logVerbosity);

  carb::logging::ILogging* CarbLogIface = nullptr;
};
#endif

#endif