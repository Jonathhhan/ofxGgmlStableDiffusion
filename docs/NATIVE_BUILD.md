# Native Build

`ofxGgmlStableDiffusion` stages the native runtime into addon-local folders instead
of relying on a global install.

## Layout

- `libs/stable-diffusion/source`
  A vendored snapshot of upstream `stable-diffusion.cpp`
- `libs/stable-diffusion/include`
  Thin passthrough include for the vendored upstream header
- `libs/stable-diffusion/lib/vs`
  Windows import library and runtime DLL
- `libs/stable-diffusion/lib/Linux64`
  Linux shared library staging location
- `libs/ggml/include`
  Staged `ggml` public headers copied from the vendored `ggml` tree
- `libs/ggml/lib/vs`
  Separately staged `ggml` import/static libraries emitted by the native build
- `libs/variants/<backend>/...`
  Snapshot of one built backend variant, kept so the canonical addon paths can
  be switched later without a full rebuild

## Why It Is Standalone (By Default)

This addon intentionally does **not** link against the `ggml` build staged by
`ofxGgmlCore` by default.

Reasons:

- `stable-diffusion.cpp` often expects a specific `ggml` revision or patch set
- backend flags can differ across diffusion and language runtimes
- a shared native binary would make addon upgrades more fragile

The right place to integrate with companion addons is the addon/API layer.
System-GGML experiments are not part of the default staging path.

## Rebuild Workflow

### Windows

```powershell
.\scripts\build-stable-diffusion.ps1 -Configuration Release
```

Or use the addon-level setup entrypoint:

```bat
scripts\setup_windows.bat
```

### Linux / macOS-style shell

```bash
./scripts/build-stable-diffusion.sh
```

## Backend Flags

The native build scripts now select one native backend per build. If you do not
pass a backend flag, they default to CPU-only.

- `--cpu`, `--cpu-only` / `-CpuOnly`
  Build CPU-only
- `--gpu`, `--cuda` / `-Cuda`
  Build with CUDA
- `--vulkan` / `-Vulkan`
  Build with Vulkan
- `--metal` / `-Metal`
  Build with Metal where supported
- `--all` / `-All`
  Build every available backend variant and leave the canonical addon runtime on
  the best one in this priority order: `cuda`, then `vulkan`, then `cpu-only`

On Windows, `scripts/setup_windows.bat` forwards the same flags into
`scripts/setup_addon.ps1`, which builds the native runtime.

Each build also snapshots the selected backend under `libs/variants/<backend>`.
That makes it possible to switch the canonical addon runtime later without
rebuilding.

In `--all` mode, the script always snapshots `cpu-only` first, then adds any
detected GPU backends. On Windows that means:

- `cpu-only`
- `vulkan` when Vulkan is detected
- `cuda` when CUDA is detected

Because the builds run in that order, the final canonical runtime is left on
the highest-priority available backend: `cuda` > `vulkan` > `cpu-only`.

## Core/System GGML Mode

By default, ofxGgmlStableDiffusion builds against the `ofxGgmlCore` ggml
provider. The scripts still contain an explicit bundled ggml fallback for
compatibility testing, but Core is the managed ecosystem default.

### Prerequisites

1. Build and install `ofxGgmlCore` first with the desired backend (CPU/CUDA/Vulkan)
2. Confirm `ofxGgmlCore\scripts\runtime-provider-manifest.ps1 -Json -SummaryOnly` reports ready
3. Verify GGML version compatibility between Core and the stable-diffusion.cpp version you're using

### Building with System GGML

**Linux/macOS:**

```bash
# Use default provider path (../ofxGgmlCore)
./scripts/build-stable-diffusion.sh

# Specify custom Core-compatible provider path
./scripts/build-stable-diffusion.sh --ofxggml-path /path/to/ofxGgmlCore

# Combine with backend selection
./scripts/build-stable-diffusion.sh --cuda

# Explicit bundled fallback
./scripts/build-stable-diffusion.sh --use-bundled-ggml --cuda
```

**Windows:**

```powershell
# Use default provider path (..\ofxGgmlCore)
.\scripts\build-stable-diffusion.ps1

# Specify custom Core-compatible provider path
.\scripts\build-stable-diffusion.ps1 -OfxGgmlPath C:\path\to\ofxGgmlCore

# Combine with backend selection
.\scripts\build-stable-diffusion.ps1 -Cuda

# Explicit bundled fallback
.\scripts\build-stable-diffusion.ps1 -UseBundledGgml -Cuda
```

### What Happens

When `-DSD_USE_SYSTEM_GGML=ON` is enabled:

1. The build scripts validate that `ofxGgmlCore` or a compatible provider exists at the specified path
2. CMake is configured to use `find_package(ggml)` instead of building bundled GGML
3. The stable-diffusion library links against the provider's GGML binaries
4. At runtime, both ofxGgmlStableDiffusion and Core-backed companions share the same GGML library

### Important Considerations

**Backend Alignment:**
- ofxGgmlCore and ofxGgmlStableDiffusion must use matching backends (both CPU, both CUDA, etc.)
- The build script does not automatically enforce this—ensure consistency manually

**Version Compatibility:**
- stable-diffusion.cpp may require specific GGML versions or patches
- Test thoroughly when updating either addon
- Monitor for ABI changes between GGML versions

**Dependency Management:**
- When using system GGML, your application must ensure the Core/provider GGML libraries are available at runtime
- On openFrameworks projects, include both addons in your project

**Fallback:**
- Core/system GGML mode is the default
- The bundled standalone mode remains available if you encounter compatibility issues
- Rebuild with `--use-bundled-ggml` / `-UseBundledGgml` to isolate ggml

### Troubleshooting

**Error: "Core ggml provider not found"**
- Verify the path to ofxGgmlCore is correct
- Use `--ofxggml-path` / `-OfxGgmlPath` to specify the correct location

**Error: "System GGML headers not found"**
- Build ofxGgmlCore first before building ofxGgmlStableDiffusion
- Ensure Core's GGML headers exist at `libs/ggml/include`

**Runtime linking errors:**
- Ensure backend flags match between ofxGgmlCore and ofxGgmlStableDiffusion
- Verify GGML library versions are compatible
- Check that GGML shared libraries are in your system's library path



## Variant Selection

To install one already-built backend variant into the normal addon paths:

```powershell
.\scripts\select-stable-diffusion-backend.ps1 -Backend cuda
```

Or through the batch wrapper:

```bat
scripts\setup_windows.bat --skip-native --select-backend cuda
```

Available selector values:

- `cpu-only`
- `cuda`
- `vulkan`
- `metal`

## Windows Source Snapshot Flow

On Windows, the addon setup entrypoints now always fetch the source snapshot
for the latest upstream `stable-diffusion.cpp` release tag, then refresh
`libs/stable-diffusion/source/ggml` from the latest upstream `ggml` release,
replace `libs/stable-diffusion/source`, and compile locally.

Available setup flags:

- `--source-release-tag TAG`
  Override the upstream release tag used for the vendored source snapshot
- `--ggml-release-tag TAG`
  Override the upstream `ggml` release tag used for the vendored `ggml` subtree

Example:

```bat
scripts\setup_windows.bat --cuda
```

Pin a specific source snapshot:

```bat
scripts\setup_windows.bat --cuda --source-release-tag master-666-7948df8
```

Pin both upstream trees explicitly:

```bat
scripts\setup_windows.bat --cuda --source-release-tag master-666-7948df8 --ggml-release-tag v0.9.11
```

The legacy-named helper below now does the same source-refresh job instead of
staging prebuilt runtime binaries:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\download-stable-diffusion-release.ps1 -SourceReleaseTag master-666-7948df8
```

## Current Pin

The vendored tree includes the required submodules so the native rebuild scripts
can run end-to-end.

- Upstream release tag: `master-666-7948df8` (published 2026-06-01)
- Vendored on: `2026-06-01`
- Default rebuilds use this release tag unless you override it via
  `--source-release-tag` / `-SourceReleaseTag`.

## Header Notes

Current upstream master uses a newer parameter-struct based C API. The addon
keeps `libs/stable-diffusion/include/stable-diffusion.h` only as a passthrough
include to the vendored upstream header, so addon code should use the current
upstream names directly.

The native rebuild script stages the diffusion runtime pair:

- `stable-diffusion.dll`
- `stable-diffusion.lib`

It also stages a separate `ggml` surface when those artifacts are emitted by the
build:

- `ggml.h`, `gguf.h`, and the rest of `ggml/include/*`
- `ggml.lib`
- `ggml-base.lib`
- `ggml-cpu.lib`
- backend libs such as `ggml-cuda.lib`, `ggml-vulkan.lib`, and `ggml-metal.lib`

This does not change the addon/runtime boundary by itself. It only makes the
separately built `ggml` artifacts available under predictable addon-local paths.

## Recommended Pinning Policy

When vendoring upstream:

1. Choose a known-compatible upstream commit.
2. Record that commit hash in this document and the changelog.
3. Rebuild the staged header/library pair together.
4. Re-run the wrapper test suite.
