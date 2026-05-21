# GitHub Copilot Repository Instructions

ofxGgmlStableDiffusion is part of the ofxGgml openFrameworks addon ecosystem.

- Scope: stable-diffusion.cpp image generation wrapper, examples, native runtime setup, and validation
- Keep changes inside this addon's lane unless a task explicitly asks for a cross-addon update.
- For ecosystem planning tasks, prefer instruction, documentation, workflow, and validation changes before addon source changes.
- Use ofxGgmlCore for shared runtime primitives and keep companion workflows out of Core.
- Avoid committing generated outputs, local models, build directories, IDE metadata, downloaded runtimes, caches, or media dumps.
- Use openFrameworks ofLogNotice, ofLogWarning, ofLogError, or module-scoped ofLog(...) for addon runtime/example logging; keep raw stdout/stderr only for tests and CLI tools with machine-readable output contracts.
- Add or update headless tests for public helper behavior.
- Validation before handoff: scripts\validate-local.ps1.
- Keep explanations concise and include the files and checks that matter.
## Stable Diffusion Lane Contract

Keep this addon as the stable-diffusion.cpp wrapper lane inherited from
ofxStableDiffusion.

Do not replace this backend with ofxGgmlDiffusion, GGUF GAN experiments, or
unrelated image-generation workflows. ofxGgmlDiffusion is paused and should stay
out of managed automation unless explicitly promoted.

Keep the default runtime standalone. Do not add a default dependency on
ofxGgmlCore or shared ggml binaries.
