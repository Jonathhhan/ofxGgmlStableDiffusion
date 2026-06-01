# LoRA Embedding Example

Focused `ofxImGui` example for LoRA adapter stacks and textual-inversion embeddings.
Use this example when adapter/embedding discovery is the focus. For basic
prompt and sampler controls without adapter management, use the basic
generation example.

## Features

- Configure model, LoRA, and embedding directories
- Discover LoRA files from a directory
- Apply one selected LoRA or load all discovered LoRAs with a shared strength
- Reload and list textual-inversion embeddings
- Generate, cancel, preview, and save one image request with the active LoRA stack

## Usage

1. Place an image-generation model in `bin/data/models/`.
2. Place LoRA files in `bin/data/models/lora/`.
3. Place textual-inversion embeddings in `bin/data/embeddings/`.
4. Update paths in the panel if needed.
5. Click **Scan LoRAs**, apply adapters, and click **Generate**.

SPACE starts generation, and Esc or C requests cancellation.
