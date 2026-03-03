// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeGLContext_h
#define UsdBridgeGLContext_h

// Ensures an offscreen OpenGL context is available on the current thread.
// HdStorm (via HgiGL) requires OpenGL 4.5 to be initialized before use.
// Since this is a library (not an application with its own window), we
// create a minimal offscreen context if none exists.
//
// Thread safety: must be called from the thread that will do rendering.
// The context is created once and reused for subsequent calls.
bool EnsureOpenGLContext();

#endif
