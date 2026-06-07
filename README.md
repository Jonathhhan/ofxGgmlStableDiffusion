# ofxGgmlStableDiffusion

`ofxGgmlStableDiffusion` is an openFrameworks addon that wraps
[`stable-diffusion.cpp`](https://github.com/leejet/stable-diffusion.cpp) for
text-to-image, image-to-image, image-to-video, and upscaling workflows.

This addon is seeded from the existing `ofxStableDiffusion` implementation and
keeps that wrapper-first stable-diffusion.cpp backend design, with the public
addon name and API surface moved under the `ofxGgmlStableDiffusion` line.

Current addon version: `1.0.2`

## Staging Scope

This repo is the `ofxStableDiffusion` addon lineage under the `ofxGgml*`
naming scheme. The base is `ofxStableDiffusion` as an addon: its wrapper shape,
example flow, scripts, docs, and native runtime expectations. It is not a
replacement backend and it is not based on `ofxGgmlDiffusion`.

- Keep the proven `ofxStableDiffusion` structure first; `stable-diffusion.cpp`
  remains because it is the inherited native runtime from that addon.
- Keep wrapper APIs, examples, and docs close to `ofxStableDiffusion` while the
  rename settles.
- Treat `ofxGgmlDiffusion`, GGUF GAN experiments, and unrelated model workflows
  as out of scope for this addon.
- Use `ofxGgmlCore` as the default ggml provider for ecosystem builds. A
  bundled ggml fallback remains available with `-UseBundledGgml` for
  compatibility testing and bisecting.

## Requirements

- **openFrameworks**: 0.11.0 or later (tested with 0.12.0)
- **C++ Standard**: C++17 or later
- **Platform Support**: Windows (x64), Linux (x64), macOS (experimental Metal support)

The addon is now structured more like a production addon:

- typed request/config/result objects
- background-thread generation
- owned image/video outputs
- explicit native-library staging
- wrapper-level callback seams for optional companion-addon scoring
- lightweight unit tests for the new video helper layer

## Highlights

- Text-to-image and image-to-image generation
- Native image modes: `TextToImage`, `ImageToImage`, and `Inpainting`
- Best-of-N image reranking through a callback seam that can be driven by an
  external CLIP scorer
- Image-to-video generation with `Standard`, `Loop`, `PingPong`, and `Boomerang` presentation modes
- `ofxGgmlStableDiffusionRealtimeVideoSession` for live creative-loop previews using prompt coalescing, low-step frames, and optional previous-frame img2img feedback
- ESRGAN upscaling support
- Progress callbacks for diffusion steps
- `getCapabilities()` plus capability/model-family helpers for UI gating and backend-aware workflows
- parameter-tuning helpers for image/video defaults, ranges, and clamping by model family
- Optional `ofxGgmlStableDiffusionHoloscanBridge` scaffold for live `frame -> conditioning -> diffusion -> preview` pipelines, with a native Holoscan runtime path on Linux and a clean fallback path when Holoscan is not installed or the platform is not supported yet
- `ofxGgmlStableDiffusionVideoWorkflowHelpers.h` for reusable video-generation presets, request validation, and richer render-manifest export on top of the existing request/result layer
- Legacy entry points still available for wrapper-level migration
- Core-backed ggml runtime management by default, with bundled ggml fallback staging

## Repo Layout

- `src/`
  Addon wrapper, typed request/result model, and background-thread integration
- `src/core/`
  Enums and owned result types
- `src/video/`
  Video clip behavior, pure helper utilities, and reusable workflow helpers for:
  - `FastPreview`, `LowVram`, `Balanced`, `Quality`, and `BatchStoryboard` presets
  - preflight request validation before a longer render starts
  - richer JSON render manifests that capture prompt, dimensions, fps, seed, LoRAs, and clip summary data
- `libs/stable-diffusion/`
  Bundled header/libs and vendoring location for upstream native source
- `scripts/`
  Native rebuild scripts
- `tests/`
  Lightweight CMake-based unit tests
- `docs/`
  Architecture and native-build notes

## Feature Readiness

| Surface | Status | Notes |
| --- | --- | --- |
| Typed image generation (`generate`) | Stable | Primary production-ready wrapper API |
| Typed video generation (`generateVideo`) | Stable | Includes owned frame results and metadata export |
| Legacy compatibility entry points | Supported | Kept for migration; new work should prefer typed requests |
| `ofxGgmlStableDiffusionRealtimeSession` | Stable | Call `update()` every frame; callbacks fire from `update()` |
| `ofxGgmlStableDiffusionRealtimeVideoSession` | Experimental | Useful preview workflow, but still evolving |
| `ofxGgmlStableDiffusionCreativeWorkflow` | Experimental | Unifies realtime preview with queued higher-quality image/video renders and session snapshots |
| `ofxGgmlStableDiffusionHoloscanBridge` | Experimental | Linux-first runtime path with fallback behavior elsewhere |
| `ofxGgmlStableDiffusionBatchProcessor` | Experimental | Runs generator-backed grids, sweeps, A/B comparisons, and batch artifact export |

## Features

- stable-diffusion.cpp wrapper lane
- typed image generation requests
- image-to-video helper workflows
- ofxGgmlCore ggml provider by default
- bundled ggml fallback runtime staging
- local wrapper validation entrypoint

## Threading Contract

- `ofxGgmlStableDiffusion` is internally synchronized unless a method explicitly documents borrowed-pointer or blocking behavior.
- `generate()`, `generateVideo()`, `configureContext()`, and `setUpscalerSettings()` may be called from any thread, but only one long-running task can run at a time.
- `isGenerating()`, `isBusy()`, `requestCancellation()`, `isCancellationRequested()`, and copied-result accessors (`getLastResult()`, `getImages()`, `getVideoClip()`) are safe to query from any thread.
- `getImagePixels()`, `getVideoFramePixels()`, and `returnImages()` expose borrowed buffers; use them only with care and never cache the returned pointers across generations.
- `setProgressCallback()` and `setImageRankCallback()` run on the worker thread. Keep those callbacks lightweight and defer UI / texture updates to your main `update()` / `draw()` flow.
- `ofxGgmlStableDiffusionRealtimeSession` and `ofxGgmlStableDiffusionRealtimeVideoSession` dispatch their callbacks from whichever thread calls `update()`.

## API Shape

The addon now supports two layers of use:

### Modern typed wrapper API

```cpp
#include "ofxGgmlStableDiffusion.h"

ofxGgmlStableDiffusion sd;

ofxGgmlStableDiffusionContextSettings context;
context.modelPath = "data/models/sd/sd_turbo.safetensors";
context.nThreads = 8;
context.weightType = SD_TYPE_COUNT;
sd.configureContext(context);

ofxGgmlStableDiffusionImageRequest request;
request.prompt = "cinematic portrait, rim lighting";
request.width = 512;
request.height = 512;
request.sampleSteps = 20;
sd.generate(request);

// Optional: apply a stack of LoRA/LoCon adapters per request
ofxGgmlStableDiffusionLora loraA;
loraA.path = "data/loras/edge.safetensors";
loraA.strength = 0.7f;
request.loras = {loraA};
sd.generate(request);

// Update the active LoRA stack globally
sd.setLoras({loraA});

// Hot-reload textual-inversion embeddings (reloads the context)
sd.reloadEmbeddings("data/embeddings");

// List currently discoverable embeddings (name, absolute path)
const auto embeddings = sd.listEmbeddings();

// Inspect what the currently configured model/runtime can actually do
const auto capabilities = sd.getCapabilities();

// Save generated video frames plus a JSON sidecar with per-frame prompts/seeds
sd.saveVideoFramesWithMetadata("output/storyboard", "shot");
```

The parameter-tuning helpers can also be used outside the wrapper instance when
you want model-family-aware defaults before building a request:

```cpp
const auto imageProfile =
    ofxGgmlStableDiffusionParameterTuningHelpers::resolveImageProfile(
        context,
        ofxGgmlStableDiffusionImageMode::ImageToImage);

request.cfgScale = imageProfile.defaultCfgScale;
request.sampleSteps = imageProfile.defaultSampleSteps;
request.strength = imageProfile.defaultStrength;
```

### Error Handling

The addon provides advanced error handling with error codes, messages, suggestions, and history tracking:

```cpp
ofxGgmlStableDiffusionImageRequest request;
request.width = 513;  // Invalid: not a multiple of 64
request.height = 512;
request.batchCount = 20;  // Invalid: exceeds maximum of 16

sd.generate(request);

// Check for errors programmatically
if (sd.getLastErrorCode() != ofxGgmlStableDiffusionErrorCode::None) {
    // Get detailed error information
    const auto& errorInfo = sd.getLastErrorInfo();
    ofLogError() << "Error: " << errorInfo.message;
    ofLogError() << "Suggestion: " << errorInfo.suggestion;
    ofLogError() << "Error code: " << ofxGgmlStableDiffusionErrorCodeLabel(errorInfo.code);
}

// Or use the simple string-based error (backward compatible)
if (!sd.getLastError().empty()) {
    ofLogError() << "Error: " << sd.getLastError();
}

// Access error history for debugging
const auto& history = sd.getErrorHistory();
for (const auto& error : history) {
    ofLogNotice() << "[" << error.timestampMicros << "] "
                  << error.message << " -> " << error.suggestion;
}

// Clear error history
sd.clearErrorHistory();
```

**Available Error Codes:**
- `None` - No error
- `ModelNotFound` - Model file not found
- `ModelCorrupted` - Model file corrupted
- `ModelLoadFailed` - Model loading failed
- `OutOfMemory` - Insufficient memory
- `InvalidDimensions` - Invalid width/height
- `InvalidBatchCount` - Invalid batch count
- `InvalidFrameCount` - Invalid video frame count
- `MissingInputImage` - Input image required but not provided
- `GenerationFailed` - Generation process failed
- `ThreadBusy` - Another task is running
- `UpscaleFailed` - Upscaling failed
- `Unknown` - Unknown error

Each error code automatically provides an actionable suggestion to help resolve the issue.

### Legacy compatibility API

The older `newSdCtx`, `txt2img`, `img2img`, and `img2vid` entry points are still
available so existing call sites, including addon-to-addon bridges, do not have
to migrate immediately.

### Realtime Creative Loop

`ofxGgmlStableDiffusionRealtimeVideoSession` adapts the interaction pattern of
realtime video tools to the local stable-diffusion.cpp backend. It keeps only the
latest prompt while a frame is generating, uses low-step preview requests for
responsiveness, can refine an idle prompt with a higher step budget, and can feed
the previous output frame back through img2img for visual continuity.

```cpp
#include "ofxGgmlStableDiffusion.h"

ofxGgmlStableDiffusionRealtimeVideoSettings liveSettings;
liveSettings.previewSteps = 4;
liveSettings.refineSteps = 16;
liveSettings.previewStrength = 0.68f;
liveSettings.refineStrength = 0.35f;
liveSettings.usePreviousFrameFeedback = true;

ofxGgmlStableDiffusionRealtimeVideoSession liveVideo;
liveVideo.start(liveSettings, sd);

ofxGgmlStableDiffusionRealtimeVideoRequest liveRequest;
liveRequest.prompt = "a fashion portrait reacting to colored stage lights";
liveVideo.submit(liveRequest);

// In update():
liveVideo.update();
```

This is a near-realtime preview workflow, not a proprietary HD world model. For
best results use fast models, modest preview dimensions, and low preview step
counts.

## Video Behavior

Video generation returns owned frames through `ofxGgmlStableDiffusionVideoClip`.

Animated requests can also drive per-frame prompt interpolation, parameter
animation, and seed sequencing through `ofxGgmlStableDiffusionVideoRequest::animationSettings`.
Each returned frame now carries the prompt / negative prompt / CFG / strength /
seed values that were actually used, and clips can export both PNG sequences and
JSON metadata.

Useful video-export helpers:

- `ofxGgmlStableDiffusionVideoClip::saveMetadataJson(...)`
- `ofxGgmlStableDiffusionVideoClip::saveFrameSequenceWithMetadata(...)`
- `ofxGgmlStableDiffusionVideoClip::saveWebm(...)`
- `ofxGgmlStableDiffusion::saveVideoMetadata(...)`
- `ofxGgmlStableDiffusion::saveVideoFramesWithMetadata(...)`

`saveWebm(...)` accepts `.webp`, `.webm`, and `.avi` paths. `.webp`/`.webm`
use the upstream stable-diffusion.cpp media writer when the rebuilt runtime
stages its media libraries; `.avi` remains a small local MJPEG fallback for
compatibility and debugging.

Long video generation (chunked rendering):

- `ofxGgmlStableDiffusionLongVideoManifest` + `ofxGgmlStableDiffusionLongVideoChunk`
  describe a long-form timeline as multiple short clips.
- `ofxGgmlStableDiffusion::renderLongVideo(...)` renders all chunks sequentially,
  optionally handing off the previous chunk's last frame as the next init image,
  and saves each chunk's frame sequence + `metadata.json`.

Supported playback/presentation modes:

- `Standard`
  Return generated frames as-is
- `Loop`
  Repeat the first frame at the end for easier closed-loop playback
- `PingPong`
  Play forward, then back through the interior frames
- `Boomerang`
  Play forward, then fully reverse including the endpoints

This is especially useful when a UI layer or another addon wants a more natural
preview clip without re-asking the native runtime for more frames.

### Video Performance Recommendations

- **Use fast models for previews**: Prefer LCM/Turbo-style checkpoints with 4-8 steps for quick iteration; only re-run hero frames at higher quality.
- **Quantize when possible**: F16 or Q8/Q5 levels often cut VRAM use by 50-75% and speed up inference, enabling higher resolutions or longer clips without swapping.
- **Keep the model warm and resident**: Reuse the same loaded context via the model manager; avoid swapping checkpoints mid-run and consider a throwaway warmup frame to eliminate first-run latency.
- **Minimize unique frames**: Generate the smallest necessary source frame count, then stretch duration with `PingPong`, `Boomerang`, or `Loop` playback instead of regenerating.
- **Right-size resolution, fps, and steps**: Lower resolution and fps where acceptable; clamp `sampleSteps` to ~15-25 for finals (lower for previews). Per-frame time scales directly with unique frames.
- **Trim preview I/O**: Skip metadata/JSON exports on preview passes; save sidecar files only on final renders to avoid extra disk churn.

## Coding Conventions (openFrameworks-aligned)

- Use tabs (as in existing headers) with K&R braces; lowerCamelCase for functions/members and PascalCase for types, keeping the `ofxGgmlStableDiffusion*` prefix for public types.
- Include order: `ofMain.h`, then addon headers, then STL/system headers.
- Document public API with `///` Doxygen-style comments; avoid mixing `//` for header docs.
- Avoid exceptions; return `bool` or error codes and populate `ofxGgmlStableDiffusionError` for failures.
- Keep small helpers inline in headers; move heavier logic to `.cpp` files to limit inline bloat.
- Prefer RAII and STL containers over raw `new`/`delete`.
- Keep UI, textures, and other openFrameworks rendering objects on the main thread; worker-thread callbacks should only capture/copy lightweight state.
- Favor openFrameworks core types at the API edge: `ofPixels`/`ofImage` for images, `ofJson` for metadata, `ofVec*`/`ofFloatColor`/`ofRectangle` where geometry or color is needed. Convert to STL or native structs internally only when necessary for performance or binding.

## Image Modes

The typed image request layer exposes the native image modes:

- `TextToImage`
- `ImageToImage`
- `Inpainting`

## CLIP-Rerank Integration

`ofxGgmlStableDiffusion` now exposes a wrapper-level image ranking callback for
Best-of-N workflows.

That is the intended integration point for companion-addon CLIP scoring:

- generate a batch in `ofxGgmlStableDiffusion`
- score the outputs with a CLIP runtime owned by another addon or app layer
- rerank or collapse to the best image without sharing native runtimes

This keeps diffusion and CLIP loosely coupled while still enabling a strong
cross-addon workflow.

## Companion Addon Integration

- use `ofxGgmlCore` as the shared ggml/runtime base for companion addons
- integrate companion addons with it through the addon API
- keep model-specific workflow logic in companion addons, not in Core

Why:

- Core gives the ecosystem one managed ggml provider by default
- wrapper-level integration keeps model behavior inside the stable-diffusion lane
- the bundled ggml fallback remains available if a stable-diffusion.cpp update
  needs an isolated compatibility build

More detail: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
[docs/STAGING_PLAN.md](docs/STAGING_PLAN.md)

## Native Runtime

The addon stages native artifacts into addon-local paths:

- `libs/stable-diffusion/include`
- `libs/stable-diffusion/lib/vs`
- `libs/stable-diffusion/lib/Linux64`
- `libs/ggml/include` (bundled fallback builds only)
- `libs/ggml/lib/vs` (bundled fallback builds only)
- `libs/variants/<backend>/...`

Rebuild helpers:

- `scripts/build-stable-diffusion.ps1`
- `scripts/build-stable-diffusion.bat`
- `scripts/build-stable-diffusion.sh`
- `scripts/download-stable-diffusion-release.ps1`
- `scripts/setup_addon.ps1`
- `scripts/setup_windows.bat`

More detail: [docs/NATIVE_BUILD.md](docs/NATIVE_BUILD.md)

Backend flags now follow the same style as `ofxGgml`, but the build selects one
backend per build:

- `--cpu`, `--cpu-only` / `-CpuOnly`
  Force CPU-only native builds (default)
- `--gpu`, `--cuda` / `-Cuda`
  Enable CUDA explicitly
- `--vulkan` / `-Vulkan`
  Enable Vulkan explicitly
- `--metal` / `-Metal`
  Enable Metal explicitly where supported
- `--all` / `-All`
  Build every available backend variant and leave the canonical runtime on the
  best one in this priority order: `cuda`, then `vulkan`, then `cpu-only`

Each backend build is also snapshotted under `libs/variants/<backend>`, so you
can switch the canonical addon runtime later without rebuilding everything.

Variant selector helpers:

- `scripts/select-stable-diffusion-backend.ps1 -Backend cuda`
- `scripts/setup_windows.bat --skip-native --select-backend cuda`

For Windows, `scripts/setup_windows.bat` and `scripts/setup_addon.ps1` now
always refresh the vendored source from the latest upstream release-tag source
snapshot, then build the native runtime locally.

- `--source-release-tag TAG`
  Override the upstream release tag used for the vendored source snapshot

When an upstream Windows zip does not include `stable-diffusion.lib`, the addon
setup script synthesizes the Visual Studio import library from the downloaded
DLL exports automatically.

## Current Native Source Status

The repo now includes a vendored upstream `stable-diffusion.cpp` source snapshot
under `libs/stable-diffusion/source`, pinned to:

- upstream repo: `https://github.com/leejet/stable-diffusion.cpp`
- upstream release tag: `master-666-7948df8`
- upstream commit: `7948df8`
- vendored on: `2026-06-01`

The optional Windows prebuilt-runtime flow is currently pinned to the upstream
GitHub release tag `master-666-7948df8`, which was the latest upstream release
published on `2026-06-01`. Override it with `--source-release-tag` if you want a
different upstream runtime. Source: [stable-diffusion.cpp releases](https://github.com/leejet/stable-diffusion.cpp/releases)

The addon now includes the upstream header directly through
`libs/stable-diffusion/include/stable-diffusion.h`, without re-exporting the
older enum aliases. Addon code should use the current upstream names such as
`EULER_A_SAMPLE_METHOD`, `DPMPP2Mv2_SAMPLE_METHOD`, and `SCHEDULER_COUNT`.

## Testing

The test suite focuses on wrapper-level logic that should stay stable even when
the native runtime changes.

Run:

```bash
cmake -S tests -B tests/build
cmake --build tests/build --config Release
ctest --test-dir tests/build -C Release --output-on-failure
```

You can also use:

- `scripts/run-tests.ps1`
- `scripts/run-tests.sh`
- `scripts/run-stable-diffusion-runtime-smoke.ps1 -Json -SummaryOnly`

Test notes: [tests/README.md](tests/README.md)

## Starter Example

The canonical `ofxGgmlStableDiffusionExample/` project is intentionally small:
load a model, enter a prompt, generate, cancel, and save one image. Use the
focused root-level examples below for deeper image controls, image workflows,
video, control frames, creative loops, LoRA stacks, and embeddings.

## Troubleshooting

### Model Loading Issues

**Problem**: Model fails to load or crashes during loading
- **Solution**: Verify the model file exists and is not corrupted
- **Solution**: Check that the model format is compatible (`.safetensors`, `.ckpt`, or `.gguf`)
- **Solution**: Ensure sufficient RAM/VRAM (models typically require 4-8GB+)

### Out of Memory Errors

**Problem**: Generation fails with OOM error
- **Solution**: Reduce batch count (try `batchCount = 1`)
- **Solution**: Reduce image dimensions (try 512x512 instead of higher)
- **Solution**: Enable VAE tiling with `vaeTiling = true`
- **Solution**: Use quantized models (Q4_0, Q5_0, etc.) for lower memory usage

### Slow Generation

**Problem**: Image generation takes too long
- **Solution**: Increase thread count with `nThreads = -1` (auto-detect) or set manually
- **Solution**: Reduce sample steps (try 10-20 steps for faster results)
- **Solution**: Use smaller models like SD-Turbo for faster iteration
- **Solution**: Enable GPU backend if available (`--cuda` or `--vulkan` during build)

### Thread Safety

**Note**: The addon manages its own worker thread and internally serializes long-running work. You may call `generate()` / `generateVideo()` from any thread, but overlapping starts still fail with `ThreadBusy`.

### Invalid Input Dimensions

**Problem**: Generation fails with dimension errors
- **Solution**: Ensure width and height are positive multiples of 64 (e.g., 512, 768, 1024)
- **Solution**: Most SD models work best with dimensions between 256-1024

## Thread Safety Notes

- The addon manages its own background thread for generation
- Only one long-running task can run at a time; overlapping starts fail with `ThreadBusy`
- Use `isGenerating()` / `isBusy()` to observe state from any thread
- Prefer copied accessors such as `getLastResult()`, `getImages()`, and `getVideoClip()` when crossing thread boundaries
- Progress and ranking callbacks are fired from the worker thread; do not touch textures/UI directly inside them

## Documentation

Comprehensive documentation is available:

- **[API Reference](docs/API_REFERENCE.md)** - Complete API documentation with classes, methods, and types
- **[Migration Guide](docs/MIGRATION_GUIDE.md)** - Upgrade from legacy API to modern request-based API
- **[Troubleshooting Guide](docs/TROUBLESHOOTING.md)** - Common issues and solutions
- **[Architecture Notes](docs/ARCHITECTURE.md)** - Design decisions and integration patterns
- **[Native Build Guide](docs/NATIVE_BUILD.md)** - Building stable-diffusion.cpp from source

### Code Examples

The addon root contains working sample applications, following the openFrameworks
addon convention used by addons such as `ofxMidi`:

- **[ofxGgmlStableDiffusionExample](ofxGgmlStableDiffusionExample/)** - Canonical starter; load, prompt, generate, cancel, and save
- **[ofxGgmlStableDiffusionBasicGenerationExample](ofxGgmlStableDiffusionBasicGenerationExample/)** - Expanded typed text-to-image control panel
- **[ofxGgmlStableDiffusionImageWorkflowExample](ofxGgmlStableDiffusionImageWorkflowExample/)** - Image-to-image, inpainting, and ControlNet guide workflows
- **[ofxGgmlStableDiffusionVideoGenerationExample](ofxGgmlStableDiffusionVideoGenerationExample/)** - Focused typed video generation flow
- **[ofxGgmlStableDiffusionVideoControlFramesExample](ofxGgmlStableDiffusionVideoControlFramesExample/)** - VACE-style guided video with per-frame controls
- **[ofxGgmlStableDiffusionCreativeLoopExample](ofxGgmlStableDiffusionCreativeLoopExample/)** - Realtime prompt preview/refine loop
- **[ofxGgmlStableDiffusionLoraEmbeddingExample](ofxGgmlStableDiffusionLoraEmbeddingExample/)** - LoRA adapter stacks and textual-inversion embeddings

Each interactive example exposes cancellation while a long-running load or generation task is active.
Press `Esc` or `C` to cancel/stop in any example; in the creative-loop example, `F` clears the feedback frame.

For WAN context-load smoke testing without rendering, build
`ofxGgmlStableDiffusionVideoGenerationExample` and run:

```powershell
scripts\run-wan-context-smoke.ps1 `
  -Model "C:\path\to\Wan2.1-T2V-1.3B-Q8_0.gguf" `
  -TextEncoder "C:\path\to\umt5-xxl-encoder-Q8_0.gguf" `
  -Vae "C:\path\to\wan_2.1_vae.safetensors"
```

### Quick Links

- [Model Family Capabilities](docs/API_REFERENCE.md#model-families-and-capabilities)
- [Error Handling](docs/API_REFERENCE.md#error-handling)
- [Performance Tips](docs/API_REFERENCE.md#performance-optimization)
- [Platform Support](docs/API_REFERENCE.md#platform-support)
- [Cancellation API](docs/API_REFERENCE.md#cancellation)

## Changelog

See [CHANGELOG.md](CHANGELOG.md).
