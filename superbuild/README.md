# CMake superbuild for USD ANARI Device

This CMake script runs stand alone to optionally build together any of:

- ANARIUsd Device (parent directory)
- USD + dependencies
- ANARI-SDK

The result of building this project is all the contents of the above (if built)
installed to `CMAKE_INSTALL_PREFIX`.

## How to build

Run CMake (3.16+) on this directory from an empty build directory. This might
look like:

```bash
% mkdir build
% cd build
% cmake /path/to/superbuild
```

You can use tools like `ccmake` or `cmake-gui` to see what options you have. The
following variables control which items are to be built:

- `BUILD_USD`: build USD + its dependencies
- `BUILD_ANARI_SDK` : build the [ANARI-SDK](https://github.com/KhronosGroup/ANARI-SDK)
- `BUILD_ANARI_USD_DEVICE`: build the root

If `BUILD_USD` or `BUILD_ANARI_SDK` are set to `OFF`, then those dependencies
will need to be found on in the host environment. Use `CMAKE_PREFIX_PATH` to
point to your USD + ANARI-SDK installations respectively.

The `BUILD_ANARI_USD_DEVICE` option lets you turn off building the device if you
only want to build the device's dependencies (mostly this is for developers).

You can then invoke the build with:

```bash
% cmake --build .
```

The resulting `install/` directory will contain everything that was built. You
can change the location of this install by setting `CMAKE_INSTALL_PREFIX`.
