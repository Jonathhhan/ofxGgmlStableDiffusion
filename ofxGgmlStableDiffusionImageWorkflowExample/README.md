# Image Workflow Example

Focused `ofxImGui` example for typed image workflows beyond basic text-to-image.
Use this example when the interesting part is the source image, mask, or
ControlNet guide image rather than the full parameter surface.

## Features

- Text-to-image, image-to-image, and inpainting modes
- Editable model and optional ControlNet model paths
- Input image and mask loading
- Optional ControlNet guide image and strength
- Prompt, negative prompt, CFG, strength, steps, seed, and batch controls
- Cancel and save result actions

## Usage

1. Place an image-generation model in `bin/data/models/`.
2. Optionally place a ControlNet model in `bin/data/models/controlnet/`.
3. Update or browse model paths from the panel if needed.
4. Run the example.
5. Click **Configure Context** after changing model paths.
6. Choose the workflow mode and click **Generate**.

SPACE starts generation, and Esc or C requests cancellation.
