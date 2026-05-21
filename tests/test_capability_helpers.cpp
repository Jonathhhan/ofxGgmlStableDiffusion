#include "core/ofxGgmlStableDiffusionCapabilityHelpers.h"

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

bool expectFamily(
	ofxGgmlStableDiffusionModelFamily actual,
	ofxGgmlStableDiffusionModelFamily expected,
	const std::string& label) {
	if (actual == expected) {
		return true;
	}

	std::cerr << "FAIL: " << label
		<< " expected " << ofxGgmlStableDiffusionModelFamilyLabel(expected)
		<< " but got " << ofxGgmlStableDiffusionModelFamilyLabel(actual)
		<< std::endl;
	return false;
}

} // namespace

int main() {
	bool ok = true;

	{
		ofxGgmlStableDiffusionContextSettings settings;
		ofxGgmlStableDiffusionUpscalerSettings upscaler;
		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expect(!capabilities.contextConfigured, "empty context is not configured");
		ok &= expect(!capabilities.runtimeResolved, "empty context is not resolved");
		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::Unknown,
			"empty context family");
		ok &= expect(!capabilities.textToImage, "empty context disables text-to-image");
		ok &= expect(!capabilities.imageToVideo, "empty context disables video");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/sdxl/juggernautXL.safetensors";
		settings.controlNetPath = "models/controlnet/control_v11p_sd15_openpose.safetensors";
		settings.stackedIdEmbedDir = "models/photomaker/photomaker-v2.bin";
		ofxGgmlStableDiffusionUpscalerSettings upscaler;

		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expect(capabilities.contextConfigured, "sdxl context is configured");
		ok &= expect(capabilities.runtimeResolved, "sdxl context resolves runtime support");
		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::SDXL,
			"sdxl family");
		ok &= expect(capabilities.textToImage, "sdxl supports text-to-image");
		ok &= expect(capabilities.imageToImage, "sdxl supports image-to-image");
		ok &= expect(!capabilities.imageToVideo, "sdxl does not expose video generation");
		ok &= expect(!capabilities.videoAnimation, "sdxl does not expose video animation");
		ok &= expect(capabilities.controlNetConfigured, "sdxl controlnet path is tracked");
		ok &= expect(capabilities.controlNet, "sdxl resolves controlnet support");
		ok &= expect(capabilities.photoMakerConfigured, "sdxl photomaker path is tracked");
		ok &= expect(capabilities.photoMaker, "sdxl resolves photomaker support");
		ok &= expect(!capabilities.videoEndFrame, "sdxl does not expose end-frame morphing");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/video/wan2.1-flf2v-14b-Q4_K_M.gguf";
		ofxGgmlStableDiffusionUpscalerSettings upscaler;

		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::WANFLF2V,
			"wan flf2v family");
		ok &= expect(!capabilities.textToImage, "wan flf2v disables image generation modes");
		ok &= expect(capabilities.imageToVideo, "wan flf2v supports image-to-video");
		ok &= expect(capabilities.videoEndFrame, "wan flf2v supports end-frame morphing");
		ok &= expect(capabilities.videoAnimation, "wan flf2v supports wrapper animation path");
		ok &= expect(!capabilities.controlNet, "wan flf2v does not resolve controlnet support");
		ok &= expect(!capabilities.photoMaker, "wan flf2v does not resolve photomaker support");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/video/Wan2.2-TI2V-5B.gguf";
		ofxGgmlStableDiffusionUpscalerSettings upscaler;

		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::WANTI2V,
			"wan ti2v family");
		ok &= expect(capabilities.imageToVideo, "wan ti2v supports video generation");
		ok &= expect(!capabilities.videoRequiresInputImage, "wan ti2v allows text-to-video without input image");
		ok &= expect(!capabilities.videoAnimation, "wan ti2v does not expose animation wrapper");
		ok &= expect(!capabilities.videoEndFrame, "wan ti2v does not advertise end-frame morphing");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/flux/flux-controls-dev.safetensors";
		ofxGgmlStableDiffusionUpscalerSettings upscaler;

		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::FLUXControl,
			"flux control family");
		ok &= expect(capabilities.nativeControlModel, "flux control family marks native control support");
		ok &= expect(capabilities.controlNet, "flux control family resolves control support without external path");
		ok &= expect(capabilities.textToImage, "flux control still supports image generation");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/sdxl/checkpoints/base-model.gguf";
		ofxGgmlStableDiffusionUpscalerSettings upscaler;

		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::Unknown,
			"directory name alone does not force sdxl family");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.modelPath = "models/checkpoints/notwanvideohelper.gguf";
		ofxGgmlStableDiffusionUpscalerSettings upscaler;

		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::Unknown,
			"unbounded wan substring does not force wan family");
	}

	{
		ofxGgmlStableDiffusionContextSettings settings;
		settings.diffusionModelPath = "models/flux/flux1-dev-Q8_0.gguf";
		settings.clipLPath = "models/flux/clip_l.safetensors";
		settings.t5xxlPath = "models/flux/t5xxl_fp16.safetensors";
		ofxGgmlStableDiffusionUpscalerSettings upscaler;
		upscaler.enabled = true;
		upscaler.modelPath = "models/esrgan/4x.pth";

		const auto capabilities =
			ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(settings, upscaler);

		ok &= expectFamily(
			capabilities.modelFamily,
			ofxGgmlStableDiffusionModelFamily::FLUX,
			"split flux family");
		ok &= expect(capabilities.splitModelPaths, "split flux settings are detected");
		ok &= expect(capabilities.textToImage, "split flux supports image generation");
		ok &= expect(!capabilities.imageToVideo, "split flux does not advertise video generation");
		ok &= expect(!capabilities.videoAnimation, "split flux does not advertise animation");
		ok &= expect(capabilities.upscaling, "configured upscaler is reflected in capabilities");
	}

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
