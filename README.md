## USD device for ANARI #

Device for ANARI generating USD+Omniverse output

### Prerequisites #

- If OpenVDB (Volume support) is required:
    - Easiest: build USD from source (https://github.com/PixarAnimationStudios/USD/) according to [Building USD Manually](#building-usd-manually)
    - Or build USD manually according to [Building OpenVDB Manually](#building-openvdb-manually)
    - For release/debug versions, observe the directory structure guidelines in [Debug Builds](#debug-builds) section
- If OpenVDB is not required:
    - Get prebuilt USD packages from https://developer.nvidia.com/usd#bin
- Python, version *equal* to the one used to build USD (refer to the download link in case of the prebuilt binaries)
- For Linux, use GCC-9 or higher

### Building the ANARI USD device #

1. Run ANARI's CMake with
    - `ANARI_DEVICE_USD` enabled
    - `USD_INSTALL_DIR` to the directory containing the `/include` and `/lib` subfolders (or `/debug` and `/release`, see [Debug Builds](#debug-builds)).
2. Set `USD_PYTHON_INSTALL_DIR` to the Python version with which the USD binaries were built (with `/include` and `/lib` subfolders).
3. (Optional) Set `USD_DEVICE_USE_OPENVDB` for OpenVDB volume support
    - On Linux, this will also require setting `ZLIB_INSTALL_DIR` to the installation used for building USD in [Building USD Manually](#building-usd-manually)
4. (Optional) If using a manual OpenVDB build from [Building OpenVDB Manually](#building-openvdb-manually) (otherwise, skip this step):
    - Unset `USE_USD_OPENVDB_BUILD`
    - Set `OPENVDB_INSTALL_DIR` to the directory containing the `/include` and `/lib` subfolders (or `/debug` and `/release`, see [Debug Builds](#debug-builds)).
    - Do the same for `OPENEXR_INSTALL_DIR`, `BLOSC_INSTALL_DIR` and `ZLIB_INSTALL_DIR`.
5. (Optional) If Omniverse support is required, set `USD_DEVICE_USE_OMNIVERSE`
    - Follow the guide on how to download the omniverse libraries at [Downloading the Omniverse libraries](#downloading-the-omniverse-libraries)
    - Set `USD_INSTALL_DIR`, `OMNI_CLIENT_INSTALL_DIR` and `USD_PYTHON_DIR` according to the guide's directions
    - Make sure to add `<OMNI_CLIENT_INSTALL_DIR>/<release/debug>` to the executable's environment when running code
6. Configure and Generate in Cmake, then build ANARI
7. On Windows, to successfully load the device:
    - Make sure all .dlls in `<USD_INSTALL_DIR>/lib` and `<USD_INSTALL_DIR>/bin` are visible/copied to the executable's environment.
    - Also include the `lib/python` and `lib/usd` subfolders to that environment
    - Lastly, copy over the pythonXX dlls if python is not in the path.

### Usage notes #

- Device name is `usd`
- All device-specific parameters are prefixed with `usd::`
- Set device `usd::serialize.location` string to the output location, `usd::serialize.outputBinary` bool for binary or text output. These parameters are **immutable**.
- Each ANARI scene object has a `name` parameter as scenegraph identifier (over time). Upon setting this name, a formatted version is stored in the `usd::name` parameter.
- Each ANARI scene object has a `usd::timestep` parameter to define the time at which `commit()` will add the data to the scenegraph object indicated by `usd::name`.
- For ANARI object parameters that should be constant over all timesteps, unset their corresponding bits in the `usd::timeVarying` parameter specific to each ANARI scene object (see headers). This parameter is **immutable**.
- Changes to data are **actually saved** when `anariRenderFrame()` is called.
- If ANARI objects of a certain `name` are not referenced from within any committed timestep, their internal data is only cleaned up when calling `anariDeviceSetParam(d, "usd::garbagecollect", ANARI_VOID_POINTER, 0)`. This is adviced after every `anariRenderFrame()` or a subfrequency thereof.
- Not supported:
    - Geometries: 
        - quads and cones
        - `*.attribute` parameters larger than 0 (`*.texcoord` is still supported)
        - color array type other than double/float (so no fixed types)
        - strided arrays
    - Volumes
        - `color/opacity.position` parameters
    - Materials:
        - `obj` materials, just use `matte` and `transparentMatte`
        - string/sampler parameters, except `map_kd` which remains as texture sampler (set `usevertexcolors` for vertex coloring - for full parameter list see `UsdMaterial.cpp`)
    - Samplers:
        - anything other than `filename` argument for texture source, with `wrapmode1/wrapmode2`
    - Lights
        - completely unsupported
    - World
        - direct surface/volume parameters
- Examples in `examples/anariTutorial_usd(_time).c`

#### Debug builds #

If you have separate release and debug versions of USD, (or standalone OpenVDB, Blosc, Zlib), make sure the `/lib` and `/include` directories of the respective installations exist within `/release` and `/debug` directories, ie. for USD: `<USD_INSTALL_DIR>/release` and `<USD_INSTALL_DIR>/debug`.

#### Building USD Manually #

- The environment variable BOOST_ROOT should *not* be set
- On Linux:
    - Zlib has to either be installed on a system level or built from source (with `-fPIC` added to `CMAKE_CXX_FLAGS`)
    - If using Anaconda, the pyside2 and pyopengl package has to be included in the environment
    - If using Anaconda, make sure a Python include directory without `m` suffix exists (create a symbolic link if necessary)
    - Run, for a particular value of `<config>`: `export LD_LIBRARY_PATH=<USD_INSTALL_DIR>/<config>/lib:$LD_LIBRARY_PATH`
    - Run, for a particular value of `<zlib_install_dir>`: `export ZLIB_INSTALL_DIR=<zlib_install_dir>` (don't add the configuration)
    - Run `export CMAKE_ZLIB_ARGS="-D ZLIB_INCLUDE_DIR:PATH=$ZLIB_INSTALL_DIR/release/include -D ZLIB_LIBRARY_DEBUG:FILEPATH=$ZLIB_INSTALL_DIR/debug/lib/libz.a -D ZLIB_LIBRARY_RELEASE:FILEPATH=$ZLIB_INSTALL_DIR/release/lib/libz.a"`
- On Windows, **only** for debug builds:
    - In pyconfig.h all `pragma comment(lib,"pythonXX.lib")` statements should change to `pragma comment(lib,"pythonXX_d.lib")`. They can be changed back after building USD.
- Build and install USD by running the buildscript `python ./build_scripts/build_usd.py <USD_INSTALL_DIR>/<config>`
    - For OpenVDB, add the --openvdb flag
    - Add --debug for debug builds
    - On Windows:
        - run **in a developer command prompt** and add the following flag: `--build-args openvdb,"-DCMAKE_CXX_FLAGS=\"-D__TBB_NO_IMPLICIT_LINKAGE /EHsc\""`
        - For a debug build:
            - The build will likely still fail in openvdb, because ILMBASE_HALF_LIBRARY/DLL is not set. Manually set them to `<USD_INSTALL_DIR>/<config>/lib/Half-X_X(_d).lib` and `<USD_INSTALL_DIR>/<config>/bin/Half-X_X(_d).dll` by using cmake on the `<USD_INSTALL_DIR>/<config>/build/openvdb-X.X.X` directory and press configure. This will fail, but afterwards just rerun the usd install command.
            - The same procedure has to be performed for the OpenEXR libraries in the `<USD_INSTALL_DIR>/<config>/build/USD` project. Just choose the dynamic `.lib` files, configure, then rerun the original usd install command.
    - On Linux, add the following flag: `--build-args openvdb,"-D CMAKE_CXX_FLAGS:STRING=-fPIC $CMAKE_ZLIB_ARGS" blosc,"-D CMAKE_C_FLAGS:STRING=-fPIC -D CMAKE_CXX_FLAGS:STRING=-fPIC" openexr,"$CMAKE_ZLIB_ARGS"`
- On Windows: After install, make sure the `boost_<version>` subfolder in the `/include` directory of the install is removed from the path altogether. So `<install_dir>/include/boost-1_70/boost/..` should become `<USD_INSTALL_DIR>/include/boost/..`

#### Building OpenVDB Manually #

If for some reason OpenVDB should be built outside of the USD build script, perform the following steps to build as a static library:
- Get OpenEXR (https://github.com/AcademySoftwareFoundation/openexr/releases), Blosc 1.5.0 (https://github.com/Blosc/c-blosc/tree/v1.5.0), Zlib (https://zlib.net/) and Boost (https://boost.org).
- Build Boost
- Build only a static IlmBase project (`ILMBASE_BUILD_BOTH_STATIC_SHARED` in cmake) from OpenEXR (https://github.com/AcademySoftwareFoundation/openexr/releases)
- Build static versions of Blosc (https://github.com/Blosc/c-blosc/archive/master.zip) and Zlib (https://zlib.net/)
    - On Linux, make sure to add `-fPIC` to both `CMAKE_CXX_FLAGS` and `CMAKE_C_FLAGS` where possible for both libraries
- Build OpenVDB:
    - Set `OPEN_VDB_CORE_STATIC` and `USE_STATIC_DEPENDENCIES`, then unset `OPEN_VDB_CORE_SHARED` and `OPENVDB_BUILD_VDB_PRINT` in cmake
    - Add `DOPENVDB_STATICLIB` to `CMAKE_CXX_FLAGS`
    - On Linux, also add `-fPIC` to `CMAKE_CXX_FLAGS`
    - The `ILMBASE_HALF_LIBRARY`, `ZLIB_LIBRARY` and `Blosc_LIBRARY` dependencies should point to the static libraries built before
    - `BOOST_DIR` should point to `<boost_install>/lib/cmake/Boost-<version>`
    - Make sure that after configure, the various libraries will point to the static versions
    - For `Tbb_INCLUDE_DIR` and `Tbb_Tbb_LIBRARY`, simply point to the tbb include folder and library of the usd install

#### Downloading the Omniverse libraries #

- Get the Nvidia Omniverse Launcher from https://www.nvidia.com/en-us/omniverse/
- Go to the `Exchange` tab and navigate to the `Connectors` section. Install the Connect Sample.
- Go to the install directory of the connect sample, which is typically in `~/.local/share/ov/pkg/` or `<userdir>/AppData/local/ov/pkg/` but can also be found in the launcher by looking at your installed apps, a triple-stack on the right side of the connector sample entry -> settings
- Build the connect sample by running `build.sh/.bat` in its folder
- Locate the `nv_usd`, `omni_client_library` and `python` folders in the `_build/target-deps` subfolder
- The location of the previous three subfolders respectively can directly be set as `USD_INSTALL_DIR`, `OMNI_CLIENT_INSTALL_DIR`, `USD_PYTHON_INSTALL_DIR` in the ANARI CMake configuration