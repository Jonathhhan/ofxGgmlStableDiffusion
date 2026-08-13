# Image Workflow Example

Focused `ofxImGui` example for typed image workflows beyond basic text-to-image.
Use this example when the interesting part is the source image, mask, or
ControlNet guide image rather than the full parameter surface.

## Features

- Text-to-image, image-to-image, and inpainting modes
- Editable model and optional ControlNet model paths
- Pasteable, unbounded model, image, mask, and prompt fields
- Explicit Auto/CPU/CUDA/Vulkan/Metal backend selection
- Input image and mask loading
- Optional output-size matching from the input image with 64-pixel alignment
- Live workflow readiness messages that identify missing input, mask, or ControlNet data
- Optional ControlNet guide image and strength
- Prompt, negative prompt, CFG, strength, steps, seed, and batch controls
- Cancel and save result actions

## Usage

1. Place an image-generation model in `bin/data/models/`.
2. Optionally place a ControlNet model in `bin/data/models/controlnet/`.
3. Update or browse model paths from the panel if needed.
4. Run the example.
5. Click **Configure Context** after changing model paths.
6. Choose the workflow mode. For image-to-image load an input image; for
   inpainting also load a mask where white is repainted and black is preserved.
7. Generate becomes available when the selected workflow is complete.

SPACE starts generation, and Esc or C requests cancellation.
