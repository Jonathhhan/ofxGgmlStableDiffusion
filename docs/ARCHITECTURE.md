# Architecture

`ofxGgmlStableDiffusion` is structured as an openFrameworks-friendly wrapper around a
bundled `stable-diffusion.cpp` runtime, with a bias toward:

- A stable addon-facing API
- Background-thread generation
- Compatibility with the existing `ofxStableDiffusion` addon model
- Native-runtime isolation so upstream `ggml` changes do not destabilize the addon

## Layers

### 1. Public addon wrapper

Files:

- `src/ofxGgmlStableDiffusion.h`
- `src/ofxGgmlStableDiffusion.cpp`

Responsibilities:

- Present a typed openFrameworks API
- Preserve the legacy `newSdCtx` / `txt2img` / `img2img` / `img2vid` surface
- Store owned image/video result objects
- Mirror enough public state to remain usable from older calling code

### 2. Typed addon data model

Files:

- `src/core/ofxGgmlStableDiffusionEnums.h`
- `src/core/ofxGgmlStableDiffusionTypes.h`

Responsibilities:

- Define addon-level tasks and video modes
- Separate context settings, image requests, video requests, upscaler settings
- Represent owned results (`ofxGgmlStableDiffusionResult`, `ofxGgmlStableDiffusionVideoClip`)

### 3. Background execution

Files:

- `src/ofxGgmlStableDiffusionThread.h`
- `src/ofxGgmlStableDiffusionThread.cpp`

Responsibilities:

- Own the native contexts
- Run load/generation/upscale work off the main thread
- Forward progress callbacks
- Hand raw native outputs back to the wrapper for conversion into owned results

### 4. Video helpers

Files:

- `src/video/ofxGgmlStableDiffusionVideo.cpp`
- `src/video/ofxGgmlStableDiffusionVideoHelpers.h`

Responsibilities:

- Build wrapper-owned frame sequences
- Apply presentation modes such as `Loop`, `PingPong`, and `Boomerang`
- Provide a small pure helper seam for unit tests

### 4b. Image ranking helpers

Files:

- `src/core/ofxGgmlStableDiffusionRankingHelpers.h`

Responsibilities:

- Define selection modes such as `KeepOrder`, `Rerank`, and `BestOnly`
- Support Best-of-N output scoring without importing a CLIP runtime directly
- Provide a clean callback seam for optional companion-addon CLIP integration

### 5. Native runtime bundle

Files:

- `libs/stable-diffusion/include`
- `libs/stable-diffusion/lib`
- `libs/stable-diffusion/source`
- `scripts/build-stable-diffusion.*`

Responsibilities:

- Keep the upstream runtime version pinned inside the addon
- Rebuild and stage headers/libs into predictable addon paths
- Avoid sharing the low-level `ggml` build from `ofxGgml`

## Integration Policy

The recommended integration model is:

- `ofxGgmlStableDiffusion` owns its native `stable-diffusion.cpp` and `ggml` runtime
- companion addons or app layers talk to `ofxGgmlStableDiffusion` through the addon API
- Shared behavior should live at the wrapper layer, not in a shared native binary

This keeps the addon safer across upstream `ggml` changes, backend toggles, and
ABI differences.

More detail: [STAGING_PLAN.md](STAGING_PLAN.md)

## `sd-cli` Parity Policy

When wrapper behavior and `sd-cli` behavior disagree, treat `sd-cli` as the
reference path.

Simple rule:

- help if it helps
- don't "help" by changing behavior away from the working backend path
- think twice before changing behavior or UI flow
- be consistent: if a model or mode has no real choice, show that consistently
  instead of inventing a different interaction pattern

Rules for addon-side defaults and UI wiring:

- Prefer backend defaults over addon-side guesses.
- Only pass values the user explicitly set, or values that `sd-cli` also sets
  explicitly.
- Leave native settings unset when `sd-cli` leaves them unset, so the backend
  can resolve the model default.
- Treat every addon-side explicit default as suspicious until it is verified
  against `sd-cli`.
- Do not silently change prompts or inject tuning values in the wrapper/UI.

This policy exists because addon-side "helpful" overrides can make generation
worse, break parity, and waste debugging time.
