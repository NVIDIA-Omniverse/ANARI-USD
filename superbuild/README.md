# CMake superbuild for USD ANARI Device

This CMake script runs stand alone to optionally build together any of:

- ANARIUsd Device (parent directory)
- ANARI-SDK
- USD + dependencies

The result of building this project is all the contents of the above (if built)
installed to `CMAKE_INSTALL_PREFIX`.

## Build setup

Run CMake (3.16+) on this directory from an empty build directory. This might
look like:

```bash
% mkdir _build
% cd _build
% cmake /path/to/superbuild
```

You can use tools like `ccmake` or `cmake-gui` to see what options you have. The
following variables control which items are to be built, and with which capabilities:

- `BUILD_ANARI_SDK` : build the [ANARI-SDK](https://github.com/KhronosGroup/ANARI-SDK)
- `BUILD_ANARI_USD_DEVICE`: build the root
- `BUILD_USD`: build USD + its dependencies (experimental, `OFF` by default)
    - Requires preinstalled boost + tbb in `CMAKE_PREFIX_PATH`
- `USD_DEVICE_USE_OPENVDB`: Add OpenVDB output capability for ANARI volumes to the USD device
    - Introduces `USE_USD_OPENVDB_BUILD`: If `ON` (default), OpenVDB is included within the USD installation
- `USD_DEVICE_USE_OMNIVERSE`: Add Omniverse output capability for generated USD output

If `BUILD_USD` or `BUILD_ANARI_SDK` are set to `OFF`, then those dependencies
will need to be found on in the host environment. Use `CMAKE_PREFIX_PATH` to
point to your USD + ANARI-SDK (+ OpenVDB + Omniverse) installations respectively. Alternatively,
one can use explicit install dir variables which support `debug/` and `release/` subdirs:

- `ANARI_INSTALL_DIR`: for the ANARI-SDK install directory
- `USD_INSTALL_DIR`: for the USD install directory
- `OPENVDB_INSTALL_DIR`: for the OpenVDB install directory (if not `USE_USD_OPENVDB_BUILD`)
- `OMNICLIENT_INSTALL_DIR`: for the Omniverse (OmniClient) install directory

Lastly, the `BUILD_ANARI_USD_DEVICE` option lets you turn off building the device if you
only want to build the device's dependencies (mostly this is for developers).

## Build

Once you have set up the variables according to your situation, you can invoke the build (Linux) with:

```bash
% cmake --build .
```

or open `_build/anari_usd_superbuild.sln` (Windows) and build/install any configuration from there.

The resulting `install/` directory will contain everything that was built. You
can change the location of this install by setting `CMAKE_INSTALL_PREFIX`.
