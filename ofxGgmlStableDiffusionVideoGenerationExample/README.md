# Video Generation Example

Focused `ofxImGui` example for the typed `generateVideo()` surface.
Use this example for direct text-to-video or image-to-video with WAN-style
model components. Use the control-frames example when each frame needs an
external guide image.

## Features

- Configure prompt, dimensions, frame count, fps, steps, CFG, guidance, and seed
- Configure WAN diffusion, UMT5 / T5XXL text encoder, and VAE paths from the panel
- Restore the last selected model, text encoder, and VAE paths on startup
- Apply model-specific defaults for resolution, frame count, fps, steps, CFG, and strength
- Start from motion-aware prompt presets for text-to-video, image-to-video, and first/last-frame workflows
- Generate text-to-video by default
- Optionally load an input image for image-to-video capable models
- Morph between prompt keyframes
- Run seed-sequence frame sweeps
- Use an optional end frame for video morphing-capable WAN models
- Preview generated frames with Play/Pause or frame scrubbing
- Save frame sequences with metadata or export animated WebP, WebM, or AVI video files

## Usage

1. Place a WAN/video-capable diffusion model, UMT5 / T5XXL encoder, and VAE in `bin/data/models/`.
2. Update the model paths in the panel if needed.
3. Click **Apply Model Defaults** after changing model paths if you want the example to reset video settings for that model family.
4. Click **Configure Context**.
5. Click **Generate Video**.
6. Preview frames with **Play** / **Pause** or the frame slider, then save the result.

For `Wan2.1-T2V-1.3B` models, the example uses the upstream-style 480p starter
recipe: `832x480`, `33` frames, `16` fps, `20` steps, and `0.75` strength.
Other WAN variants use the addon video profile helper for their own defaults.

`Save Frames` writes a PNG sequence plus `metadata.json`. `Save Video` defaults
to animated `.webp` because it is the most reliable upstream export path on
Windows. Use the export format selector to try `.webm` or the `.avi` fallback.

Optional environment variables can prefill the paths:
`OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL`, `OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER`,
and `OFXGGML_STABLE_DIFFUSION_VAE`.

Set `OFXGGML_STABLE_DIFFUSION_AUTO_LOAD=1` to configure the context during
startup. Without it, the example restores paths but waits for the
**Configure Context** button so the GUI stays responsive.

SPACE starts generation, and Esc or C requests cancellation.
