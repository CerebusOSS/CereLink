# CereLink Build Instructions

Pre-built packages are available on the [releases page](https://github.com/CerebusOSS/CereLink/releases).

For GitHub Actions build scripts, see [.github/workflows/](./.github/workflows/).

## Requirements

C++17 toolchain and CMake 3.16+.

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `CBSDK_BUILD_TEST` | ON (standalone) | Build unit and integration tests |
| `CBSDK_BUILD_SAMPLE` | ON (standalone) | Build example applications |
| `CBSDK_BUILD_SHARED` | OFF | Build `cbsdk_shared` (DLL/dylib/so) for pycbsdk |

### Run Tests

```bash
ctest --test-dir build --build-config Release --output-on-failure -E "^device\."
```

The `-E "^device\."` filter excludes tests that require a physical device or network.

### Build Shared Library (for pycbsdk)

```bash
cmake -B build -S . -DCBSDK_BUILD_SHARED=ON -DCBSDK_BUILD_TEST=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target cbsdk_shared --config Release
```

### Install

```bash
cmake --build build --target install --config Release
```

### Package

```bash
cmake --build build --target package --config Release
```

## Python (pycbsdk)

See [pycbsdk/](./pycbsdk/) for building the Python wheel from source.
