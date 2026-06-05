#include "core/ofxGgmlStableDiffusionCreativeWorkflow.h"
#include "core/ofxGgmlStableDiffusionParameterTuningHelpers.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
	if (condition) {
		return true;
	}

	std::cerr << "FAIL: " << message << std::endl;
	return false;
}

bool expectNear(float actual, float expected, float epsilon, const std::string& label) {
	if (std::fabs(actual - expected) <= epsilon) {
		return true;
	}

	std::cerr << "FAIL: " << label << " expected " << expected << " but got " << actual << std::endl;
	return false;
}

} // namespace

int main() {
	bool ok = true;

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/sd_turbo.safetensors";
		const auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveImageProfile(
			settings,
			ofxGgmlStableDiffusionImageMode::TextToImage);
		ok &= expect(profile.defaultSampleSteps <= 12, "turbo keeps a short schedule");
		ok &= expect(profile.maxCfgScale <= 3.0f, "turbo keeps cfg range tight");
		ok &= expect(profile.summary != nullptr && profile.summary[0] != '\0', "turbo summary exists");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/juggernautXL.safetensors";
		const auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveImageProfile(
			settings,
			ofxGgmlStableDiffusionImageMode::Inpainting);
		ok &= expect(profile.modelFamily == ofxGgmlStableDiffusionModelFamily::SDXL, "sdxl profile detects family");
		ok &= expect(profile.defaultSampleSteps >= 30, "sdxl inpainting prefers longer schedules");
		ok &= expect(profile.defaultCfgScale <= profile.maxCfgScale, "sdxl cfg stays in range");
		ok &= expect(profile.supportsClipSkip, "sdxl still exposes clip skip");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.diffusionModelPath = "models/flux/flux1-dev-Q8_0.gguf";
		settings.clipLPath = "models/flux/clip_l.safetensors";
		settings.t5xxlPath = "models/flux/t5xxl.safetensors";
		const auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveImageProfile(
			settings,
			ofxGgmlStableDiffusionImageMode::ImageToImage);
		ok &= expect(profile.modelFamily == ofxGgmlStableDiffusionModelFamily::FLUX, "flux profile detects split family");
		ok &= expect(profile.defaultCfgScale <= 8.0f, "flux img2img keeps cfg moderate");
		ok &= expect(!profile.supportsClipSkip, "flux disables clip skip tuning");
		ok &= expect(profile.maxCfgScale <= 8.0f, "flux keeps cfg range moderate");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/video/Wan2.1-VACE-1.3B.gguf";
		const auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveVideoProfile(settings);
		ok &= expect(profile.modelFamily == ofxGgmlStableDiffusionModelFamily::WANVACE, "vace profile detects family");
		ok &= expect(profile.supportsVaceStrength, "vace profile exposes vace strength");
		ok &= expect(profile.defaultVaceStrength > 0.9f, "vace profile starts with strong guidance");
		ok &= expect(!profile.supportsClipSkip, "wan video profile disables clip skip");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/video/Wan2.1-FLF2V-14B.gguf";
		auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveVideoProfile(settings);
		float cfgScale = 99.0f;
		int sampleSteps = 1;
		float strength = 0.01f;
		int clipSkip = 9;
		float vaceStrength = 0.25f;
		int frameCount = 200;
		int fps = 60;

		ofxGgmlStableDiffusionParameterTuningHelpers::clampVideoParametersToProfile(
			profile,
			cfgScale,
			sampleSteps,
			strength,
			clipSkip,
			vaceStrength,
			frameCount,
			fps);

		ok &= expect(cfgScale <= profile.maxCfgScale, "video cfg is clamped");
		ok &= expect(sampleSteps >= profile.minSampleSteps, "video steps are clamped");
		ok &= expect(strength >= profile.minStrength, "video strength is clamped");
		ok &= expect(clipSkip == -1, "unsupported clip skip resets to auto");
		ok &= expect(frameCount == profile.maxFrameCount, "video frame count is clamped");
		ok &= expect(fps == profile.maxFps, "video fps is clamped");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/sdxl/juggernautXL.safetensors";
		auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveImageProfile(
			settings,
			ofxGgmlStableDiffusionImageMode::Inpainting);
		float cfgScale = 50.0f;
		int sampleSteps = 2;
		float strength = 0.0f;
		int clipSkip = 99;

		ofxGgmlStableDiffusionParameterTuningHelpers::clampImageParametersToProfile(
			profile,
			cfgScale,
			sampleSteps,
			strength,
			clipSkip);

		ok &= expect(cfgScale == profile.maxCfgScale, "image cfg is clamped");
		ok &= expect(sampleSteps == profile.minSampleSteps, "image steps are clamped");
		ok &= expect(strength == profile.minStrength, "image strength is clamped");
		ok &= expect(clipSkip == profile.maxClipSkip, "image clip skip is clamped");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/sd_turbo.safetensors";
		ofxGgmlStableDiffusionCapabilities capabilities;
		capabilities.contextConfigured = true;
		capabilities.textToImage = true;

		ofxGgmlStableDiffusionImageRequest request;
		request.mode = ofxGgmlStableDiffusionImageMode::Inpainting;
		ofxGgmlStableDiffusionParameterTuningHelpers::applyRecommendedImageRequest(
			settings,
			request,
			&capabilities);

		ok &= expect(
			request.mode == ofxGgmlStableDiffusionImageMode::TextToImage,
			"unsupported image modes fall back to a supported mode");
		ok &= expect(std::isfinite(request.cfgScale), "recommended image request fills cfg");
		ok &= expect(request.sampleSteps > 0, "recommended image request fills steps");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/sd_turbo.safetensors";
		const auto realtime =
			ofxGgmlStableDiffusionParameterTuningHelpers::resolveRecommendedRealtimeSettings(
				settings);
		ok &= expect(
			realtime.mode == ofxGgmlStableDiffusionRealtimeMode::LowLatency,
			"turbo models prefer low-latency realtime mode");
		ok &= expect(
			realtime.targetLatencyMs <= 350,
			"turbo realtime recommendations tighten latency targets");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/video/Wan2.1-T2V-1.3B.gguf";
		const auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveVideoProfile(settings);
		ok &= expect(profile.modelFamily == ofxGgmlStableDiffusionModelFamily::WAN, "wan t2v profile detects family");
		ok &= expect(profile.defaultWidth == 832, "wan t2v defaults to 832px width");
		ok &= expect(profile.defaultHeight == 480, "wan t2v defaults to 480px height");
		ok &= expect(profile.defaultFrameCount == 33, "wan t2v defaults to 33 frames");
		ok &= expect(profile.defaultFps == 16, "wan t2v defaults to 16 fps");
		ok &= expect(profile.defaultSampleSteps == 20, "wan t2v defaults to 20 steps");
		ok &= expectNear(profile.defaultCfgScale, 6.0f, 0.0001f, "wan t2v cfg default");
		ok &= expectNear(profile.defaultStrength, 0.75f, 0.0001f, "wan t2v strength default");

		const auto realtimeVideo =
			ofxGgmlStableDiffusionParameterTuningHelpers::resolveRecommendedRealtimeVideoSettings(
				settings);
		ok &= expect(realtimeVideo.previewWidth == 832, "wan realtime video keeps 832px width");
		ok &= expect(realtimeVideo.previewHeight == 480, "wan realtime video keeps 480px height");
		ok &= expect(
			realtimeVideo.refineSteps >= realtimeVideo.previewSteps,
			"realtime video refine budget is not below preview budget");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/flux/flux1-dev-Q8_0.gguf";
		ofxGgmlStableDiffusionCapabilities capabilities;
		capabilities.contextConfigured = true;
		capabilities.textToImage = true;

		ofxGgmlStableDiffusionRealtimeRequest preview;
		preview.prompt = "stage visuals";
		preview.width = 640;
		preview.height = 640;
		preview.sampleSteps = 4;
		preview.cfgScale = 1.5f;

		ofxGgmlStableDiffusionCreativeWorkflowSettings workflow;
		workflow.renderSampleSteps = 40;
		workflow.renderCfgScale = 7.5f;

		const auto request = ofxGgmlStableDiffusionBuildCreativeRenderRequest(
			preview,
			settings,
			workflow,
			&capabilities);

		ok &= expect(request.prompt == preview.prompt, "creative render keeps prompt");
		ok &= expect(request.sampleSteps == 40, "creative render overrides sample steps");
		ok &= expectNear(request.cfgScale, 7.5f, 0.0001f, "creative render overrides cfg");
	}

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
