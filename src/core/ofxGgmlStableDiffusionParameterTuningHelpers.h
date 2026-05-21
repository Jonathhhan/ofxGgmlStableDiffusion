#pragma once

#include "ofxGgmlStableDiffusionCapabilityHelpers.h"
#include "ofxGgmlStableDiffusionRealtimeSession.h"
#include "ofxGgmlStableDiffusionRealtimeVideoSession.h"
#include "ofxGgmlStableDiffusionStringUtils.h"

#include <string>

struct ofxGgmlStableDiffusionImageParameterProfile {
	ofxGgmlStableDiffusionModelFamily modelFamily = ofxGgmlStableDiffusionModelFamily::Unknown;
	ofxGgmlStableDiffusionImageMode mode = ofxGgmlStableDiffusionImageMode::TextToImage;
	float defaultCfgScale = 7.0f;
	float minCfgScale = 1.0f;
	float maxCfgScale = 12.0f;
	int defaultSampleSteps = 24;
	int minSampleSteps = 4;
	int maxSampleSteps = 60;
	float defaultStrength = 0.5f;
	float minStrength = 0.05f;
	float maxStrength = 1.0f;
	int defaultClipSkip = -1;
	int minClipSkip = -1;
	int maxClipSkip = 12;
	bool supportsStrength = false;
	bool supportsClipSkip = true;
	const char* summary = "";
};

struct ofxGgmlStableDiffusionVideoParameterProfile {
	ofxGgmlStableDiffusionModelFamily modelFamily = ofxGgmlStableDiffusionModelFamily::Unknown;
	int defaultWidth = 512;
	int defaultHeight = 512;
	float defaultCfgScale = 5.0f;
	float minCfgScale = 1.0f;
	float maxCfgScale = 12.0f;
	int defaultSampleSteps = 28;
	int minSampleSteps = 1;
	int maxSampleSteps = 80;
	float defaultStrength = 0.7f;
	float minStrength = 0.2f;
	float maxStrength = 0.95f;
	int defaultClipSkip = -1;
	int minClipSkip = -1;
	int maxClipSkip = 12;
	float defaultVaceStrength = 1.0f;
	float minVaceStrength = 0.0f;
	float maxVaceStrength = 1.0f;
	int defaultFrameCount = 8;
	int minFrameCount = 4;
	int maxFrameCount = 121;
	int defaultFps = 8;
	int minFps = 4;
	int maxFps = 60;
	bool supportsClipSkip = false;
	bool supportsVaceStrength = false;
	const char* summary = "";
};

namespace ofxGgmlStableDiffusionParameterTuningHelpers {

inline bool isTurboLikeModel(const ofxGgmlStableDiffusionContextSettings& settings) {
	const std::string descriptor = ofxSdToLowerCopy(
		settings.modelPath + " " +
		settings.diffusionModelPath + " " +
		settings.clipLPath + " " +
		settings.clipGPath + " " +
		settings.t5xxlPath);
	return descriptor.find("turbo") != std::string::npos ||
		descriptor.find("lightning") != std::string::npos ||
		descriptor.find("hyper") != std::string::npos ||
		descriptor.find("lcm") != std::string::npos;
}

inline bool isWan13BT2VModel(const ofxGgmlStableDiffusionContextSettings& settings) {
	const std::string descriptor = ofxSdToLowerCopy(
		settings.modelPath + " " +
		settings.diffusionModelPath + " " +
		settings.clipLPath + " " +
		settings.clipGPath + " " +
		settings.t5xxlPath);
	return descriptor.find("wan") != std::string::npos &&
		descriptor.find("t2v") != std::string::npos &&
		descriptor.find("1.3b") != std::string::npos;
}

template <typename T>
inline T clampValue(T value, T minValue, T maxValue) {
	return std::max(minValue, std::min(value, maxValue));
}

inline ofxGgmlStableDiffusionImageParameterProfile resolveImageProfile(
	const ofxGgmlStableDiffusionContextSettings& settings,
	ofxGgmlStableDiffusionImageMode mode) {
	ofxGgmlStableDiffusionImageParameterProfile profile;
	profile.modelFamily = ofxGgmlStableDiffusionCapabilityHelpers::inferModelFamily(settings);
	profile.mode = mode;
	profile.supportsStrength = ofxGgmlStableDiffusionImageModeUsesInputImage(mode);
	profile.supportsClipSkip = !(
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::SD3 ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::FLUX ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::FLUXFill ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::FLUXControl ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::FLUX2 ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::WAN ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::WANI2V ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::WANTI2V ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::WANFLF2V ||
		profile.modelFamily == ofxGgmlStableDiffusionModelFamily::WANVACE);
	profile.defaultClipSkip = -1;
	profile.minClipSkip = -1;
	profile.maxClipSkip = profile.supportsClipSkip ? 12 : -1;
	profile.defaultStrength = ofxGgmlStableDiffusionDefaultStrengthForImageMode(mode);
	profile.summary = "Balanced defaults for classic Stable Diffusion checkpoints.";

	if (isTurboLikeModel(settings)) {
		profile.defaultCfgScale = 1.5f;
		profile.minCfgScale = 1.0f;
		profile.maxCfgScale = 3.0f;
		profile.defaultSampleSteps = 6;
		profile.minSampleSteps = 1;
		profile.maxSampleSteps = 12;
		profile.summary = "Turbo and lightning checkpoints prefer low CFG and very short schedules.";
		switch (mode) {
		case ofxGgmlStableDiffusionImageMode::Inpainting:
			profile.defaultCfgScale = 1.7f;
			profile.defaultStrength = 0.7f;
			break;
		case ofxGgmlStableDiffusionImageMode::ImageToImage:
			profile.defaultCfgScale = 1.5f;
			profile.defaultStrength = 0.5f;
			break;
		case ofxGgmlStableDiffusionImageMode::TextToImage:
		default:
			profile.defaultStrength = 0.5f;
			break;
		}
		return profile;
	}

	switch (profile.modelFamily) {
	case ofxGgmlStableDiffusionModelFamily::SDXL:
		profile.defaultCfgScale = 6.5f;
		profile.minCfgScale = 2.0f;
		profile.maxCfgScale = 10.0f;
		profile.defaultSampleSteps = 30;
		profile.minSampleSteps = 10;
		profile.maxSampleSteps = 60;
		profile.summary = "SDXL usually responds best to moderate CFG and a slightly longer schedule.";
		break;
	case ofxGgmlStableDiffusionModelFamily::SD3:
	case ofxGgmlStableDiffusionModelFamily::FLUX:
	case ofxGgmlStableDiffusionModelFamily::FLUXFill:
	case ofxGgmlStableDiffusionModelFamily::FLUXControl:
	case ofxGgmlStableDiffusionModelFamily::FLUX2:
		profile.defaultCfgScale = 3.5f;
		profile.minCfgScale = 1.0f;
		profile.maxCfgScale = 8.0f;
		profile.defaultSampleSteps = 28;
		profile.minSampleSteps = 10;
		profile.maxSampleSteps = 50;
		profile.summary = "Modern DiT-style models tend to want lower CFG than SD1.x / SDXL.";
		break;
	case ofxGgmlStableDiffusionModelFamily::SD2:
		profile.defaultCfgScale = 7.0f;
		profile.minCfgScale = 2.0f;
		profile.maxCfgScale = 12.0f;
		profile.defaultSampleSteps = 28;
		profile.minSampleSteps = 8;
		profile.maxSampleSteps = 60;
		profile.summary = "SD2.x likes classic CFG ranges with enough steps to stabilize detail.";
		break;
	case ofxGgmlStableDiffusionModelFamily::Unknown:
	case ofxGgmlStableDiffusionModelFamily::SD1:
	default:
		profile.defaultCfgScale = 7.0f;
		profile.minCfgScale = 2.0f;
		profile.maxCfgScale = 14.0f;
		profile.defaultSampleSteps = 26;
		profile.minSampleSteps = 8;
		profile.maxSampleSteps = 60;
		break;
	}

	switch (mode) {
	case ofxGgmlStableDiffusionImageMode::Inpainting:
		profile.defaultCfgScale = clampValue(profile.defaultCfgScale + 0.5f, profile.minCfgScale, profile.maxCfgScale);
		profile.defaultSampleSteps = std::min(profile.maxSampleSteps, profile.defaultSampleSteps + 2);
		profile.defaultStrength = 0.75f;
		profile.minStrength = 0.15f;
		break;
	case ofxGgmlStableDiffusionImageMode::ImageToImage:
		profile.defaultStrength = 0.5f;
		break;
	case ofxGgmlStableDiffusionImageMode::TextToImage:
	default:
		profile.supportsStrength = false;
		break;
	}

	return profile;
}

inline ofxGgmlStableDiffusionVideoParameterProfile resolveVideoProfile(
	const ofxGgmlStableDiffusionContextSettings& settings) {
	ofxGgmlStableDiffusionVideoParameterProfile profile;
	profile.modelFamily = ofxGgmlStableDiffusionCapabilityHelpers::inferModelFamily(settings);
	profile.summary = "Balanced defaults for image-to-video generation.";

	switch (profile.modelFamily) {
	case ofxGgmlStableDiffusionModelFamily::WAN:
		profile.defaultCfgScale = 6.0f;
		profile.defaultSampleSteps = 40;
		profile.defaultStrength = 0.7f;
		profile.defaultFrameCount = 81;
		profile.defaultFps = 16;
		profile.maxCfgScale = 12.0f;
		profile.maxSampleSteps = 80;
		profile.maxFrameCount = 121;
		profile.maxFps = 60;
		profile.summary = "Wan T2V benefits from more steps and longer clips; wider ranges stay open for tuning.";
		break;
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V:
		profile.defaultCfgScale = 5.0f;
		profile.defaultSampleSteps = 28;
		profile.defaultStrength = 0.65f;
		profile.defaultFrameCount = 10;
		profile.defaultFps = 8;
		profile.maxSampleSteps = 80;
		profile.maxFrameCount = 121;
		profile.maxFps = 60;
		profile.summary = "FLF2V models respond well to moderate denoise and support end-frame morphing.";
		break;
	case ofxGgmlStableDiffusionModelFamily::WANTI2V:
		profile.defaultCfgScale = 5.5f;
		profile.defaultSampleSteps = 30;
		profile.defaultStrength = 0.72f;
		profile.defaultFrameCount = 8;
		profile.defaultFps = 8;
		profile.maxSampleSteps = 80;
		profile.maxFrameCount = 121;
		profile.maxFps = 60;
		profile.summary = "TI2V models prefer a slightly firmer CFG and do not use end-frame morphing.";
		break;
	case ofxGgmlStableDiffusionModelFamily::WANVACE:
		profile.defaultCfgScale = 4.5f;
		profile.defaultSampleSteps = 24;
		profile.defaultStrength = 0.8f;
		profile.supportsVaceStrength = true;
		profile.defaultVaceStrength = 1.0f;
		profile.defaultFrameCount = 8;
		profile.defaultFps = 8;
		profile.maxSampleSteps = 80;
		profile.maxFrameCount = 121;
		profile.maxFps = 60;
		profile.summary = "VACE models expose an extra conditioning weight; start high and back it off only if motion feels too constrained.";
		break;
	case ofxGgmlStableDiffusionModelFamily::WANI2V:
		profile.defaultCfgScale = 5.0f;
		profile.defaultSampleSteps = 28;
		profile.defaultStrength = 0.7f;
		profile.defaultFrameCount = 8;
		profile.defaultFps = 8;
		profile.maxSampleSteps = 80;
		profile.maxFrameCount = 121;
		profile.maxFps = 60;
		profile.summary = "Wan I2V models prefer moderate CFG and enough denoise strength to preserve motion.";
		break;
	case ofxGgmlStableDiffusionModelFamily::Unknown:
	default:
		profile.defaultCfgScale = 6.0f;
		profile.defaultSampleSteps = 24;
		profile.defaultStrength = 0.65f;
		profile.defaultFrameCount = 6;
		profile.defaultFps = 6;
		profile.supportsClipSkip = true;
		profile.summary = "Fallback video defaults; tune conservatively until you know the model family.";
		break;
	}

	if (isWan13BT2VModel(settings)) {
		profile.defaultWidth = 832;
		profile.defaultHeight = 480;
		profile.defaultSampleSteps = 20;
		profile.defaultStrength = 0.75f;
		profile.defaultFrameCount = 33;
		profile.defaultFps = 16;
		profile.summary = "Wan 1.3B T2V matches the upstream 480p recipe best at 832x480 with 33 frames, 20 steps, and 0.75 strength.";
	}

	return profile;
}

inline void clampImageParametersToProfile(
	const ofxGgmlStableDiffusionImageParameterProfile& profile,
	float& cfgScale,
	int& sampleSteps,
	float& strength,
	int& clipSkip) {
	cfgScale = clampValue(cfgScale, profile.minCfgScale, profile.maxCfgScale);
	sampleSteps = clampValue(sampleSteps, profile.minSampleSteps, profile.maxSampleSteps);
	if (profile.supportsStrength) {
		strength = clampValue(strength, profile.minStrength, profile.maxStrength);
	}
	if (profile.supportsClipSkip) {
		clipSkip = clampValue(clipSkip, profile.minClipSkip, profile.maxClipSkip);
	} else {
		clipSkip = -1;
	}
}

inline ofxGgmlStableDiffusionImageMode resolveSupportedImageMode(
	ofxGgmlStableDiffusionImageMode requestedMode,
	const ofxGgmlStableDiffusionCapabilities& capabilities) {
	if (capabilities.supportsImageMode(requestedMode)) {
		return requestedMode;
	}
	if (capabilities.textToImage) {
		return ofxGgmlStableDiffusionImageMode::TextToImage;
	}
	if (capabilities.imageToImage) {
		return ofxGgmlStableDiffusionImageMode::ImageToImage;
	}
	if (capabilities.inpainting) {
		return ofxGgmlStableDiffusionImageMode::Inpainting;
	}
	return requestedMode;
}

inline void applyImageProfileDefaults(
	const ofxGgmlStableDiffusionImageParameterProfile& profile,
	ofxGgmlStableDiffusionImageRequest& request,
	bool overwriteExplicitValues = false) {
	if (overwriteExplicitValues || !std::isfinite(request.cfgScale)) {
		request.cfgScale = profile.defaultCfgScale;
	}
	if (overwriteExplicitValues || request.sampleSteps <= 0) {
		request.sampleSteps = profile.defaultSampleSteps;
	}
	if (profile.supportsStrength) {
		if (overwriteExplicitValues || !std::isfinite(request.strength)) {
			request.strength = profile.defaultStrength;
		}
	} else {
		request.strength = std::numeric_limits<float>::infinity();
	}
	if (profile.supportsClipSkip) {
		if (overwriteExplicitValues || request.clipSkip < profile.minClipSkip) {
			request.clipSkip = profile.defaultClipSkip;
		}
	} else {
		request.clipSkip = -1;
	}

	clampImageParametersToProfile(
		profile,
		request.cfgScale,
		request.sampleSteps,
		request.strength,
		request.clipSkip);
}

inline void applyRecommendedImageRequest(
	const ofxGgmlStableDiffusionContextSettings& settings,
	ofxGgmlStableDiffusionImageRequest& request,
	const ofxGgmlStableDiffusionCapabilities* capabilities = nullptr,
	bool overwriteExplicitValues = false) {
	if (capabilities != nullptr && capabilities->contextConfigured) {
		request.mode = resolveSupportedImageMode(request.mode, *capabilities);
	}
	const auto profile = resolveImageProfile(settings, request.mode);
	applyImageProfileDefaults(profile, request, overwriteExplicitValues);
}

inline void clampVideoParametersToProfile(
	const ofxGgmlStableDiffusionVideoParameterProfile& profile,
	float& cfgScale,
	int& sampleSteps,
	float& strength,
	int& clipSkip,
	float& vaceStrength,
	int& frameCount,
	int& fps) {
	cfgScale = clampValue(cfgScale, profile.minCfgScale, profile.maxCfgScale);
	sampleSteps = clampValue(sampleSteps, profile.minSampleSteps, profile.maxSampleSteps);
	strength = clampValue(strength, profile.minStrength, profile.maxStrength);
	frameCount = clampValue(frameCount, profile.minFrameCount, profile.maxFrameCount);
	fps = clampValue(fps, profile.minFps, profile.maxFps);
	if (profile.supportsClipSkip) {
		clipSkip = clampValue(clipSkip, profile.minClipSkip, profile.maxClipSkip);
	} else {
		clipSkip = -1;
	}
	if (profile.supportsVaceStrength) {
		vaceStrength = clampValue(vaceStrength, profile.minVaceStrength, profile.maxVaceStrength);
	} else {
		vaceStrength = profile.defaultVaceStrength;
	}
}

inline void applyVideoProfileDefaults(
	const ofxGgmlStableDiffusionVideoParameterProfile& profile,
	ofxGgmlStableDiffusionVideoRequest& request,
	bool overwriteExplicitValues = false) {
	if (overwriteExplicitValues || request.width <= 0) {
		request.width = profile.defaultWidth;
	}
	if (overwriteExplicitValues || request.height <= 0) {
		request.height = profile.defaultHeight;
	}
	if (overwriteExplicitValues || !std::isfinite(request.cfgScale)) {
		request.cfgScale = profile.defaultCfgScale;
	}
	if (overwriteExplicitValues || request.sampleSteps <= 0) {
		request.sampleSteps = profile.defaultSampleSteps;
	}
	if (overwriteExplicitValues || !std::isfinite(request.strength)) {
		request.strength = profile.defaultStrength;
	}
	if (overwriteExplicitValues || request.frameCount <= 0) {
		request.frameCount = profile.defaultFrameCount;
	}
	if (overwriteExplicitValues || request.fps <= 0) {
		request.fps = profile.defaultFps;
	}
	if (profile.supportsClipSkip) {
		if (overwriteExplicitValues || request.clipSkip < profile.minClipSkip) {
			request.clipSkip = profile.defaultClipSkip;
		}
	} else {
		request.clipSkip = -1;
	}
	if (profile.supportsVaceStrength) {
		if (overwriteExplicitValues || !std::isfinite(request.vaceStrength)) {
			request.vaceStrength = profile.defaultVaceStrength;
		}
	} else {
		request.vaceStrength = std::numeric_limits<float>::infinity();
	}

	clampVideoParametersToProfile(
		profile,
		request.cfgScale,
		request.sampleSteps,
		request.strength,
		request.clipSkip,
		request.vaceStrength,
		request.frameCount,
		request.fps);
}

inline void applyRecommendedVideoRequest(
	const ofxGgmlStableDiffusionContextSettings& settings,
	ofxGgmlStableDiffusionVideoRequest& request,
	const ofxGgmlStableDiffusionCapabilities* capabilities = nullptr,
	bool overwriteExplicitValues = false) {
	if (capabilities != nullptr && capabilities->contextConfigured && !capabilities->imageToVideo) {
		request.frameCount = 0;
		return;
	}
	const auto profile = resolveVideoProfile(settings);
	applyVideoProfileDefaults(profile, request, overwriteExplicitValues);
}

inline ofxGgmlStableDiffusionRealtimeSettings resolveRecommendedRealtimeSettings(
	const ofxGgmlStableDiffusionContextSettings& settings) {
	ofxGgmlStableDiffusionRealtimeSettings realtime;
	const auto imageProfile = resolveImageProfile(
		settings,
		ofxGgmlStableDiffusionImageMode::TextToImage);
	realtime.cfgScale = imageProfile.defaultCfgScale;
	realtime.minSampleSteps = std::max(1, imageProfile.minSampleSteps);
	realtime.maxSampleSteps = std::max(
		realtime.minSampleSteps,
		std::min(imageProfile.maxSampleSteps, imageProfile.defaultSampleSteps + 4));
	realtime.enableProgressiveRefinement =
		realtime.maxSampleSteps > realtime.minSampleSteps;
	realtime.targetLatencyMs = isTurboLikeModel(settings) ? 350 : 650;
	realtime.mode = isTurboLikeModel(settings)
		? ofxGgmlStableDiffusionRealtimeMode::LowLatency
		: ofxGgmlStableDiffusionRealtimeMode::Streaming;
	return realtime;
}

inline ofxGgmlStableDiffusionRealtimeVideoSettings resolveRecommendedRealtimeVideoSettings(
	const ofxGgmlStableDiffusionContextSettings& settings) {
	ofxGgmlStableDiffusionRealtimeVideoSettings realtime;
	const auto videoProfile = resolveVideoProfile(settings);
	realtime.previewWidth = videoProfile.defaultWidth;
	realtime.previewHeight = videoProfile.defaultHeight;
	realtime.cfgScale = videoProfile.defaultCfgScale;
	realtime.previewSteps = std::max(1, videoProfile.minSampleSteps);
	realtime.refineSteps = std::max(
		realtime.previewSteps,
		std::min(videoProfile.maxSampleSteps, videoProfile.defaultSampleSteps));
	realtime.previewStrength = videoProfile.defaultStrength;
	realtime.refineStrength = std::max(
		videoProfile.minStrength,
		std::min(videoProfile.maxStrength, videoProfile.defaultStrength * 0.8f));
	realtime.dropIfBusy = isTurboLikeModel(settings);
	realtime.refineAfterStableMs = isTurboLikeModel(settings) ? 450 : 700;
	return realtime;
}

} // namespace ofxGgmlStableDiffusionParameterTuningHelpers
