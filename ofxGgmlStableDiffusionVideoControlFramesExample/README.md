# Video Control Frames Example

Focused `ofxImGui` example for VACE-style guided video generation with per-frame control images.
Use this example when video structure comes from an ordered frame folder. Use
the plain video example for prompt-only or single-image video generation.

## Features

- Load a folder of image control frames
- Configure WAN/VACE diffusion, UMT5 / T5XXL text encoder, and VAE paths from the panel
- Generate a guided video request with `controlFrames`
- Warn when loaded control-frame count and requested frame count differ
- Tune VACE strength, WAN high-noise overrides, and native cache settings
- Preview generated frames
- Save frame sequences with metadata or export a video file

## Usage

1. Place a VACE/video-capable diffusion model, UMT5 / T5XXL encoder, and VAE in `bin/data/models/`.
2. Place control frames in `bin/data/control_frames/`.
3. Update paths in the panel if needed.
4. Click **Load Control Frames** and then **Generate Guided Video**.

Optional environment variables can prefill the paths:
`OFXGGML_STABLE_DIFFUSION_VACE_MODEL`, `OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER`,
and `OFXGGML_STABLE_DIFFUSION_VAE`.

SPACE starts generation, and Esc or C requests cancellation.
