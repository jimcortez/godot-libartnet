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
    
    # For macOS universal builds, we need to build for both architectures
    if platform == "macos" and env.get('arch') == 'universal':
        # Build for both arm64 and x86_64, then combine with lipo
        archs = ['arm64', 'x86_64']
        lib_files = []
        
        for arch in archs:
            build_path = os.path.join(libartnet_dir, ".build", platform, arch)
            if not os.path.exists(build_path):
                os.makedirs(build_path)
            
            # Build for this architecture
            if not _build_libartnet_arch(env, build_path, arch):
                return 1
            
            # Find the built library
            lib_file = os.path.join(build_path, "lib", "libartnet.a")
            if os.path.exists(lib_file):
                # Verify the architecture of the built library BEFORE adding to list
                result = subprocess.run(["lipo", "-info", lib_file], capture_output=True, text=True)
                if result.returncode == 0:
                    arch_info = result.stdout.lower()
                    expected_arch = arch.lower()
                    
                    # Check if the library has the expected architecture
                    has_expected_arch = False
                    if expected_arch == "arm64" and "arm64" in arch_info:
                        has_expected_arch = True
                    elif expected_arch == "x86_64" and ("x86_64" in arch_info or "x86_64" in arch_info):
                        has_expected_arch = True
                    
                    if has_expected_arch:
                        # Check for duplicate architectures in existing lib_files
                        is_duplicate = False
                        for existing_lib in lib_files:
                            existing_result = subprocess.run(["lipo", "-info", existing_lib], capture_output=True, text=True)
                            if existing_result.returncode == 0:
                                existing_arch_info = existing_result.stdout.lower()
                                # Check if both have the same architecture
                                if "arm64" in arch_info and "arm64" in existing_arch_info:
                                    is_duplicate = True
                                    print("WARNING: Skipping {} - duplicate arm64 architecture (x86_64 build produced arm64)".format(lib_file))
                                    break
                                elif "x86_64" in arch_info and "x86_64" in existing_arch_info:
                                    is_duplicate = True
                                    print("WARNING: Skipping {} - duplicate x86_64 architecture".format(lib_file))
                                    break
                        
                        if not is_duplicate:
                            lib_files.append(lib_file)
                        else:
                            # Skip this library as it's a duplicate architecture
                            continue
                    else:
                        print_error("Library {} has wrong architecture. Expected {}, got: {}".format(
                            lib_file, arch, result.stdout.strip()))
                        # If we're on arm64 and trying to build x86_64, skip it
                        if arch == "x86_64" and "arm64" in arch_info:
                            print("WARNING: x86_64 build produced arm64 library. Skipping x86_64 for universal binary.")
                            continue
                else:
                    print_error("Failed to check architecture of library:", lib_file)
                    # Add it anyway, but this might cause issues
                    lib_files.append(lib_file)
        
        # Combine into universal binary
        if len(lib_files) >= 1:
            universal_build_path = os.path.join(libartnet_dir, ".build", platform)
            universal_lib_dir = os.path.join(universal_build_path, "lib")
            if not os.path.exists(universal_lib_dir):
                os.makedirs(universal_lib_dir)
            
            universal_lib = os.path.join(universal_lib_dir, "libartnet.a")
            
            if len(lib_files) == 2:
                # Combine both architectures
                result = subprocess.run(["lipo", "-create", "-output", universal_lib] + lib_files, 
                                      capture_output=True, text=True)
                if result.returncode != 0:
                    print_error("lipo failed to create universal binary:", result.stderr)
                    return 1
            elif len(lib_files) == 1:
                # Only one architecture available, just copy it
                print("WARNING: Only one architecture available for universal binary. Using single-arch library.")
                shutil.copy2(lib_files[0], universal_lib)
            else:
                print_error("Failed to build libartnet for any required architectures")
                return 1
            
            # Copy headers to universal build directory
            include_src = os.path.join(libartnet_dir, ".build", platform, "arm64", "include")
            if not os.path.exists(include_src):
                include_src = os.path.join(libartnet_dir, ".build", platform, "x86_64", "include")
            include_dst = os.path.join(universal_build_path, "include")
            if os.path.exists(include_src) and not os.path.exists(include_dst):
                shutil.copytree(include_src, include_dst)
        else:
            print_error("Failed to build libartnet for any required architectures")
            return 1
    else:
        # Single architecture build
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
        # Android NDK toolchain setup
        android_ndk = os.environ.get("ANDROID_NDK_ROOT") or os.environ.get("ANDROID_NDK")
        if not android_ndk:
            print_error("ANDROID_NDK_ROOT or ANDROID_NDK environment variable not set")
            return 1
        
        arch = env.get('arch', 'arm64')
        if arch == 'arm64':
            host = 'aarch64-linux-android'
        elif arch == 'arm32':
            host = 'arm-linux-androideabi'
        elif arch == 'x86_64':
            host = 'x86_64-linux-android'
        elif arch == 'x86_32':
            host = 'i686-linux-android'
        else:
            host = 'aarch64-linux-android'
        
        # Set up Android toolchain
        api_level = os.environ.get("ANDROID_API_LEVEL", "21")
        toolchain = os.path.join(android_ndk, "toolchains", "llvm", "prebuilt")
        toolchain_dirs = os.listdir(toolchain) if os.path.exists(toolchain) else []
        if toolchain_dirs:
            toolchain = os.path.join(toolchain, toolchain_dirs[0])
        
        cc = os.path.join(toolchain, "bin", "{}-clang".format(host))
        cxx = os.path.join(toolchain, "bin", "{}-clang++".format(host))
        
        if os.path.exists(cc):
            configure_flags.extend([
                "--host={}".format(host),
                "CC={}".format(cc),
                "CXX={}".format(cxx),
                "CFLAGS=-fPIC -DANDROID",
                "CXXFLAGS=-fPIC -DANDROID",
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
        
        # Run configure
        # Add -fPIC flag for static libraries that will be linked into shared libraries
        configure_env = os.environ.copy()
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
        
        result = subprocess.run(configure_cmd, capture_output=True, text=True, env=configure_env)
        if result.returncode != 0:
            print_error("configure failed:", result.stderr)
            return False
        
        # Run make with CFLAGS to disable problematic warnings that cause build failures
        # libartnet has some code that triggers warnings in newer GCC versions
        # We need to override CFLAGS to disable -Werror and specific warnings
        # Also ensure -fPIC is included for shared library linking
        make_env = os.environ.copy()
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
        os.chdir(msvc_dir)
        msbuild_cmd = [
            msbuild,
            solution_file,
            "/t:libartnet",
            "/p:Configuration=" + config,
            "/p:Platform=" + msbuild_platform,
            "/p:OutDir=" + os.path.abspath(os.path.join(build_path, "lib")) + "\\",
        ]
        
        result = subprocess.run(msbuild_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print_error("MSBuild failed:", result.stderr)
            return False
        
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
        
    finally:
        os.chdir(original_dir)
    
    return True

# Build libartnet before building the extension
if env['platform'] != "web":
    # For universal macOS builds, use the universal build directory
    if env['platform'] == "macos" and env.get('arch') == 'universal':
        libartnet_build_dir = os.path.join(libartnet_dir, ".build", env['platform'])
    
    # Create marker file creation function
    def create_marker(target, source, env):
        marker_file = str(target[0])
        # Ensure directory exists
        os.makedirs(os.path.dirname(marker_file), exist_ok=True)
        with open(marker_file, 'w') as f:
            f.write("libartnet built successfully\n")
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
# .universal just means "compatible with all relevant arches" so we don't need to key it.
suffix = env['suffix'].replace(".dev", "").replace(".universal", "")

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
