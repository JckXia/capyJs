# CapyJS

<div align="center">
  <img src="img/capy_js_logo.png" alt="Logo" width="400" height="400">
</div>

## Building

Requires CMake 3.21+.

### Development (x86, debug symbols, dynamic linking)

```sh
cmake --preset dev
cmake --build --preset dev
# output: build-dev/capy
```

### Release x86 (static)

```sh
cmake --preset release-x86
cmake --build --preset release-x86
# output: build-release-x86/capy
```

### Release ARM64 (cross-compile, static)

Requires `aarch64-linux-gnu-g++`:

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
cmake --preset release-arm64
cmake --build --preset release-arm64
# output: build-release-arm64/capy
```
