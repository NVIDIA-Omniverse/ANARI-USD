
// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeDiagnosticMgrDelegate.h"

#ifdef USE_USDRT
#include "carb/ClientUtils.h"
#endif

bool UsdBridgeDiagnosticMgrDelegate::OutputEnabled = false;

UsdBridgeDiagnosticMgrDelegate::UsdBridgeDiagnosticMgrDelegate(void* logUserData, UsdBridgeLogCallback logCallback)
    : LogUserData(logUserData)
    , LogCallback(logCallback)
{}

void UsdBridgeDiagnosticMgrDelegate::IssueError(TfError const& err)
{
    LogTfMessage(UsdBridgeLogLevel::ERR, err);
}

void UsdBridgeDiagnosticMgrDelegate::IssueFatalError(TfCallContext const& context,
    std::string const& msg)
{
    std::string message = TfStringPrintf(
    "[USD Internal error]: %s in %s at line %zu of %s",
    msg.c_str(), context.GetFunction(), context.GetLine(), context.GetFile()
    );
    LogMessage(UsdBridgeLogLevel::ERR, message);
}

void UsdBridgeDiagnosticMgrDelegate::IssueStatus(TfStatus const& status)
{
    LogTfMessage(UsdBridgeLogLevel::STATUS, status);
}

void UsdBridgeDiagnosticMgrDelegate::IssueWarning(TfWarning const& warning)
{
    LogTfMessage(UsdBridgeLogLevel::WARNING, warning);
}

void UsdBridgeDiagnosticMgrDelegate::LogTfMessage(UsdBridgeLogLevel level, TfDiagnosticBase const& diagBase)
{
    std::string message = TfStringPrintf(
    "[USD Internal Message]: %s with error code %s in %s at line %zu of %s",
    diagBase.GetCommentary().c_str(),
    TfDiagnosticMgr::GetCodeName(diagBase.GetDiagnosticCode()).c_str(),
    diagBase.GetContext().GetFunction(),
    diagBase.GetContext().GetLine(),
    diagBase.GetContext().GetFile()
    );

    LogMessage(level, message);
}

void UsdBridgeDiagnosticMgrDelegate::LogMessage(UsdBridgeLogLevel level, const std::string& message)
{
    if(OutputEnabled)
    LogCallback(level, LogUserData, message.c_str());
}

#ifdef USE_USDRT
UsdBridgeCarbLogger::UsdBridgeCarbLogger()
{
  this->handleMessage = CarbLogCallback;
  if(carb::Framework* framework = carb::getFramework())
  {
    if(CarbLogIface = framework->acquireInterface<carb::logging::ILogging>())
    {
      CarbLogIface->addLogger(this);
      CarbLogIface->setLogEnabled(true);
      CarbLogIface->setLevelThreshold(carb::logging::kLevelVerbose);
    }
  }
}

UsdBridgeCarbLogger::~UsdBridgeCarbLogger()
{
  if(CarbLogIface)
  {
    CarbLogIface->removeLogger(this);
    CarbLogIface = nullptr;
  }
}

void UsdBridgeCarbLogger::CarbLogCallback(carb::logging::Logger* logger,
  const char* source,
  int32_t level,
  const char* filename,
  const char* functionName,
  int lineNumber,
  const char* message)
{
}

void UsdBridgeCarbLogger::SetCarbLogVerbosity(int logVerbosity)
{
  carb::Framework* framework = carb::getFramework();
  carb::logging::ILogging* logIface = framework ? framework->tryAcquireInterface<carb::logging::ILogging>() : nullptr;
  if(logIface)
  {
    logIface->setLevelThreshold(
      std::max(carb::logging::kLevelVerbose, carb::logging::kLevelFatal - logVerbosity) );
  }
}

#endif