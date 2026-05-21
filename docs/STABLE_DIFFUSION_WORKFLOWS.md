# Stable Diffusion Workflows

This guide is the agent-facing workflow boundary for `ofxGgmlStableDiffusion`.

## Lane Ownership

`ofxGgmlStableDiffusion` owns the stable-diffusion.cpp wrapper lane:

- text-to-image and image-to-image generation
- inpainting and upscaling wrapper behavior
- stable-diffusion.cpp native runtime staging
- addon examples and validation around the inherited `ofxStableDiffusion` flow

`ofxGgmlDiffusion` is intentionally paused and should stay out of managed
ecosystem automation while this lane carries stable-diffusion.cpp work.

## Guardrails

- Keep the default runtime standalone.
- Do not introduce a default dependency on `ofxGgmlCore` or shared ggml
  binaries.
- Do not replace stable-diffusion.cpp with unrelated GAN, GGUF GAN, or
  `ofxGgmlDiffusion` workflows.
- Keep model weights, downloaded runtimes, generated media, build output, and
  generated openFrameworks project files out of git.

## Validation

Before handoff, run:

```powershell
scripts\validate-local.ps1
```

For wrapper-only checks, run:

```powershell
scripts\run-tests.ps1
```
