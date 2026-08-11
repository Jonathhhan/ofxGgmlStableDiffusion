#include "video/ofxGgmlStableDiffusionVideoHelpers.h"
#include "core/ofxGgmlStableDiffusionNativeAdapter.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const std::string & message) {
	if (condition) {
		return true;
	}

	std::cerr << "FAIL: " << message << std::endl;
	return false;
}

template <typename T>
std::string joinVector(const std::vector<T> & values) {
	std::ostringstream stream;
	for (std::size_t i = 0; i < values.size(); ++i) {
		if (i > 0) {
			stream << ", ";
		}
		stream << values[i];
	}
	return stream.str();
}

bool expectSequence(
	int sourceFrameCount,
	ofxGgmlStableDiffusionVideoMode mode,
	const std::vector<int> & expected,
	const std::string & label) {
	const std::vector<int> actual = ofxGgmlStableDiffusionBuildVideoFrameSequence(sourceFrameCount, mode);
	if (actual == expected) {
		return true;
	}

	std::cerr << "FAIL: " << label
		<< " expected [" << joinVector(expected)
		<< "] but got [" << joinVector(actual) << "]" << std::endl;
	return false;
}

bool expectNear(float actual, float expected, float epsilon, const std::string & label) {
	if (std::fabs(actual - expected) <= epsilon) {
		return true;
	}

	std::cerr << "FAIL: " << label << " expected " << expected << " but got " << actual << std::endl;
	return false;
}

bool expectEqual(const std::string & actual, const std::string & expected, const std::string & label) {
	if (actual == expected) {
		return true;
	}

	std::cerr << "FAIL: " << label << " expected \"" << expected << "\" but got \"" << actual << "\"" << std::endl;
	return false;
}

bool expectEqual(int64_t actual, int64_t expected, const std::string & label) {
	if (actual == expected) {
		return true;
	}

	std::cerr << "FAIL: " << label << " expected " << expected << " but got " << actual << std::endl;
	return false;
}

} // namespace

int main() {
	bool ok = true;

	// Test basic sequences (from original test)
	ok &= expectSequence(0, ofxGgmlStableDiffusionVideoMode::Standard, {}, "empty standard");
	ok &= expectSequence(1, ofxGgmlStableDiffusionVideoMode::Boomerang, {0}, "single-frame boomerang");
	ok &= expectSequence(4, ofxGgmlStableDiffusionVideoMode::Standard, {0, 1, 2, 3}, "standard sequence");
	ok &= expectSequence(4, ofxGgmlStableDiffusionVideoMode::Loop, {0, 1, 2, 3, 0}, "loop sequence");
	ok &= expectSequence(4, ofxGgmlStableDiffusionVideoMode::PingPong, {0, 1, 2, 3, 2, 1}, "ping-pong sequence");
	ok &= expectSequence(4, ofxGgmlStableDiffusionVideoMode::Boomerang, {0, 1, 2, 3, 3, 2, 1, 0}, "boomerang sequence");

	// Test edge cases - negative frame count
	ok &= expectSequence(-1, ofxGgmlStableDiffusionVideoMode::Standard, {}, "negative frame count standard");
	ok &= expectSequence(-5, ofxGgmlStableDiffusionVideoMode::Loop, {}, "negative frame count loop");

	// Test edge cases - single frame for all modes
	ok &= expectSequence(1, ofxGgmlStableDiffusionVideoMode::Standard, {0}, "single-frame standard");
	ok &= expectSequence(1, ofxGgmlStableDiffusionVideoMode::Loop, {0}, "single-frame loop");
	ok &= expectSequence(1, ofxGgmlStableDiffusionVideoMode::PingPong, {0}, "single-frame ping-pong");

	// Test edge cases - two frames
	ok &= expectSequence(2, ofxGgmlStableDiffusionVideoMode::Standard, {0, 1}, "two-frame standard");
	ok &= expectSequence(2, ofxGgmlStableDiffusionVideoMode::Loop, {0, 1, 0}, "two-frame loop");
	ok &= expectSequence(2, ofxGgmlStableDiffusionVideoMode::PingPong, {0, 1}, "two-frame ping-pong (no middle)");
	ok &= expectSequence(2, ofxGgmlStableDiffusionVideoMode::Boomerang, {0, 1, 1, 0}, "two-frame boomerang");

	// Test larger sequences
	ok &= expectSequence(6, ofxGgmlStableDiffusionVideoMode::Standard, {0, 1, 2, 3, 4, 5}, "6-frame standard");
	ok &= expectSequence(6, ofxGgmlStableDiffusionVideoMode::Loop, {0, 1, 2, 3, 4, 5, 0}, "6-frame loop");
	ok &= expectSequence(6, ofxGgmlStableDiffusionVideoMode::PingPong, {0, 1, 2, 3, 4, 5, 4, 3, 2, 1}, "6-frame ping-pong");
	ok &= expectSequence(6, ofxGgmlStableDiffusionVideoMode::Boomerang, {0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 0}, "6-frame boomerang");

	// Test duration calculations
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(0, 6), 0.0f, 0.0001f, "empty duration");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(6, 6), 1.0f, 0.0001f, "whole-second duration");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(5, 10), 0.5f, 0.0001f, "fractional duration");

	// Native August API timing mapping: FPS must reach sd_vid_gen_params_t,
	// not only the preview/export layer.
	ofxGgmlStableDiffusionVideoRequest nativeTimingRequest;
	nativeTimingRequest.frameCount = 33;
	nativeTimingRequest.fps = 16;
	sd_vid_gen_params_t nativeTimingParams{};
	ofxGgmlStableDiffusionNativeAdapter::applyVideoTiming(nativeTimingRequest, nativeTimingParams);
	ok &= expect(nativeTimingParams.video_frames == 33, "native video frame count is mapped");
	ok &= expect(nativeTimingParams.fps == 16, "native video fps is mapped");

	// Current stable-diffusion.cpp runtime placement and VRAM controls must
	// reach sd_ctx_params_t without requiring a model-backed test.
	ofxGgmlStableDiffusionContextSettings runtimeSettings;
	runtimeSettings.maxVram = "cuda0=6,cuda1=8";
	runtimeSettings.streamLayers = true;
	runtimeSettings.eagerLoad = true;
	runtimeSettings.backend = "diffusion=cuda0&cuda1,vae=cuda0";
	runtimeSettings.paramsBackend = "cpu";
	runtimeSettings.splitMode = "diffusion=row,te=layer";
	runtimeSettings.autoFit = true;
	sd_ctx_params_t nativeContextParams{};
	ofxGgmlStableDiffusionNativeAdapter::applyContextRuntimeOptions(
		runtimeSettings,
		nativeContextParams);
	ok &= expect(
		nativeContextParams.max_vram != nullptr &&
			std::string(nativeContextParams.max_vram) == runtimeSettings.maxVram,
		"native max VRAM specification is mapped");
	ok &= expect(nativeContextParams.stream_layers, "native layer streaming is mapped");
	ok &= expect(nativeContextParams.eager_load, "native eager loading is mapped");
	ok &= expect(
		nativeContextParams.backend != nullptr &&
			std::string(nativeContextParams.backend) == runtimeSettings.backend,
		"native backend assignment is mapped");
	ok &= expect(
		nativeContextParams.params_backend != nullptr &&
			std::string(nativeContextParams.params_backend) == runtimeSettings.paramsBackend,
		"native params backend assignment is mapped");
	ok &= expect(
		nativeContextParams.split_mode != nullptr &&
			std::string(nativeContextParams.split_mode) == runtimeSettings.splitMode,
		"native multi-device split mode is mapped");
	ok &= expect(nativeContextParams.auto_fit, "native device auto-fit is mapped");

	ofxGgmlStableDiffusionContextSettings defaultRuntimeSettings;
	sd_ctx_params_t defaultNativeContextParams{};
	ofxGgmlStableDiffusionNativeAdapter::applyContextRuntimeOptions(
		defaultRuntimeSettings,
		defaultNativeContextParams);
	ok &= expect(defaultNativeContextParams.max_vram == nullptr, "default max VRAM remains disabled");
	ok &= expect(defaultNativeContextParams.split_mode == nullptr, "default split mode remains native default");
	ok &= expect(!defaultNativeContextParams.stream_layers, "default layer streaming remains disabled");
	ok &= expect(!defaultNativeContextParams.eager_load, "default eager loading remains disabled");
	ok &= expect(!defaultNativeContextParams.auto_fit, "default auto-fit remains disabled");

	ofxGgmlStableDiffusionContextSettings changedRuntimeSettings = runtimeSettings;
	ok &= expect(changedRuntimeSettings == runtimeSettings, "runtime settings copy compares equal");
	changedRuntimeSettings.maxVram = "4";
	ok &= expect(changedRuntimeSettings != runtimeSettings, "max VRAM participates in context equality");
	changedRuntimeSettings = runtimeSettings;
	changedRuntimeSettings.streamLayers = false;
	ok &= expect(changedRuntimeSettings != runtimeSettings, "layer streaming participates in context equality");
	changedRuntimeSettings = runtimeSettings;
	changedRuntimeSettings.eagerLoad = false;
	ok &= expect(changedRuntimeSettings != runtimeSettings, "eager loading participates in context equality");
	changedRuntimeSettings = runtimeSettings;
	changedRuntimeSettings.splitMode = "layer";
	ok &= expect(changedRuntimeSettings != runtimeSettings, "split mode participates in context equality");
	changedRuntimeSettings = runtimeSettings;
	changedRuntimeSettings.autoFit = false;
	ok &= expect(changedRuntimeSettings != runtimeSettings, "auto-fit participates in context equality");

	// Test duration edge cases
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(0, 0), 0.0f, 0.0001f, "zero frames zero fps");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(10, 0), 0.0f, 0.0001f, "zero fps");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(10, -5), 0.0f, 0.0001f, "negative fps");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(1, 1), 1.0f, 0.0001f, "1 frame at 1 fps");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(30, 30), 1.0f, 0.0001f, "30 frames at 30 fps");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(60, 30), 2.0f, 0.0001f, "60 frames at 30 fps");
	ok &= expectNear(ofxGgmlStableDiffusionVideoDurationSeconds(1, 60), 1.0f/60.0f, 0.0001f, "1 frame at 60 fps");

	// Test frame index lookup
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(0, 6, 0.1f) == -1, "empty frame lookup");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(4, 0, 1.0f) == 0, "zero-fps lookup clamps to first frame");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(4, 4, -1.0f) == 0, "negative time clamps to first frame");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(4, 4, 0.49f) == 1, "fractional frame lookup");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(4, 4, 99.0f) == 3, "late time clamps to last frame");

	// Test frame index lookup edge cases
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(0, 0, 0.0f) == -1, "zero frames zero fps");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, -1, 1.0f) == 0, "negative fps clamps to first");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(1, 30, 0.0f) == 0, "single frame at time 0");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(1, 30, 100.0f) == 0, "single frame at large time");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, 30, 0.0f) == 0, "first frame at time 0");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, 30, 0.03f) == 0, "frame at boundary");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, 30, 0.034f) == 1, "frame just after boundary");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, 10, 0.5f) == 5, "frame at middle");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, 10, 0.9f) == 9, "frame near end");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, 10, 0.99f) == 9, "frame very near end");
	ok &= expect(ofxGgmlStableDiffusionVideoFrameIndexForTime(10, 10, 1.0f) == 9, "frame at exact duration");

	// Test mode labels
	ok &= expect(std::string(ofxGgmlStableDiffusionVideoModeName(ofxGgmlStableDiffusionVideoMode::Standard)) == "Standard", "standard label");
	ok &= expect(std::string(ofxGgmlStableDiffusionVideoModeName(ofxGgmlStableDiffusionVideoMode::Loop)) == "Loop", "loop label");
	ok &= expect(std::string(ofxGgmlStableDiffusionVideoModeName(ofxGgmlStableDiffusionVideoMode::PingPong)) == "PingPong", "ping-pong label");
	ok &= expect(std::string(ofxGgmlStableDiffusionVideoModeName(ofxGgmlStableDiffusionVideoMode::Boomerang)) == "Boomerang", "boomerang label");

	// Test prompt interpolation helpers
	const ofxGgmlStableDiffusionVideoRequest promptRequest = ofxGgmlStableDiffusionCreatePromptInterpolationRequest(
		{
			{0, "sunrise city"},
			{9, "midnight city"}
		},
		10,
		576,
		1024,
		ofxGgmlStableDiffusionInterpolationMode::Linear);
	ok &= expect(promptRequest.hasAnimation(), "prompt interpolation request enables animation");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFramePrompt(promptRequest, 0), "sunrise city", "prompt interpolation start");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFramePrompt(promptRequest, 9), "midnight city", "prompt interpolation end");
	const std::string blendedPrompt = ofxGgmlStableDiffusionGetFramePrompt(promptRequest, 4);
	ok &= expect(blendedPrompt.find("sunrise city") != std::string::npos, "blended prompt includes first keyframe");
	ok &= expect(blendedPrompt.find("midnight city") != std::string::npos, "blended prompt includes second keyframe");
	ok &= expect(blendedPrompt.find("AND") != std::string::npos, "blended prompt uses weighted AND syntax");

	// Test parameter animation helpers
	ofxGgmlStableDiffusionVideoRequest parameterRequest = ofxGgmlStableDiffusionCreateParameterAnimationRequest(
		{
			[] {
				ofxGgmlStableDiffusionKeyframe keyframe(0);
				keyframe.cfgScale = 3.0f;
				keyframe.strength = 0.2f;
				keyframe.prompt = "frame zero";
				keyframe.negativePrompt = "none";
				return keyframe;
			}(),
			[] {
				ofxGgmlStableDiffusionKeyframe keyframe(10);
				keyframe.cfgScale = 9.0f;
				keyframe.strength = 0.8f;
				keyframe.prompt = "frame ten";
				keyframe.negativePrompt = "busy";
				keyframe.seed = 555;
				return keyframe;
			}()
		},
		11,
		576,
		1024,
		ofxGgmlStableDiffusionInterpolationMode::Linear);
	parameterRequest.prompt = "fallback prompt";
	parameterRequest.negativePrompt = "fallback negative";
	parameterRequest.seed = 123;
	ok &= expect(parameterRequest.hasAnimation(), "parameter animation request enables animation");
	ok &= expectNear(ofxGgmlStableDiffusionGetFrameCfgScale(parameterRequest, 5), 6.0f, 0.0001f, "cfg interpolation midpoint");
	ok &= expectNear(ofxGgmlStableDiffusionGetFrameStrength(parameterRequest, 5), 0.5f, 0.0001f, "strength interpolation midpoint");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFramePrompt(parameterRequest, 5), "frame zero", "keyframed prompt holds previous value");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFrameNegativePrompt(parameterRequest, 10), "busy", "keyframed negative prompt exact match");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFrameSeed(parameterRequest, 10), static_cast<int64_t>(555), "keyframed seed exact match");

	// Test seed sequence helpers and precedence
	ofxGgmlStableDiffusionVideoRequest seedSequenceRequest =
		ofxGgmlStableDiffusionCreateSeedSequenceRequest(100, 4, 3);
	ok &= expect(seedSequenceRequest.hasAnimation(), "seed sequence request enables animation");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFrameSeed(seedSequenceRequest, 0), static_cast<int64_t>(100), "seed sequence first frame");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFrameSeed(seedSequenceRequest, 3), static_cast<int64_t>(109), "seed sequence last frame");

	seedSequenceRequest.animationSettings.enableParameterAnimation = true;
	seedSequenceRequest.animationSettings.parameterKeyframes = {
		[] {
			ofxGgmlStableDiffusionKeyframe keyframe(2);
			keyframe.seed = 999;
			return keyframe;
		}()
	};
	ok &= expectEqual(ofxGgmlStableDiffusionGetFrameSeed(seedSequenceRequest, 2), static_cast<int64_t>(999), "keyframed seed overrides sequence");
	ok &= expectEqual(ofxGgmlStableDiffusionGetFrameSeed(seedSequenceRequest, 3), static_cast<int64_t>(999), "keyframed seed persists after keyframe");

	ofxGgmlStableDiffusionVideoRequest overflowSeedRequest =
		ofxGgmlStableDiffusionCreateSeedSequenceRequest(
			std::numeric_limits<int64_t>::max() - 2,
			4,
			2);
	ok &= expectEqual(
		ofxGgmlStableDiffusionGetFrameSeed(overflowSeedRequest, 3),
		std::numeric_limits<int64_t>::max(),
		"seed sequence clamps on positive overflow");

	ofxGgmlStableDiffusionVideoRequest underflowSeedRequest =
		ofxGgmlStableDiffusionCreateSeedSequenceRequest(
			5,
			4,
			std::numeric_limits<int64_t>::min() / 2);
	ok &= expectEqual(
		ofxGgmlStableDiffusionGetFrameSeed(underflowSeedRequest, 3),
		std::numeric_limits<int64_t>::min(),
		"seed sequence clamps on negative overflow");

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
