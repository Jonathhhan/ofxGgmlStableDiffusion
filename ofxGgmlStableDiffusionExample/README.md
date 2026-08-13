# Starter Example

Small canonical `ofxImGui` starter for `ofxGgmlStableDiffusion`.

## Features

- Load one Stable Diffusion image model
- Restore the last successfully loaded model, or discover the first supported model in `bin/data/models/`
- Recursively list local models with exact file sizes and rescan without restarting
- Paste model paths and long prompts without fixed-size text buffers
- Select CUDA automatically when the staged runtime contains CUDA support
- Show the selected backend, context state, model size, and VRAM placement policy separately
- Configure and restore native VRAM budgets, automatic device fitting, layer streaming, eager loading, and multi-device split modes
- Generate one text-to-image request
- Display progress and the latest generated image
- Cancel an active load or generation
- Save the current image

## Usage

1. Place an image-generation model (`.safetensors`, `.ckpt`, `.gguf`, or `.ggml`) in `bin/data/models/`, set `OFXGGML_STABLE_DIFFUSION_MODEL`, or paste/browse to its path.
2. Run the example.
3. Click **Load Context** after changing the model path. Successful paths are restored on the next run.
4. Edit the prompt and click **Generate**.

The CUDA runtime is selected automatically when the staged `stable-diffusion.dll`
contains CUDA support. Set `OFXGGML_STABLE_DIFFUSION_BACKEND` to override the
backend explicitly.

Open **Runtime and GPU memory** before loading the context to use the current
native placement controls. `Max VRAM` accepts a single GiB value such as `6`, a
negative reserve such as `-1`, or assignments such as `cuda0=6,cuda1=8`.
`Split mode` may be `layer`, `row`, or a per-module assignment such as
`diffusion=row,te=layer`. These preferences are saved only after a context loads
successfully and are restored on the next run.

SPACE starts generation, C or Esc requests cancellation, and S saves the current image.

## Model-backed openFrameworks smoke

After building the example and staging a native runtime, exercise the actual
openFrameworks wrapper path with a local model:

```powershell
.\scripts\run-image-generation-smoke.ps1 -Model C:\path\to\model.safetensors -Backend cuda
```

The opt-in smoke mode loads the model, generates a 256x256 one-step image,
validates the saved PNG, reports a process exit status, and leaves normal
interactive runs unchanged.
