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

- Use `ofxGgmlCore` as the default ggml/runtime provider for ecosystem builds.
- Keep bundled stable-diffusion.cpp ggml available only as an explicit fallback
  via `scripts\build-stable-diffusion.ps1 -UseBundledGgml`.
- The native build defaults the Core provider path to `..\ofxGgmlCore`, and
  doctor reports whether the staged runtime is Core/system ggml, standalone, or
  unknown.
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

## Migration Notes

- Existing `ofxStableDiffusion` users should migrate to
  `ofxGgmlStableDiffusion`; this repository is the managed continuation of that
  addon under the ofxGgml naming scheme.
- Legacy `ofxGgml` users should move text/chat/embedding workflows to
  `ofxGgmlLlama`, segmentation workflows to `ofxGgmlSam`, and shared ggml
  runtime setup to `ofxGgmlCore`.
- Do not copy dirty legacy folders wholesale. Promote individual improvements
  only when they match a managed lane and have focused validation.
