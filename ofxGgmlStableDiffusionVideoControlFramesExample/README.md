# Video Control Frames Example

Focused `ofxImGui` example for VACE-style guided video generation with per-frame control images.

## Features

- Load a folder of image control frames
- Generate a guided video request with `controlFrames`
- Tune VACE strength, WAN high-noise overrides, and native cache settings
- Preview generated frames
- Save frame sequences with metadata or export a video file

## Usage

1. Place a VACE/video-capable model in `bin/data/models/video/`.
2. Place control frames in `bin/data/control_frames/`.
3. Update paths in the panel if needed.
4. Click **Load Control Frames** and then **Generate Guided Video**.

SPACE starts generation and C requests cancellation.
