# Basic Generation Example

This example demonstrates an expanded typed image-generation panel for
ofxGgmlStableDiffusion. For the smallest starter path, begin with
[`ofxGgmlStableDiffusionExample`](../ofxGgmlStableDiffusionExample/).

## Features

- Load a Stable Diffusion model
- Tune the request from an ofxImGui control panel
- Generate images from text prompts
- Display progress during generation
- Cancel an active load or generation
- Save generated images

## Usage

1. Place a Stable Diffusion model (`.safetensors` or `.ckpt`) in `bin/data/models/`
2. Update the model path in `ofApp::setup()` if needed
3. Run the example
4. Use the ImGui panel to edit the prompt and generation settings
5. Click **Generate** to start an image
6. Click **Cancel** to stop a long-running task, or **Save** to save the current image

## Code Walkthrough

### Setup (ofApp::setup)

```cpp
ofxGgmlStableDiffusionContextSettings settings;
settings.modelPath = ofToDataPath("models/sd_v1.5.safetensors");
settings.weightType = SD_TYPE_COUNT;
sd.configureContext(settings);
```

This loads the model and configures the generation context.

### Generate (keyPressed)

```cpp
ofxGgmlStableDiffusionImageRequest request;
request.prompt = "A serene mountain landscape at sunset";
request.width = 512;
request.height = 512;
request.sampleSteps = 20;
sd.generate(request);
```

Creates a request with desired parameters and starts generation.
The example keeps the request controls in ofxImGui and still accepts SPACE/S as
keyboard shortcuts for quick smoke testing.

### Progress Tracking

```cpp
sd.setProgressCallback([](int step, int steps, float time) {
    float progress = (float)step / (float)steps;
    // Update UI
});
```

Callbacks fire on each diffusion step for progress updates.
They run on the addon worker thread, so this example only copies lightweight
progress state there and updates `ofImage` / UI later from `update()` / `draw()`.

### Get Results

```cpp
if (sd.hasImageResult()) {
    auto images = sd.getImages();
    resultImage.setFromPixels(images[0].pixels);
}
```

Results are available after generation completes.

## Next Steps

- See [ofxGgmlStableDiffusionImageWorkflowExample](../ofxGgmlStableDiffusionImageWorkflowExample/) for image-to-image, inpainting, and ControlNet workflows
- See [API Reference](../docs/API_REFERENCE.md) for more generation options

## Troubleshooting

**Model won't load?**
- Verify the model path exists
- Check the model format (`.safetensors` recommended)
- See [Troubleshooting Guide](../../docs/TROUBLESHOOTING.md#model-loading-issues)

**Out of memory?**
- Reduce image dimensions (e.g., 512x512)
- Use `SD_TYPE_COUNT` to keep each model's stored weight type unless you need an explicit conversion
- See [Troubleshooting Guide](../../docs/TROUBLESHOOTING.md#memory-issues)

**Generation too slow?**
- Enable Flash Attention: `settings.flashAttn = true;`
- Reduce sample steps: `request.sampleSteps = 15;`
- See [Troubleshooting Guide](../../docs/TROUBLESHOOTING.md#performance-issues)
