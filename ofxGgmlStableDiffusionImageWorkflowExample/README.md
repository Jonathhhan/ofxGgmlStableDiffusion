# Image Workflow Example

Focused `ofxImGui` example for typed image workflows beyond basic text-to-image.

## Features

- Text-to-image, image-to-image, and inpainting modes
- Input image and mask loading
- Optional ControlNet guide image and strength
- Prompt, negative prompt, CFG, strength, steps, seed, and batch controls
- Cancel and save result actions

## Usage

1. Place an image-generation model in `bin/data/models/`.
2. Optionally place a ControlNet model in `bin/data/models/controlnet/`.
3. Update paths in `ofApp::configureContext()` if needed.
4. Run the example.
5. Choose the workflow mode and click **Generate**.

SPACE starts generation and C requests cancellation.
