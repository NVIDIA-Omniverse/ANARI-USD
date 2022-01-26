## USD device for ANARI #

Device for ANARI generating USD+Omniverse output

### Prerequisites #

- For Linux, use GCC-9 or higher
- A USD install in any of the following ways (depending on desired capabilities):
    - Get prebuilt USD packages from https://developer.nvidia.com/usd#bin (No OpenVDB or Omniverse support)
    - Get prebuilt USD + Omniverse packages from Omniverse launcher according to [Downloading the Omniverse libraries](#downloading-the-omniverse-libraries) (no OpenVDB support)
    - Automatic installation of USD as part of the superbuild, see `superbuild/README.md` (Experimental, no Omniverse support)
    - Build USD from source (https://github.com/PixarAnimationStudios/USD/) with OpenVDB support according to [Building USD Manually](#building-usd-manually) (no Omniverse support)
        - For release/debug versions, observe the directory structure guidelines in [Debug Builds](#debug-builds) section

### Building the ANARI USD device #

Follow the instructions from `superbuild/README.md` to build and install a superbuild. Typically this requires setting a `USD_INSTALL_DIR` to the directory containing 
the `/include` and `/lib` subfolders (or `/debug` and `/release`, see [Debug Builds](#debug-builds)), and optionally an `OPENVDB_INSTALL_DIR` or `OMNICLIENT_INSTALL_DIR`.

### Usage notes #

- Device name is `usd`
- All device-specific parameters are prefixed with `usd::`
- Set device `usd::serialize.location` string to the output location, `usd::serialize.outputBinary` bool for binary or text output. These parameters are **immutable**.
- Each ANARI scene object has a `name` parameter as scenegraph identifier (over time). Upon setting this name, a formatted version is stored in the `usd::name` property.
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

### Detailed build info #

#### Debug builds #

If you have separate release and debug versions of USD, (or standalone OpenVDB, Blosc, Zlib), make sure the `/lib` and `/include` directories of the respective installations exist within `/release` and `/debug` directories, ie. for USD: `<USD_INSTALL_DIR>/release` and `<USD_INSTALL_DIR>/debug`.

#### Downloading the Omniverse libraries #

- Get the Nvidia Omniverse Launcher from https://www.nvidia.com/en-us/omniverse/
- Go to the `Exchange` tab and navigate to the `Connectors` section. Install the Connect Sample.
- Go to the install directory of the connect sample, which is typically in `~/.local/share/ov/pkg/` or `<userdir>/AppData/local/ov/pkg/` but can also be found in the launcher by looking at your installed apps, a triple-stack on the right side of the connector sample entry -> settings
- Build the connect sample by running `build.sh/.bat` in its folder
- Locate the `nv_usd`, `omni_client_library` and `python` folders in the `_build/target-deps` subfolder
- The location of the previous three subfolders respectively can directly be set as `USD_INSTALL_DIR`, `OMNI_CLIENT_INSTALL_DIR`, `USD_PYTHON_INSTALL_DIR` in the ANARI CMake superbuild configuration

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
