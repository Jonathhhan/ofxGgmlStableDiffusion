# Staging Plan

`ofxGgmlStableDiffusion` should remain based on `ofxStableDiffusion` while it is
being adopted into the ofxGgml addon family.

## Decisions

- Use the existing `ofxStableDiffusion` addon as the base.
- Keep `stable-diffusion.cpp` as the native backend because that is what
  `ofxStableDiffusion` already uses.
- Keep native ggml artifacts owned by this addon by default.
- Exclude `ofxGgmlDiffusion` from this addon. Its prompt/GAN examples are not a
  source for this lane.
- Avoid GGUF GAN work unless a real upstream ecosystem appears and the addon
  owner explicitly asks for that direction.

## Near-Term Work

- Keep README, examples, and scripts aligned with the ofxStableDiffusion user
  experience.
- Add local validation and ecosystem metadata without changing runtime behavior.
- Promote to wider ecosystem automation only after local validation is stable.

## Compatibility Notes

The name changes, but the mental model should not: users should recognize the
same `ofxStableDiffusion` addon flow, API style, scripts, docs, example shape,
and model-file expectations.
