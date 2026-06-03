# Codex Repository Instructions

This repository is part of the ofxGgml openFrameworks addon ecosystem.

## Addon Scope

- Addon: ofxGgmlStableDiffusion
- Lane: stable-diffusion.cpp image generation
- Role: stable-diffusion.cpp image generation wrapper, examples, native runtime setup, and validation

## Working Rules

- Read the existing code and docs before changing behavior.
- Keep edits scoped to this addon's lane and preserve the companion-addon split.
- Start with an ecosystem plan when a task asks for cross-repo improvement or planning.
- Use ofxGgmlCore as the default shared ggml/runtime base for companion addons; do not add reverse dependencies from Core to companion addons.
- Do not commit generated project files, binaries, model weights, downloaded runtimes, sample media dumps, memory indexes, or caches.
- Prefer focused tests and local validation over broad refactors.
- Use openFrameworks ofLogNotice, ofLogWarning, ofLogError, or module-scoped ofLog(...) for addon runtime/example logging; keep raw stdout/stderr only for tests and CLI tools with machine-readable output contracts.
- Preserve openFrameworks-style public names and document intentional breaking changes.
## Stable Diffusion Lane Contract

Keep this addon as the stable-diffusion.cpp wrapper lane inherited from
ofxStableDiffusion.

Do not replace this backend with ofxGgmlDiffusion, GGUF GAN experiments, or
unrelated image-generation workflows. ofxGgmlDiffusion is paused and should stay
out of managed automation unless explicitly promoted.

Use ofxGgmlCore as the default ggml provider for the stable-diffusion.cpp
runtime. Keep a bundled/standalone ggml fallback available for compatibility and
bisecting, but do not make it the default ecosystem path.

## Validation

Validation before handoff: scripts\validate-local.ps1.

For ecosystem planning work, run scripts\plan-ecosystem.ps1 from ofxGgmlCore
before proposing addon-code changes.

## Ecosystem Notes

Model-specific UX belongs in companion addons. Shared code should move down into
ofxGgmlCore only after it is stable, domain-neutral, dependency-light, and
covered by focused tests.
