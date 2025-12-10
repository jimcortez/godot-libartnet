# godot-libartnet

A GDExtension wrapper for the [libartnet](https://github.com/OpenLightingProject/libartnet) library, enabling ArtNet DMX control from Godot projects.

## Overview

This GDExtension provides a GDScript interface to send ArtNet DMX-512 data over IP networks. ArtNet is a protocol commonly used in professional lighting control systems to transmit DMX lighting data over Ethernet.

## Features

- Send DMX-512 data over ArtNet protocol
- Configure ArtNet network settings (subnet, universe, etc.)
- Support for up to 512 DMX channels per universe
- Cross-platform support (Linux, Windows, macOS, Android)
- WASM stub implementation (networking not supported on WebAssembly)

## Requirements

- Godot 4.1 or later
- C++ compiler with C++17 support
- Autotools (autoconf, automake, libtool) for building libartnet
- For Android: Android NDK

## Installation

### 1. Clone the Repository

```bash
git clone <repository-url>
cd godot-libartnet
```

### 2. Initialize Submodules

```bash
git submodule update --init --recursive
```

This will download:
- `godot-cpp` - Godot C++ bindings
- `libartnet` - ArtNet protocol library

### 3. Build the Extension

Build using SCons:

```bash
scons platform=<platform> target=template_release
```

Replace `<platform>` with one of:
- `linux`
- `windows`
- `macos`
- `android` (requires Android NDK)
- `web` (WASM - stub implementation only)

For example, on Linux:

```bash
scons platform=linux target=template_release
```

The built library will be in `bin/<platform>/` and copied to `demo/bin/<platform>/`.

### 4. Use in Your Godot Project

1. Copy the `demo/bin/libartnet.gdextension` file to your project
2. Copy the appropriate library file from `demo/bin/<platform>/` to your project
3. Load the extension in your GDScript code

## Usage

### Basic Example

```gdscript
extends Node

var artnet: ArtNetController

func _ready():
    # Create ArtNet controller
    artnet = ArtNetController.new()
    
    # Configure the controller
    # Parameters: bind_address, port, net, subnet, universe, broadcast_address
    if artnet.configure("192.168.1.100", 6454, 0, 0, 0):
        print("ArtNet configured successfully")
        
        # Start the controller
        if artnet.start():
            print("ArtNet started")
            
            # Create DMX data (512 channels)
            var dmx_data = PackedByteArray()
            dmx_data.resize(512)
            
            # Set some channel values
            dmx_data[0] = 255  # Channel 1 at full brightness
            dmx_data[1] = 128  # Channel 2 at half brightness
            dmx_data[2] = 64   # Channel 3 at quarter brightness
            
            # Set the DMX data
            if artnet.set_dmx_data(0, dmx_data):
                # Send the DMX data
                if artnet.send_dmx():
                    print("DMX data sent successfully")
            
            # Clean up when done
            artnet.stop()
        else:
            print("Failed to start ArtNet")
    else:
        print("Failed to configure ArtNet")
```

### API Reference

#### `configure(bind_address: String, port: int, net: int, subnet: int, universe: int, broadcast_address: String = "255.255.255.255") -> bool`

Configures the ArtNet node with network settings. Must be called before `start()`.

- `bind_address`: IP address to bind to (e.g., "192.168.1.100")
- `port`: UDP port (typically 6454 for ArtNet)
- `net`: ArtNet net address (0-127)
- `subnet`: ArtNet subnet address (0-15)
- `universe`: DMX universe number (0-15 within subnet)
- `broadcast_address`: Broadcast address for packets (default: "255.255.255.255")

Returns `true` on success.

#### `start() -> bool`

Starts the ArtNet node and begins network operations. Must be called after `configure()`.

Returns `true` on success.

#### `stop() -> void`

Stops the ArtNet node and ends network operations.

#### `is_running() -> bool`

Returns `true` if the ArtNet node is currently running.

#### `set_dmx_data(universe: int, data: PackedByteArray) -> bool`

Sets the DMX channel data to be sent. Data can contain up to 512 bytes (one per channel).

- `universe`: DMX universe number
- `data`: PackedByteArray with DMX channel values (0-255)

Returns `true` on success.

#### `send_dmx() -> bool`

Sends the DMX data set with `set_dmx_data()` over the ArtNet network.

Returns `true` on success.

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux | ✅ Supported | Native build |
| Windows | ✅ Supported | Native build |
| macOS | ✅ Supported | Native build |
| Android | ✅ Supported | Requires Android NDK |
| WebAssembly | ⚠️ Stub Only | Networking not supported, methods return errors |

## Building for Different Platforms

### Linux

```bash
scons platform=linux target=template_release
```

### Windows

```bash
scons platform=windows target=template_release
```

### macOS

```bash
scons platform=macos target=template_release
```

### Android

Set the Android NDK environment variable:

```bash
export ANDROID_NDK_ROOT=/path/to/android-ndk
scons platform=android arch=arm64 target=template_release
```

### WebAssembly

```bash
scons platform=web target=template_release
```

Note: WASM builds provide stub implementations that return errors, as UDP networking is not available in browser environments.

## Troubleshooting

### Build Issues

- **autoreconf not found**: Install autotools (`autoconf`, `automake`, `libtool`)
- **libartnet build fails**: Ensure all submodules are initialized with `git submodule update --init --recursive`
- **Android build fails**: Verify `ANDROID_NDK_ROOT` or `ANDROID_NDK` environment variable is set

### Runtime Issues

- **"Failed to create artnet node"**: Check that the bind address is valid and the network interface exists
- **"Failed to start"**: Ensure the port is not already in use and you have network permissions
- **DMX data not received**: Verify network connectivity, subnet/universe settings, and firewall rules

## License

This project uses the libartnet library, which is licensed under LGPL-2.1. See [LICENSE.md](LICENSE.md) for details.

## Contributing

Contributions are welcome! Please ensure your code follows the existing style and includes appropriate error handling.

## References

- [libartnet](https://github.com/OpenLightingProject/libartnet) - The underlying ArtNet library
- [ArtNet Protocol Specification](https://art-net.org.uk/) - Official ArtNet documentation
- [Godot GDExtension Documentation](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html)
