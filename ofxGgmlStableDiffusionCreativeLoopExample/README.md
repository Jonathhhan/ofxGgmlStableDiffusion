# Creative Loop Example

Focused `ofxImGui` example for `ofxGgmlStableDiffusionRealtimeVideoSession`.

## Features

- Coalesce live prompt updates into preview frames
- Refine the latest prompt after it stays idle
- Feed the previous frame into the next preview for motion continuity
- Track prompt updates, coalesced updates, dropped updates, and preview/refine latency
- Clear feedback without stopping the loop

## Usage

1. Place an image-generation model in `bin/data/models/`.
2. Update the model path in `ofApp::configureContext()` if needed.
3. Run the example.
4. Click **Start Loop**.
5. Edit the prompt and click **Send Prompt**.

SPACE sends the current prompt and C clears the feedback frame.
