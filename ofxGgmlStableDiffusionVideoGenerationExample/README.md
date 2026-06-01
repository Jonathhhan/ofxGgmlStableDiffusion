# Video Generation Example

Focused `ofxImGui` example for the typed `generateVideo()` surface.

## Features

- Configure prompt, dimensions, frame count, fps, steps, CFG, guidance, and seed
- Configure WAN diffusion, UMT5 / T5XXL text encoder, and VAE paths from the panel
- Generate text-to-video by default
- Optionally load an input image for image-to-video capable models
- Morph between prompt keyframes
- Run seed-sequence frame sweeps
- Use an optional end frame for video morphing-capable WAN models
- Preview generated frames
- Save frame sequences with metadata or export a video file

## Usage

1. Place a WAN/video-capable diffusion model, UMT5 / T5XXL encoder, and VAE in `bin/data/models/`.
2. Update the model paths in the panel if needed.
3. Run the example.
4. Click **Generate Video**.
5. Preview frames and save the result.

Optional environment variables can prefill the paths:
`OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL`, `OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER`,
and `OFXGGML_STABLE_DIFFUSION_VAE`.

SPACE starts generation and C requests cancellation.
