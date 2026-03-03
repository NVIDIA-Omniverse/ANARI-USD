// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeGLContext.h"

#ifdef USD_DEVICE_RENDERING_ENABLED

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#pragma comment(lib, "opengl32")

typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

static HWND s_hiddenWindow = nullptr;
static HDC s_hdc = nullptr;
static HGLRC s_hglrc = nullptr;
static bool s_initialized = false;

static LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool EnsureOpenGLContext()
{
    if (s_initialized)
        return true;

    if (wglGetCurrentContext() != nullptr)
    {
        s_initialized = true;
        return true;
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "UsdBridgeGLContextClass";
    RegisterClassA(&wc);

    s_hiddenWindow = CreateWindowExA(
        0, wc.lpszClassName, "UsdBridgeGLContext",
        0, 0, 0, 1, 1,
        nullptr, nullptr, hInstance, nullptr);
    if (!s_hiddenWindow)
        return false;

    s_hdc = GetDC(s_hiddenWindow);
    if (!s_hdc)
        return false;

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pixelFormat = ChoosePixelFormat(s_hdc, &pfd);
    if (!pixelFormat || !SetPixelFormat(s_hdc, pixelFormat, &pfd))
        return false;

    HGLRC tempContext = wglCreateContext(s_hdc);
    if (!tempContext || !wglMakeCurrent(s_hdc, tempContext))
        return false;

    auto wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    if (wglCreateContextAttribsARB)
    {
        int attribs[] =
        {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 5,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        s_hglrc = wglCreateContextAttribsARB(s_hdc, nullptr, attribs);
    }

    if (s_hglrc)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempContext);
        wglMakeCurrent(s_hdc, s_hglrc);
    }
    else
    {
        s_hglrc = tempContext;
    }

    s_initialized = true;
    return true;
}

#elif defined(__linux__)

#include <dlfcn.h>
#include <cstdint>

// EGL types and constants loaded dynamically to avoid header/link dependencies
typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef void* EGLConfig;
typedef unsigned int EGLBoolean;
typedef int32_t EGLint;

#define EGL_NO_DISPLAY   ((EGLDisplay)0)
#define EGL_NO_CONTEXT   ((EGLContext)0)
#define EGL_NO_SURFACE   ((EGLSurface)0)
#define EGL_NONE         0x3038
#define EGL_OPENGL_API   0x30A2
#define EGL_PBUFFER_BIT  0x0001
#define EGL_SURFACE_TYPE 0x3033
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_BIT   0x0008
#define EGL_RED_SIZE     0x3024
#define EGL_GREEN_SIZE   0x3023
#define EGL_BLUE_SIZE    0x3022
#define EGL_DEPTH_SIZE   0x3025
#define EGL_WIDTH        0x3057
#define EGL_HEIGHT       0x3058
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001

typedef EGLDisplay (*PFN_eglGetDisplay)(void*);
typedef EGLBoolean (*PFN_eglInitialize)(EGLDisplay, EGLint*, EGLint*);
typedef EGLBoolean (*PFN_eglChooseConfig)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
typedef EGLContext (*PFN_eglCreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
typedef EGLSurface (*PFN_eglCreatePbufferSurface)(EGLDisplay, EGLConfig, const EGLint*);
typedef EGLBoolean (*PFN_eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
typedef EGLBoolean (*PFN_eglBindAPI)(unsigned int);
typedef EGLContext (*PFN_eglGetCurrentContext)();

static bool s_initialized = false;

bool EnsureOpenGLContext()
{
    if (s_initialized)
        return true;

    void* libEGL = dlopen("libEGL.so.1", RTLD_LAZY);
    if (!libEGL)
        libEGL = dlopen("libEGL.so", RTLD_LAZY);
    if (!libEGL)
        return false;

    auto eglGetDisplay = (PFN_eglGetDisplay)dlsym(libEGL, "eglGetDisplay");
    auto eglInitialize = (PFN_eglInitialize)dlsym(libEGL, "eglInitialize");
    auto eglChooseConfig = (PFN_eglChooseConfig)dlsym(libEGL, "eglChooseConfig");
    auto eglCreateContext = (PFN_eglCreateContext)dlsym(libEGL, "eglCreateContext");
    auto eglCreatePbufferSurface = (PFN_eglCreatePbufferSurface)dlsym(libEGL, "eglCreatePbufferSurface");
    auto eglMakeCurrent = (PFN_eglMakeCurrent)dlsym(libEGL, "eglMakeCurrent");
    auto eglBindAPI = (PFN_eglBindAPI)dlsym(libEGL, "eglBindAPI");
    auto eglGetCurrentContext = (PFN_eglGetCurrentContext)dlsym(libEGL, "eglGetCurrentContext");

    if (!eglGetDisplay || !eglInitialize || !eglChooseConfig ||
        !eglCreateContext || !eglCreatePbufferSurface || !eglMakeCurrent ||
        !eglBindAPI || !eglGetCurrentContext)
        return false;

    if (eglGetCurrentContext() != EGL_NO_CONTEXT)
    {
        s_initialized = true;
        return true;
    }

    EGLDisplay display = eglGetDisplay(nullptr);
    if (display == EGL_NO_DISPLAY)
        return false;

    if (!eglInitialize(display, nullptr, nullptr))
        return false;

    eglBindAPI(EGL_OPENGL_API);

    EGLint configAttribs[] =
    {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs == 0)
        return false;

    EGLint pbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbufferAttribs);

    EGLint contextAttribs[] =
    {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT)
        return false;

    if (!eglMakeCurrent(display, surface, surface, context))
        return false;

    s_initialized = true;
    return true;
}

#else

bool EnsureOpenGLContext()
{
    return false;
}

#endif

#else // !USD_DEVICE_RENDERING_ENABLED

bool EnsureOpenGLContext()
{
    return false;
}

#endif
