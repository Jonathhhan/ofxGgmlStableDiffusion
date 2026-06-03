# `ofxGgml` Bridge Notes

This note captures integration options between `ofxGgml` and `ofxGgmlStableDiffusion`.

Status: ecosystem bridge notes. The staged addon baseline is
`ofxStableDiffusion` as an addon, keeps the `stable-diffusion.cpp` wrapper lane,
uses `ofxGgmlCore` as the default ggml provider, and does not use
`ofxGgmlDiffusion`.

## Integration Options

### 1. Addon-Level Integration

`ofxGgml` already has an addon-level bridge in:

- `addons/ofxGgml/src/inference/ofxGgmlStableDiffusionAdapters.h`

That adapter:

- conditionally includes `ofxGgmlStableDiffusion.h`
- creates an image-generation backend named `ofxGgmlStableDiffusion`
- reloads `ofxGgmlStableDiffusion` contexts when model/runtime settings change
- forwards typed requests through the `ofxGgmlStableDiffusion` wrapper API

This remains the recommended workflow integration path because it:
- Keeps stable-diffusion behavior inside this addon
- Lets companion addons call the wrapper API instead of reaching into diffusion internals
- Avoids moving model-specific logic into Core
- Keeps Core focused on the shared ggml/runtime base

### 2. Core/System GGML Integration (Default)

By default, ofxGgmlStableDiffusion consumes GGML from `ofxGgmlCore` at build
time using `stable-diffusion.cpp`'s built-in
`-DSD_USE_SYSTEM_GGML=ON` support.

**Build with system GGML:**

```bash
# Linux/macOS
./scripts/build-stable-diffusion.sh --cuda --ofxggml-path ../ofxGgmlCore

# Windows
.\scripts\build-stable-diffusion.ps1 -Cuda -OfxGgmlPath ..\ofxGgmlCore
```

**What this provides:**
- Single GGML binary shared between stable-diffusion and llama/whisper/etc.
- Reduced disk footprint (one set of GGML libs instead of duplicates)
- Consistent GGML version across all addons

**Important requirements:**
- ofxGgmlCore must be built first
- Backend flags must match (CPU/CUDA/Vulkan)
- GGML versions must be compatible
- Use `-UseBundledGgml` / `--use-bundled-ggml` only for fallback compatibility builds

See [docs/NATIVE_BUILD.md](NATIVE_BUILD.md) for complete system GGML build instructions.

## Staged Runtime Comparison

### `ofxGgmlCore`

`ofxGgmlCore` stages a full `ggml` SDK/runtime:

- public headers in `libs/ggml/include`
- public libs in `libs/ggml/lib`
- direct backend libs such as:
  - `ggml-cuda.lib`
  - `ggml-vulkan.lib`
  - `ggml-cpu.lib`

It also performs runtime backend selection in addon code.

### `ofxGgmlStableDiffusion`

**In bundled fallback mode:**

`ofxGgmlStableDiffusion` stages a diffusion runtime surface only:

- public header in `libs/stable-diffusion/include/stable-diffusion.h`
- staged runtime in `libs/stable-diffusion/lib/...`
  - `stable-diffusion.dll`
  - `stable-diffusion.lib`

It does not stage a public `ggml` SDK surface for other addons to consume.

**In Core/system GGML mode (default):**

`ofxGgmlStableDiffusion` links against ofxGgmlCore's GGML and only stages:
- public header in `libs/stable-diffusion/include/stable-diffusion.h`
- staged runtime in `libs/stable-diffusion/lib/...`
  - `stable-diffusion.dll` (linked against ofxGgmlCore's GGML)
  - `stable-diffusion.lib`

GGML itself comes from ofxGgmlCore in this mode.

## Why Core GGML Is The Default

Core-backed ggml is the recommended default for managed ecosystem builds:

**Stability:**
- One managed provider controls ggml version and backend selection
- Companion addons avoid silently drifting across incompatible ggml builds
- Agent automation has a single place to validate runtime readiness

**Flexibility:**
- The bundled fallback remains available when stable-diffusion.cpp needs isolation
- Backend flags can still be selected per build while using Core as the provider

**Simplicity:**
- Build scripts default to `..\ofxGgmlCore`
- Validation checks the provider path and staged libraries
- Companion projects list Core explicitly

Bundled fallback mode is available for users who:
- Need to bisect a stable-diffusion.cpp/ggml compatibility issue
- Temporarily need an isolated ggml build
- Are working outside the managed ofxGgml ecosystem
