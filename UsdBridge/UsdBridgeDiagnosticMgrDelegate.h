// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeDiagnosticMgrDelegate_h
#define UsdBridgeDiagnosticMgrDelegate_h

#include "UsdBridgeData.h"

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

#endif