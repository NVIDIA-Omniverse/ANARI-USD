// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#define USE_USD_GEOM_POINTS

#if defined(USE_FABRIC) && !defined(USE_USDRT)
#define USE_USDRT // Force USDRT when Fabric is enabled
#endif

#define OMNIVERSE_CREATE_WORKAROUNDS
//#define CUSTOM_PBR_MDL
#define USE_INDEX_MATERIALS

#ifdef USE_USDRT
// UsdRt cannot use composition elements like sublayers,
// changes don't propagate, so external stages just replace the existing scenestage
#define REPLACE_SCENE_BY_EXTERNAL_STAGE
#endif

// To enable output that usdview can digest (just a single float)
//#define USDBRIDGE_VOL_FLOAT1_OUTPUT

#if defined(TIME_CLIP_STAGES) && !defined(VALUE_CLIP_RETIMING)
#define VALUE_CLIP_RETIMING
#endif

#if defined(VALUE_CLIP_RETIMING) && !defined(TIME_BASED_CACHING)
#define TIME_BASED_CACHING
#endif

#define USDBRIDGE_MAX_LOG_VERBOSITY 4