# CapyJS

<div align="center">
  <img src="img/capy_js_logo.png" alt="Logo" width="400" height="400">
</div>

A high performance C++ compute engine with a JavaScript scripting layer, built on [QuickJS](https://bellard.org/quickjs/) and [libuv](https://libuv.org/).

JavaScript handles I/O and orchestration. CPU-intensive work runs in native workers on dedicated OS threads. The boundary between the two is a `uv_async_t` doorbell. However, the main event loop never blocks waiting on compute.

## Motivation

This is not meant to be a competitor to Node/Deno/Bun. Code that works on mainline runtimes might not work on CapyJS and vice versa. Rather, this is a research project and a proof of concept, to explore the architecture of a JS runtime where JS is strictly an thin glue layer and heavy lifting is done native side.

## Status

Not production ready. That said, this repo powers [jxia.dev](https://jxia.dev/) running on a Raspberry Pi 5. The server source is [here](https://github.com/JckXia/portfolio/tree/main).     
    


## Building

Requires CMake 3.21+.

### macOS (arm64, debug + ASan)

```sh
cmake --preset macos-dev
cmake --build --preset macos-dev
# output: build/macos-dev/capy
```

### macOS (arm64, release)

```sh
cmake --preset arm64-release
cmake --build --preset arm64-release
# output: build/arm64-release/capy
```

### Linux x86_64 (debug)

```sh
cmake --preset x86-dev
cmake --build --preset x86-dev
# output: build/x86-dev/capy
```

### Linux x86_64 (release, static)

```sh
cmake --preset x86-release
cmake --build --preset x86-release
# output: build/x86-release/capy
```

### Linux ARM64 cross-compile from x86 host (release, static)

Requires `aarch64-linux-gnu-g++`:

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
cmake --preset cross-arm64-release
cmake --build --preset cross-arm64-release
# output: build/cross-arm64-release/capy
```

## Running

```sh
./build/macos-dev/capy examples/server/hello_server.js
```

## Benchmarks

See [benchmark/README.md](benchmark/README.md) for results against Node.js v24.

## API Docs

- [Server](docs/server.md)
- [FileSystem](docs/filesystem.md)
- [Timers](docs/timers.md)
- [CPU Workers](docs/cpu_workers.md)
