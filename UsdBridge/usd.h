// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#pragma warning(push)
#pragma warning(disable:4244) // = Conversion from double to float / int to float
#pragma warning(disable:4305) // argument truncation from double to float
#pragma warning(disable:4800) // int to bool
#pragma warning(disable:4996) // call to std::copy with parameters that may be unsafe
#define NOMINMAX // Make sure nobody #defines min or max
// Python must be included first because it monkeys with macros that cause
// TBB to fail to compile in debug mode if TBB is included before Python
#include <boost/python/object.hpp>
#include <pxr/pxr.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/trace/reporter.h>
#include <pxr/base/trace/trace.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/gf/range3f.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/clipsAPI.h>
#include <pxr/usd/usd/inherits.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdVol/volume.h>
#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/kind/registry.h>
#pragma warning(pop)
