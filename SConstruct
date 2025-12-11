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
    """Build libartnet using autotools"""
    platform = env['platform']
    
    # For WASM, provide stub implementation
    if platform == "web":
        print("WARNING: libartnet networking not supported on WASM. Using stub implementation.")
        return 0
    
    # Check if libartnet submodule exists
    if not os.path.isdir(libartnet_dir):
        print_error("libartnet submodule not found. Run: git submodule update --init --recursive")
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
                lib_files.append(lib_file)
        
        # Combine into universal binary
        if len(lib_files) == 2:
            universal_build_path = os.path.join(libartnet_dir, ".build", platform)
            universal_lib_dir = os.path.join(universal_build_path, "lib")
            if not os.path.exists(universal_lib_dir):
                os.makedirs(universal_lib_dir)
            
            universal_lib = os.path.join(universal_lib_dir, "libartnet.a")
            result = subprocess.run(["lipo", "-create", "-output", universal_lib] + lib_files, 
                                  capture_output=True, text=True)
            if result.returncode != 0:
                print_error("lipo failed to create universal binary:", result.stderr)
                return 1
            
            # Copy headers to universal build directory
            include_src = os.path.join(libartnet_dir, ".build", platform, "arm64", "include")
            include_dst = os.path.join(universal_build_path, "include")
            if os.path.exists(include_src) and not os.path.exists(include_dst):
                shutil.copytree(include_src, include_dst)
        else:
            print_error("Failed to build libartnet for all required architectures")
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
    
    # Get configure flags based on platform
    configure_flags = []
    
    # For macOS, set architecture-specific flags
    if platform == "macos":
        if arch == "arm64":
            configure_flags.extend(["CFLAGS=-arch arm64", "CXXFLAGS=-arch arm64"])
        elif arch == "x86_64":
            configure_flags.extend(["CFLAGS=-arch x86_64", "CXXFLAGS=-arch x86_64"])
    
    
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
        configure_cmd = [configure_script, "--prefix", os.path.abspath("."), "--disable-shared", "--enable-static"]
        configure_cmd.extend(configure_flags)
        
        result = subprocess.run(configure_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print_error("configure failed:", result.stderr)
            return False
        
        # Run make with CFLAGS to disable problematic warnings that cause build failures
        # libartnet has some code that triggers warnings in newer GCC versions
        # We need to override CFLAGS to disable -Werror and specific warnings
        make_env = os.environ.copy()
        # Disable -Werror and specific warnings that cause issues with libartnet code
        # Append to existing CFLAGS if set by configure, otherwise set new ones
        cflags_extra = "-Wno-error -Wno-memset-elt-size -Wno-stringop-truncation"
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
