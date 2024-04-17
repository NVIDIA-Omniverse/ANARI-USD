# Copyright 2021-2024 The Khronos Group
# SPDX-License-Identifier: Apache-2.0

import numpy as np
from anari import *


# Data arrays for the scene
width = 1024
height = 768

cam_pos = [0.0, 0.0, -100.0]
cam_up = [0.0, 1.0, 0.0]
cam_view = [0.1, 0.0, 1.0]

vertex = np.array([
  -1.0, -1.0, 3.0,
  -1.0,  1.0, 3.0,
   1.0, -1.0, 3.0,
   0.1,  0.1, 0.3
], dtype = np.float32)

color =  np.array([
  0.9, 0.5, 0.5, 1.0,
  0.8, 0.8, 0.8, 1.0,
  0.8, 0.8, 0.8, 1.0,
  0.5, 0.9, 0.5, 1.0
], dtype = np.float32)

orientation =  np.array([
  0.0, 0.0, 0.0, 1.0,
  0.0, 0.707031, 0.0, 0.707031,
  -0.707031, 0.00282288, 0.0, 0.707031,
  0.96582, 0.00946808, 0.0, 0.258789
], dtype = np.float32) # These are 4-component quaternions, with the real component at the end (IJKW). So the first entry is the identity rotation.

scale =  np.array([
  2.0, 0.5, 0.3, 1.0
], dtype = np.float32) # Simple scalars for uniform scaling

index = np.array([
  0, 1, 2, 3
], dtype = np.uint32)


# Logging function definitions
prefixes = {
    lib.ANARI_SEVERITY_FATAL_ERROR : "FATAL",
    lib.ANARI_SEVERITY_ERROR : "ERROR",
    lib.ANARI_SEVERITY_WARNING : "WARNING",
    lib.ANARI_SEVERITY_PERFORMANCE_WARNING : "PERFORMANCE",
    lib.ANARI_SEVERITY_INFO : "INFO",
    lib.ANARI_SEVERITY_DEBUG : "DEBUG"
}

def anari_status(device, source, sourceType, severity, code, message):
    print('[%s]: '%prefixes[severity]+message)

status_handle = ffi.new_handle(anari_status) #something needs to keep this handle alive


# Loading the USD library and creating the device
usdLibrary = anariLoadLibrary('usd', status_handle)
usdDevice = anariNewDevice(usdLibrary, 'usd')


# Setting the USD device parameters, which determine where the USD output goes
usdOutputBinary = 0 # ANARI's spec defines BOOL and INT32 to be of equal size, so playing it safe here by using an explicit int
usdPreviewSurface = 0
anariSetParameter(usdDevice, usdDevice, 'usd::serialize.location', ANARI_STRING, './'); # Location on disk, replace with any desired path
anariSetParameter(usdDevice, usdDevice, 'usd::serialize.outputBinary', ANARI_BOOL, usdOutputBinary); # Output binary files (smaller) or ascii output (for debugging)
anariSetParameter(usdDevice, usdDevice, 'usd::output.previewSurfaceShader', ANARI_BOOL, usdPreviewSurface); # Materials that work for rendering in non-nvidia USD clients are disabled for now

# Nucleus specific device parameters; uncomment if output to Nucleus is desired
#usdConnectionLogVerbosity = 0
#anariSetParameter(usdDevice, usdDevice, 'usd::serialize.hostName', ANARI_STRING, 'localhost'); # Used in case Nucleus output is desired
#anariSetParameter(usdDevice, usdDevice, 'usd::connection.logVerbosity', ANARI_INT32, usdConnectionLogVerbosity); # In case of Nucleus connection, set connection log verbosity from [0-4], with the highest value being the most verbose

anariCommitParameters(usdDevice, usdDevice)


# The USD device is wrapped within a debug device, for additional sanity checking of the API usage
# All further ANARI calls use the debug device, which internally routes the calls through to the usdDevice
debugLibrary = anariLoadLibrary('debug', status_handle)
debugDevice = anariNewDevice(debugLibrary, 'debug')
anariSetParameter(debugDevice, debugDevice, 'wrappedDevice', ANARI_DEVICE, usdDevice)
anariSetParameter(debugDevice, debugDevice, 'traceMode', ANARI_STRING, 'code')
anariCommitParameters(debugDevice, debugDevice)


# Choose either the debug or the usd device
device = usdDevice


# Setting up the scene objects, with a camera, light, world and the glyph geometry with a basic material.
camera = anariNewCamera(device, 'perspective')
anariSetParameter(device, camera, 'aspect', ANARI_FLOAT32, width/height)
anariSetParameter(device, camera, 'position', ANARI_FLOAT32_VEC3, cam_pos)
anariSetParameter(device, camera, 'direction', ANARI_FLOAT32_VEC3, cam_view)
anariSetParameter(device, camera, 'up', ANARI_FLOAT32_VEC3, cam_up)
anariCommitParameters(device, camera)

world = anariNewWorld(device)

glyphs = anariNewGeometry(device, 'glyph')

anariSetParameter(device, glyphs, 'shapeType', ANARI_STRING, 'cone') # The shape of the glyph can be specified here - 'sphere/cylinder/cone' are supported, a custom shape is a bit more involved 

array = anariNewArray1D(device, ffi.from_buffer(vertex), ANARI_FLOAT32_VEC3, 4)
anariSetParameter(device, glyphs, 'vertex.position', ANARI_ARRAY1D, array)

array = anariNewArray1D(device, ffi.from_buffer(color), ANARI_FLOAT32_VEC4, 4)
anariSetParameter(device, glyphs, 'vertex.color', ANARI_ARRAY1D, array)

array = anariNewArray1D(device, ffi.from_buffer(orientation), ANARI_FLOAT32_QUAT_IJKW, 4)
anariSetParameter(device, glyphs, 'vertex.orientation', ANARI_ARRAY1D, array)

array = anariNewArray1D(device, ffi.from_buffer(scale), ANARI_FLOAT32, 4) # Could also be ANARI_FLOAT32_VEC3 - a different scale for every axis
anariSetParameter(device, glyphs, 'vertex.scale', ANARI_ARRAY1D, array)

array = anariNewArray1D(device, ffi.from_buffer(index), ANARI_UINT32, 4) # The index array can optionally be left out completely
anariSetParameter(device, glyphs, 'primitive.index', ANARI_ARRAY1D, array)

anariCommitParameters(device, glyphs)

material = anariNewMaterial(device, 'matte') # If a particular look is required, consider changing to 'physicallyBased' and look for parameters in https://registry.khronos.org/ANARI/specs/1.0/ANARI-1.0.html#object_types_material_physically_based
anariCommitParameters(device, material)

surface = anariNewSurface(device)
anariSetParameter(device, surface, 'geometry', ANARI_GEOMETRY, glyphs)
anariSetParameter(device, surface, 'material', ANARI_MATERIAL, material)
anariCommitParameters(device, surface)

surfaces = ffi.new('ANARISurface[]', [surface])
array = anariNewArray1D(device, surfaces, ANARI_SURFACE, 1)
anariSetParameter(device, world, 'surface', ANARI_ARRAY1D, array)

light = anariNewLight(device, 'directional')
anariCommitParameters(device, light)

lights = ffi.new('ANARILight[]', [light])
array = anariNewArray1D(device, lights, ANARI_LIGHT, 1)
anariSetParameter(device, world, 'light', ANARI_ARRAY1D, array)

anariCommitParameters(device, world)


# Creating the frame dummy object required to call 'anariRenderFrame'
frame = anariNewFrame(device)
anariCommitParameters(device, frame)


# AnariRenderFrame is where the USD output is actually written
anariRenderFrame(device, frame)
anariFrameReady(device, frame, ANARI_WAIT)