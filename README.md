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

The best way to build is to run `cmake(-gui)` on the `superbuild` subdir, for detailed instructions see `superbuild/README.md`. 

In short, use the above to set `USD_ROOT_DIR` to the directory containing the `/include` and `/lib` subfolders (or `/debug` and `/release`, see [Debug Builds](#debug-builds)), and optionally an `OPENVDB_ROOT_DIR` or `OMNICLIENT_ROOT_DIR`. After configuring and genering the superbuild, the actual projects and dependencies are configured, generated and built with `cmake --build . --config [Release|Debug]`

### Usage notes #

- Device name is `usd`
- All device-specific parameters are prefixed with `usd::`
- Set device `usd::serialize.location` string to the output location on disk, `usd::serialize.outputBinary` bool for binary or text output. These parameters are **immutable**.
- Alternatively, `usd::serialize.location` will also try the `ANARI_USD_SERIALIZE_LOCATION`
environment variable. If neither are specified, it will default to `"./"` and emit a warning.
- If Omniverse support is available, `usd::serialize.hostname` has to be used to specify the server name, and optionally port for Omniverse connections. This parameter is **immutable**.
- Each ANARI scene object has a `name` parameter as scenegraph identifier (over time). Upon setting this name, a formatted version is stored in the `usd::name` property.
- Each ANARI scene object has a `usd::timestep` parameter to define the time at which `commit()` will add the data to the scenegraph object indicated by `usd::name`.
- For ANARI object parameters that should be constant over all timesteps, unset their corresponding bits in the `usd::timeVarying` parameter specific to each ANARI scene object (see headers). This parameter is **immutable**.
- Changes to data are **actually saved** when `anariRenderFrame()` is called.
- If ANARI objects of a certain `name` are not referenced from within any committed timestep, their internal data is only cleaned up when calling `anariDeviceSetParam(d, "usd::garbagecollect", ANARI_VOID_POINTER, 0)`. This is adviced after every `anariRenderFrame()` or a subfrequency thereof.
- Not supported:
    - Arbitrary commit order - commits need to happen from leaf objects (Geometry/Materials/etc.) to root objects (World)
    - Geometries:
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

### Advanced parameters #
- Device parameter `usd::scenestage` allows the user to provide a pre-constructed stage, into which the USD output will be constructed. For correct operation, make sure that `anariSetParameter` for `usd::scenestage` takes a `UsdStage*` (ie. the `mem` argument is directly of `UsdStage*` type) with `ANARI_VOID_POINTER` as type enumeration. This parameter is **immutable**.
- Device parameter `usd::enablesaving` of type `ANARI_BOOL` allows the user to explicitly control whether USD output is written out to disk, or kept in memory. Assets that are not stored in USD format, such as MDL materials, texture images and volumes, will always be written to disk regardless of the value of this parameter. In order for no files to be written at all, additionally pass the special string `"void"` to `usd::serialize.location`.

### Detailed build info #

#### Debug builds #

If you have separate release and debug versions of USD, (or standalone OpenVDB, Blosc, Zlib), make sure the `/lib` and `/include` directories of the respective installations exist within `/release` and `/debug` directories, ie. for USD: `<USD_ROOT_DIR>/release` and `<USD_ROOT_DIR>/debug`.

#### Downloading the Omniverse libraries #

- Get the Nvidia Omniverse Launcher from https://www.nvidia.com/en-us/omniverse/
- Go to the `Exchange` tab and navigate to the `Connectors` section. Install the Connect Sample (version 102.1.5 is known to work).
- Go to the install directory of the connect sample, which is typically in `~/.local/share/ov/pkg/` or `<userdir>/AppData/local/ov/pkg/` but can also be found in the launcher by looking at your installed apps, a triple-stack on the right side of the connector sample entry -> settings
- Build the connect sample by running `build.sh/.bat` in its folder
- Locate the `nv_usd`, `omni_client_library` and `python` folders in the `_build/target-deps` subfolder
- The location of the previous three subfolders respectively can directly be set as `USD_ROOT_DIR`, `OMNICLIENT_ROOT_DIR`, `Python_ROOT_DIR` in the ANARI CMake superbuild configuration

#### Building USD Manually #

- The environment variable BOOST_ROOT should *not* be set
- On Linux:
    - Zlib has to either be installed on a system level or built from source (with `-fPIC` added to `CMAKE_CXX_FLAGS`)
        - Run, for a particular value of `<zlib_install_dir>`: `export CMAKE_ZLIB_ARGS="-D ZLIB_ROOT=<zlib_install_dir>` 
    - If using Anaconda: 
        - The pyside2 and pyopengl package has to be included in the environment
        - Make sure a Python include directory without `m` suffix exists (create a symbolic link if necessary)
    - Run, for a particular value of `<config>`: `export LD_LIBRARY_PATH=<USD_ROOT_DIR>/<config>/lib:$LD_LIBRARY_PATH` 
- On Windows:
    - Make sure `pyside2-uic.exe` from your Python `./Scripts` directory is in the `PATH`
    - **only** for debug builds: In pyconfig.h all `pragma comment(lib,"pythonXX.lib")` statements should change to `pragma comment(lib,"pythonXX_d.lib")`. They can be changed back after building USD.
- Build and install USD by running the buildscript `python <usd_source_dir>/build_scripts/build_usd.py <USD_ROOT_DIR>/<config>` (where `USD_ROOT_DIR` is the desired installation location)
    - Add `--debug` (v21.08-) or `--build-variant debug` (v21.11+) for debug builds
    - For OpenVDB on Windows, run **in a developer command prompt** and add the following flags: `--openvdb --build-args openvdb,"-DCMAKE_CXX_FLAGS=\"-D__TBB_NO_IMPLICIT_LINKAGE /EHsc\""`
        - OpenVDB's FindIlmBase.cmake is broken for debug builds, so make sure that the `_d` suffix is removed from `<USD_ROOT_DIR>/<config>/lib/Half-X_X_d.lib` and `<USD_ROOT_DIR>/<config>/bin/Half-X_X_d.dll` after running the build once, then run it again.
    - For OpenVDB on Linux, add the following flags: `--openvdb --build-args openvdb,"-D CMAKE_CXX_FLAGS:STRING=-fPIC $CMAKE_ZLIB_ARGS" blosc,"-D CMAKE_C_FLAGS:STRING=-fPIC -D CMAKE_CXX_FLAGS:STRING=-fPIC" openexr,"$CMAKE_ZLIB_ARGS"`
