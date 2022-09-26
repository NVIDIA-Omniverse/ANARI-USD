// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeTimeEvaluator.h"
#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

const UsdTimeCode UsdBridgeTimeEvaluator<bool>::DefaultTime = UsdTimeCode::Default();