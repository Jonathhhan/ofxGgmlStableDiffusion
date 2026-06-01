# Video Generation Example

Focused `ofxImGui` example for the typed `generateVideo()` surface.

## Features

- Configure prompt, dimensions, frame count, fps, steps, CFG, guidance, and seed
- Generate text-to-video by default
- Optionally load an input image for image-to-video capable models
- Morph between prompt keyframes
- Run seed-sequence frame sweeps
- Use an optional end frame for video morphing-capable WAN models
- Preview generated frames
- Save frame sequences with metadata or export a video file

## Usage

1. Place a WAN/video-capable model in `bin/data/models/video/`.
2. Update the model path in `ofApp::configureContext()` if needed.
3. Run the example.
4. Click **Generate Video**.
5. Preview frames and save the result.

SPACE starts generation and C requests cancellation.
