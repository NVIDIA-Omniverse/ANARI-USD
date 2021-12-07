// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#define TIME_BASED_CACHING // Timesamples for timevarying properties, but no retiming (so all timesteps are global)
#define SUPPORT_MDL_SHADERS

#ifdef TIME_BASED_CACHING
#define VALUE_CLIP_RETIMING // Retiming of timesteps through value clips, for selected objects
#endif

#ifdef VALUE_CLIP_RETIMING
#define TIME_CLIP_STAGES // Separate clip stages for each timestep, for selected objects
#endif

#define USE_USD_GEOM_POINTS

// To enable output that usdview can digest (just a single float)
//#define USDBRIDGE_VOL_FLOAT1_OUTPUT