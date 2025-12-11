#!/usr/bin/env python
import os
import sys
import subprocess
import shutil

from methods import print_error


libname = "godot-libartnet"
projectdir = "demo"

localEnv = Environment(tools=["default"], PLATFORM="")

# Build profiles can be used to decrease compile times.
# You can either specify "disabled_classes", OR
# explicitly specify "enabled_classes" which disables all other classes.
# Modify the example file as needed and uncomment the line below or
# manually specify the build_profile parameter when running SCons.

# localEnv["build_profile"] = "build_profile.json"

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

if not (os.path.isdir("libartnet") and os.listdir("libartnet")):
    print_error("""libartnet is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download libartnet:

    git submodule update --init --recursive""")
    sys.exit(1)

# Build godot-cpp library (the SConstruct automatically builds it via env.GodotCPP())
# This returns the environment with godot-cpp library already set up
env = SConscript("godot-cpp/SConstruct", {"env": env, "customs": customs})

# Build libartnet using autotools
libartnet_dir = "libartnet"
libartnet_build_dir = os.path.join("libartnet", ".build", env['platform'])

def build_libartnet(target, source, env):
    """Build libartnet using autotools (Linux/macOS/Android) or MSBuild (Windows)"""
    platform = env['platform']
    
    # For WASM, provide stub implementation
    if platform == "web":
        print("WARNING: libartnet networking not supported on WASM. Using stub implementation.")
        return 0
    
    # Check if libartnet submodule exists
    if not os.path.isdir(libartnet_dir):
        print_error("libartnet submodule not found. Run: git submodule update --init --recursive")
        return 1
    
    # Handle Windows builds using MSBuild (not autotools)
    if platform == "windows":
        build_path = os.path.join(libartnet_dir, ".build", platform)
        if not os.path.exists(build_path):
            os.makedirs(build_path)
        arch = env.get('arch', 'x86_64')
        if _build_libartnet_windows(env, build_path, arch):
            return 0
        else:
            return 1
    
    # Single architecture build (including macOS arm64 and x86_64)
    build_path = os.path.join(libartnet_dir, ".build", platform)
    if not os.path.exists(build_path):
        os.makedirs(build_path)
    
    arch = env.get('arch', 'arm64' if platform == 'macos' else 'x86_64')
    if not _build_libartnet_arch(env, build_path, arch):
        return 1
    
    return 0

def _build_libartnet_arch(env, build_path, arch):
    """Build libartnet for a specific architecture"""
    platform = env['platform']
    
    # Handle Windows builds using MSBuild
    if platform == "windows":
        return _build_libartnet_windows(env, build_path, arch)
    
    # Get configure flags based on platform
    configure_flags = []
    
    # For macOS, set architecture-specific flags
    if platform == "macos":
        if arch == "arm64":
            configure_flags.extend(["CFLAGS=-arch arm64 -fPIC", "CXXFLAGS=-arch arm64 -fPIC"])
        elif arch == "x86_64":
            # For x86_64 on arm64 hosts, we need to ensure proper cross-compilation
            # Use clang with explicit architecture targeting
            import platform as plat_module
            host_arch = plat_module.machine().lower()
            if host_arch == "arm64":
                # On arm64 host building for x86_64, ensure we use the right compiler flags
                # Check if we have x86_64 support (might not be available on all systems)
                configure_flags.extend([
                    "CFLAGS=-arch x86_64 -fPIC -target x86_64-apple-macosx",
                    "CXXFLAGS=-arch x86_64 -fPIC -target x86_64-apple-macosx",
                    "CC=clang",
                    "CXX=clang++"
                ])
            else:
                # On x86_64 host, just use -arch flag
                configure_flags.extend(["CFLAGS=-arch x86_64 -fPIC", "CXXFLAGS=-arch x86_64 -fPIC"])
    
    
    # Handle cross-compilation for Android
    if platform == "android":
        # Use the compiler tools that godot-cpp has already configured in the environment
        # These are set up correctly for Android cross-compilation
        cc = env.get('CC', os.environ.get('CC'))
        cxx = env.get('CXX', os.environ.get('CXX'))
        
        if not cc or not cxx:
            print_error("CC and CXX not set in environment. Android NDK toolchain may not be configured.")
            return False
        
        # Map architecture to Android host triple
        arch = env.get('arch', 'arm64')
        if arch == 'arm64':
            host = 'aarch64-linux-android'
        elif arch == 'arm32':
            host = 'armv7a-linux-androideabi'
        elif arch == 'x86_64':
            host = 'x86_64-linux-android'
        elif arch == 'x86_32':
            host = 'i686-linux-android'
        else:
            host = 'aarch64-linux-android'
        
        # Get API level from environment (set by godot-cpp)
        api_level = env.get('android_api_level', os.environ.get('ANDROID_API_LEVEL', '21'))
        
        # Get CCFLAGS from environment to preserve target and architecture flags
        ccflags = env.get('CCFLAGS', [])
        cflags_str = ' '.join(ccflags) if isinstance(ccflags, list) else str(ccflags)
        cxxflags_str = cflags_str  # Use same flags for CXX
        
        # Add Android-specific defines and ensure -fPIC
        cflags_str = "-fPIC -DANDROID " + cflags_str
        cxxflags_str = "-fPIC -DANDROID " + cxxflags_str
        
        configure_flags.extend([
            "--host={}".format(host),
            "CC={}".format(cc),
            "CXX={}".format(cxx),
            "CFLAGS={}".format(cflags_str),
            "CXXFLAGS={}".format(cxxflags_str),
        ])
    
    # Run autotools build process
    original_dir = os.getcwd()
    source_dir = os.path.abspath(libartnet_dir)
    
    try:
        # First, run autoreconf in the source directory if configure doesn't exist
        configure_script = os.path.join(source_dir, "configure")
        if not os.path.exists(configure_script):
            result = subprocess.run(["autoreconf", "-fi"], cwd=source_dir, capture_output=True, text=True)
            if result.returncode != 0:
                print_error("autoreconf failed:", result.stderr)
                return False
        
        # Now run configure in the build directory
        os.chdir(build_path)
        
        # For Android, remove any cached config to ensure fresh detection
        if platform == "android":
            config_cache = os.path.join(build_path, "config.cache")
            if os.path.exists(config_cache):
                os.remove(config_cache)
        
        # Run configure
        # Add -fPIC flag for static libraries that will be linked into shared libraries
        configure_env = os.environ.copy()
        
        # For Android, use the environment from godot-cpp
        if platform == "android":
            # Get the environment that godot-cpp set up
            if 'ENV' in env:
                configure_env.update(env['ENV'])
            # Set CC and CXX as environment variables (in addition to configure flags)
            if cc:
                configure_env['CC'] = cc
            if cxx:
                configure_env['CXX'] = cxx
            # Ensure PATH includes Android NDK toolchain
            if 'ENV' in env and 'PATH' in env['ENV']:
                existing_path = configure_env.get('PATH', '')
                configure_env['PATH'] = env['ENV']['PATH'] + os.pathsep + existing_path
        
        pic_flags = "-fPIC"
        if "CFLAGS" in configure_env:
            configure_env["CFLAGS"] = configure_env["CFLAGS"] + " " + pic_flags
        else:
            configure_env["CFLAGS"] = pic_flags
        if "CXXFLAGS" in configure_env:
            configure_env["CXXFLAGS"] = configure_env["CXXFLAGS"] + " " + pic_flags
        else:
            configure_env["CXXFLAGS"] = pic_flags
        
        configure_cmd = [configure_script, "--prefix", os.path.abspath("."), "--disable-shared", "--enable-static"]
        configure_cmd.extend(configure_flags)
        
        print("Running configure with CC={}, CXX={}".format(configure_env.get('CC', 'not set'), configure_env.get('CXX', 'not set')))
        result = subprocess.run(configure_cmd, capture_output=True, text=True, env=configure_env)
        if result.returncode != 0:
            print_error("configure failed:")
            if result.stdout:
                print_error("configure stdout:", result.stdout)
            if result.stderr:
                print_error("configure stderr:", result.stderr)
            return False
        
        # Run make with CFLAGS to disable problematic warnings that cause build failures
        # libartnet has some code that triggers warnings in newer GCC versions
        # We need to override CFLAGS to disable -Werror and specific warnings
        # Also ensure -fPIC is included for shared library linking
        make_env = os.environ.copy()
        
        # For Android, preserve the PATH and other environment variables from godot-cpp
        if platform == "android":
            # Use the same environment that godot-cpp set up
            if 'ENV' in env:
                make_env.update(env['ENV'])
            # Ensure PATH includes Android NDK toolchain
            if 'ENV' in env and 'PATH' in env['ENV']:
                existing_path = make_env.get('PATH', '')
                make_env['PATH'] = env['ENV']['PATH'] + os.pathsep + existing_path
        
        # Disable -Werror and specific warnings that cause issues with libartnet code
        # Append to existing CFLAGS if set by configure, otherwise set new ones
        cflags_extra = "-fPIC -Wno-error -Wno-memset-elt-size -Wno-stringop-truncation"
        # Read Makefile to see what CFLAGS configure set, then override
        # The safest approach is to pass CFLAGS directly to make
        make_cmd = ["make", "-j", str(os.cpu_count() or 4), "CFLAGS+=" + cflags_extra]
        
        result = subprocess.run(make_cmd, capture_output=True, text=True, env=make_env)
        if result.returncode != 0:
            print_error("make failed:", result.stderr)
            return False
        
        # Run make install
        result = subprocess.run(["make", "install"], capture_output=True, text=True)
        if result.returncode != 0:
            print_error("make install failed:", result.stderr)
            return False
        
    finally:
        os.chdir(original_dir)
    
    return True

def _build_libartnet_windows(env, build_path, arch):
    """Build libartnet on Windows using MSBuild"""
    original_dir = os.getcwd()
    source_dir = os.path.abspath(libartnet_dir)
    msvc_dir = os.path.join(source_dir, "msvc", "libartnet")
    solution_file = os.path.join(msvc_dir, "libartnet.sln")
    
    if not os.path.exists(solution_file):
        print_error("libartnet Visual Studio solution not found:", solution_file)
        return False
    
    try:
        # Determine build configuration and platform
        target_type = env.get('target', 'template_release')
        if 'debug' in target_type.lower():
            config = "Debug"
        else:
            config = "Release"
        
        # Map architecture
        if arch == "x86_64" or arch == "x64":
            msbuild_platform = "x64"
        elif arch == "x86_32" or arch == "x86":
            msbuild_platform = "Win32"
        else:
            msbuild_platform = "x64"  # Default to x64
        
        # Find MSBuild
        # Try common MSBuild locations
        msbuild_paths = [
            os.path.join(os.environ.get("ProgramFiles", "C:\\Program Files"), "Microsoft Visual Studio", "2022", "Community", "MSBuild", "Current", "Bin", "MSBuild.exe"),
            os.path.join(os.environ.get("ProgramFiles", "C:\\Program Files"), "Microsoft Visual Studio", "2022", "Enterprise", "MSBuild", "Current", "Bin", "MSBuild.exe"),
            os.path.join(os.environ.get("ProgramFiles", "C:\\Program Files"), "Microsoft Visual Studio", "2022", "Professional", "MSBuild", "Current", "Bin", "MSBuild.exe"),
            os.path.join(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "Microsoft Visual Studio", "2019", "Community", "MSBuild", "Current", "Bin", "MSBuild.exe"),
            os.path.join(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "Microsoft Visual Studio", "2019", "Enterprise", "MSBuild", "Current", "Bin", "MSBuild.exe"),
            os.path.join(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "Microsoft Visual Studio", "2019", "Professional", "MSBuild", "Current", "Bin", "MSBuild.exe"),
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        ]
        
        # Also try to find MSBuild in PATH
        msbuild = shutil.which("msbuild.exe") or shutil.which("MSBuild.exe")
        
        if not msbuild:
            for path in msbuild_paths:
                if os.path.exists(path):
                    msbuild = path
                    break
        
        if not msbuild:
            print_error("MSBuild not found. Please install Visual Studio or ensure MSBuild is in PATH.")
            return False
        
        # Build the solution
        # Determine output directory
        lib_dir = os.path.join(build_path, "lib")
        if not os.path.exists(lib_dir):
            os.makedirs(lib_dir)
        
        out_dir = os.path.abspath(lib_dir)
        # MSBuild expects trailing backslash for OutDir
        if not out_dir.endswith("\\"):
            out_dir = out_dir + "\\"
        
        # Determine platform toolset - use v143 (VS 2022) or v142 (VS 2019) if available
        # Default to v143 which should be available on GitHub Actions
        platform_toolset = "v143"  # Visual Studio 2022
        
        msbuild_cmd = [
            msbuild,
            solution_file,
            "/t:libartnet",
            "/p:Configuration=" + config,
            "/p:Platform=" + msbuild_platform,
            "/p:OutDir=" + out_dir,
            "/p:WindowsTargetPlatformVersion=10.0",  # Override SDK version to use available SDK
            "/p:PlatformToolset=" + platform_toolset,  # Override toolset to use available version
            "/v:minimal",  # Minimal verbosity
            "/nologo",  # Suppress MSBuild banner
        ]
        
        print("Running MSBuild command:", " ".join(msbuild_cmd))
        print("Working directory:", msvc_dir)
        print("Output directory:", out_dir)
        
        result = subprocess.run(msbuild_cmd, capture_output=True, text=True, cwd=msvc_dir, shell=False)
        
        if result.returncode != 0:
            print_error("MSBuild failed with return code:", result.returncode)
            if result.stdout:
                print_error("MSBuild stdout:")
                print(result.stdout)
            if result.stderr:
                print_error("MSBuild stderr:")
                print(result.stderr)
            # Also print the full command for debugging
            print_error("Command was:", " ".join(msbuild_cmd))
            return False
        
        # Print success message with any output
        if result.stdout:
            print("MSBuild output:", result.stdout)
        
        # MSBuild outputs to a specific directory structure
        # Find the built library (libartnet.lib)
        # The output directory structure varies, so we need to search
        lib_dir = os.path.join(build_path, "lib")
        if not os.path.exists(lib_dir):
            os.makedirs(lib_dir)
        
        # MSBuild typically outputs to msvc/libartnet/{Config}/{Platform}/
        # or to the OutDir we specified
        possible_lib_paths = [
            os.path.join(build_path, "lib", "libartnet.lib"),  # Our specified OutDir
            os.path.join(msvc_dir, config, msbuild_platform, "libartnet.lib"),
            os.path.join(msvc_dir, msbuild_platform, config, "libartnet.lib"),
            os.path.join(msvc_dir, "..", "..", config, msbuild_platform, "libartnet.lib"),
        ]
        
        # Also check common Visual Studio output locations
        vs_output_dirs = [
            os.path.join(os.path.dirname(msvc_dir), "..", "..", "..", "..", ".."),
            os.path.join(os.path.dirname(source_dir), ".."),
        ]
        for vs_output_base in vs_output_dirs:
            if os.path.exists(vs_output_base):
                possible_lib_paths.extend([
                    os.path.join(vs_output_base, "libartnet", config, msbuild_platform, "libartnet.lib"),
                    os.path.join(vs_output_base, "libartnet", msbuild_platform, config, "libartnet.lib"),
                ])
        
        lib_found = False
        for lib_path in possible_lib_paths:
            if os.path.exists(lib_path):
                # Copy to expected location
                shutil.copy2(lib_path, os.path.join(lib_dir, "libartnet.lib"))
                lib_found = True
                break
        
        if not lib_found:
            # Try to find any .lib file in the msvc directory tree
            for root, dirs, files in os.walk(msvc_dir):
                for file in files:
                    if file == "libartnet.lib":
                        lib_path = os.path.join(root, file)
                        shutil.copy2(lib_path, os.path.join(lib_dir, "libartnet.lib"))
                        lib_found = True
                        break
                if lib_found:
                    break
        
        if not lib_found:
            # Last resort: check if MSBuild created it in the OutDir we specified
            final_lib_path = os.path.join(lib_dir, "libartnet.lib")
            if os.path.exists(final_lib_path):
                lib_found = True
            else:
                print_error("Could not find built libartnet.lib file. Searched in:")
                for path in possible_lib_paths[:5]:  # Show first 5 paths
                    print_error("  -", path)
                return False
        
        # Copy headers to include directory
        include_src = os.path.join(source_dir, "artnet")
        include_dst = os.path.join(build_path, "include", "artnet")
        if os.path.exists(include_src) and not os.path.exists(include_dst):
            os.makedirs(os.path.join(build_path, "include"), exist_ok=True)
            shutil.copytree(include_src, include_dst)
        
        # On Windows, add a small delay after MSBuild to allow file handles to be released
        if platform == "windows":
            import time
            time.sleep(0.5)  # Wait 500ms for file handles to be released
        
    finally:
        os.chdir(original_dir)
    
    return True

# Build libartnet before building the extension
if env['platform'] != "web":
    # Use platform-specific build directory
    libartnet_build_dir = os.path.join(libartnet_dir, ".build", env['platform'])
    
    # Create marker file creation function
    def create_marker(target, source, env):
        marker_file = str(target[0])
        marker_dir = os.path.dirname(marker_file)
        
        # Ensure directory exists
        try:
            os.makedirs(marker_dir, exist_ok=True)
        except (IOError, OSError):
            # Directory might be locked, wait and retry
            import time
            time.sleep(0.2)
            os.makedirs(marker_dir, exist_ok=True)
        
        # Create marker file with retry logic for Windows file locking
        import time
        max_retries = 10
        retry_delay = 0.2
        for attempt in range(max_retries):
            try:
                # Try to create/truncate the file
                # Use 'x' mode first to ensure it doesn't exist, fall back to 'w'
                try:
                    with open(marker_file, 'x') as f:
                        f.write("libartnet built successfully\n")
                except FileExistsError:
                    # File exists, just update it
                    with open(marker_file, 'w') as f:
                        f.write("libartnet built successfully\n")
                break  # Success, exit retry loop
            except (IOError, OSError, PermissionError) as e:
                if attempt < max_retries - 1:
                    time.sleep(retry_delay)
                    retry_delay = min(retry_delay * 1.5, 2.0)  # Exponential backoff, max 2s
                else:
                    # Last attempt failed, try one more time with a longer delay
                    time.sleep(1.0)
                    try:
                        with open(marker_file, 'w') as f:
                            f.write("libartnet built successfully\n")
                    except:
                        # Final failure - print error but don't crash
                        print_warning("Warning: Could not create marker file {}, but build succeeded".format(marker_file))
                        # Try to create it in a different location as fallback
                        alt_marker = marker_file + ".tmp"
                        try:
                            with open(alt_marker, 'w') as f:
                                f.write("libartnet built successfully\n")
                            # Try to rename it
                            import time
                            time.sleep(0.5)
                            if os.path.exists(alt_marker):
                                os.rename(alt_marker, marker_file)
                        except:
                            pass
        return 0
    
    # Build libartnet and create marker file
    # Ensure godot-cpp is built first
    godot_cpp_lib_path = "godot-cpp/bin/libgodot-cpp{}{}".format(env['suffix'], env['LIBSUFFIX'])
    godot_cpp_lib_file = env.File(godot_cpp_lib_path)
    
    libartnet_built = env.Command(
        os.path.join(libartnet_build_dir, ".built"),
        [godot_cpp_lib_file],  # Depend on godot-cpp being built first
        [build_libartnet, create_marker]
    )
    env.AlwaysBuild(libartnet_built)
    
    # Add libartnet include and library paths
    # Headers are installed in include/artnet/ directory
    libartnet_include = os.path.join(libartnet_build_dir, "include")
    libartnet_lib_dir = os.path.join(libartnet_build_dir, "lib")
    
    env.Append(CPPPATH=[libartnet_include])
    env.Append(LIBPATH=[libartnet_lib_dir])
    # On Windows, the library is named libartnet.lib, on Unix it's libartnet.a
    if env['platform'] == "windows":
        env.Append(LIBS=["libartnet"])
        # Windows socket libraries required by libartnet (ws2_32 for sockets, IPHLPAPI for network interface)
        env.Append(LIBS=["ws2_32", "IPHLPAPI"])
    else:
        env.Append(LIBS=["artnet"])
else:
    # For WASM, we'll need to handle this differently or provide stubs
    # For now, just add the include path so headers can be found
    libartnet_include = os.path.join(libartnet_dir, "artnet")
    env.Append(CPPPATH=[libartnet_include])
    # Define a preprocessor macro to indicate WASM stub mode
    env.Append(CPPDEFINES=["ARTNET_WASM_STUB"])

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# .dev doesn't inhibit compatibility, so we don't need to key it.
# Clean up suffix
suffix = env['suffix'].replace(".dev", "")

lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

# Make libartnet build a dependency before creating the library
if env['platform'] != "web":
    # Ensure libartnet is built before we try to compile sources
    # Make each source file depend on libartnet_built
    for source in sources:
        env.Depends(source, libartnet_built)

library = env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=sources,
)

# Ensure godot-cpp library is built before our library
# The godot-cpp SConstruct builds it automatically, but we make it an explicit dependency
godot_cpp_lib_path = "godot-cpp/bin/libgodot-cpp{}{}".format(env['suffix'], env['LIBSUFFIX'])
godot_cpp_lib_file = env.File(godot_cpp_lib_path)
env.Depends(library, godot_cpp_lib_file)

# Make libartnet build a dependency of the library
if env['platform'] != "web":
    env.Depends(library, libartnet_built)

copy = env.Install("{}/bin/{}/".format(projectdir, env["platform"]), library)

default_args = [library, copy]
Default(*default_args)
