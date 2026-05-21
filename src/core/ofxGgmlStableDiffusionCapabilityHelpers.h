#pragma once

#include "ofxGgmlStableDiffusionStringUtils.h"
#include "ofxGgmlStableDiffusionTypes.h"

#include <cctype>
#include <initializer_list>
#include <string>
#include <vector>

inline const char * ofxGgmlStableDiffusionModelFamilyLabel(ofxGgmlStableDiffusionModelFamily family) {
	switch (family) {
	case ofxGgmlStableDiffusionModelFamily::SD1: return "SD1";
	case ofxGgmlStableDiffusionModelFamily::SD2: return "SD2";
	case ofxGgmlStableDiffusionModelFamily::SDXL: return "SDXL";
	case ofxGgmlStableDiffusionModelFamily::SD3: return "SD3";
	case ofxGgmlStableDiffusionModelFamily::FLUX: return "FLUX";
	case ofxGgmlStableDiffusionModelFamily::FLUXFill: return "FLUXFill";
	case ofxGgmlStableDiffusionModelFamily::FLUXControl: return "FLUXControl";
	case ofxGgmlStableDiffusionModelFamily::FLUX2: return "FLUX2";
	case ofxGgmlStableDiffusionModelFamily::WAN: return "WAN";
	case ofxGgmlStableDiffusionModelFamily::WANI2V: return "WANI2V";
	case ofxGgmlStableDiffusionModelFamily::WANTI2V: return "WANTI2V";
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V: return "WANFLF2V";
	case ofxGgmlStableDiffusionModelFamily::WANVACE: return "WANVACE";
	case ofxGgmlStableDiffusionModelFamily::Unknown:
	default:
		return "Unknown";
	}
}

namespace ofxGgmlStableDiffusionCapabilityHelpers {

inline char descriptorBoundaryClass(char ch) {
	if (!std::isalnum(static_cast<unsigned char>(ch))) {
		return '\0';
	}
	return std::isdigit(static_cast<unsigned char>(ch)) ? 'd' : 'a';
}

inline bool containsBoundedToken(
	const std::string& haystack,
	const std::string& needle) {
	if (needle.empty() || haystack.empty() || needle.size() > haystack.size()) {
		return false;
	}
	std::size_t pos = haystack.find(needle);
	while (pos != std::string::npos) {
		const char beforeClass =
			(pos == 0) ? '\0' : descriptorBoundaryClass(haystack[pos - 1]);
		const std::size_t end = pos + needle.size();
		const char afterClass =
			(end >= haystack.size()) ? '\0' : descriptorBoundaryClass(haystack[end]);
		const char needleFirstClass = descriptorBoundaryClass(needle.front());
		const char needleLastClass = descriptorBoundaryClass(needle.back());
		const bool startBounded =
			beforeClass == '\0' ||
			needleFirstClass == '\0' ||
			beforeClass != needleFirstClass;
		const bool endBounded =
			afterClass == '\0' ||
			needleLastClass == '\0' ||
			afterClass != needleLastClass;
		if (startBounded && endBounded) {
			return true;
		}
		pos = haystack.find(needle, pos + 1);
	}
	return false;
}

inline bool containsAny(
	const std::string& haystack,
	std::initializer_list<const char*> needles) {
	for (const char* needle : needles) {
		if (containsBoundedToken(haystack, needle)) {
			return true;
		}
	}
	return false;
}

inline bool hasConfiguredModelPaths(const ofxGgmlStableDiffusionContextSettings& settings) {
	return !settings.modelPath.empty() ||
		!settings.diffusionModelPath.empty() ||
		!settings.clipLPath.empty() ||
		!settings.clipGPath.empty() ||
		!settings.t5xxlPath.empty();
}

inline std::string buildModelFilenameDescriptor(const std::string& path) {
	const std::string lower = ofxSdToLowerCopy(path);
	const std::size_t separator = lower.find_last_of("/\\");
	if (separator == std::string::npos) {
		return lower;
	}
	return lower.substr(separator + 1);
}

inline std::string buildPrimaryModelDescriptor(const ofxGgmlStableDiffusionContextSettings& settings) {
	if (!settings.modelPath.empty()) {
		return buildModelFilenameDescriptor(settings.modelPath);
	}
	if (!settings.diffusionModelPath.empty()) {
		return buildModelFilenameDescriptor(settings.diffusionModelPath);
	}
	if (!settings.clipLPath.empty()) {
		return buildModelFilenameDescriptor(settings.clipLPath);
	}
	if (!settings.clipGPath.empty()) {
		return buildModelFilenameDescriptor(settings.clipGPath);
	}
	return buildModelFilenameDescriptor(settings.t5xxlPath);
}

inline std::vector<std::string> buildAllModelDescriptors(const ofxGgmlStableDiffusionContextSettings& settings) {
	std::vector<std::string> descriptors;
	const auto addDescriptor = [&descriptors](const std::string& path) {
		if (!path.empty()) {
			descriptors.push_back(buildModelFilenameDescriptor(path));
		}
	};
	addDescriptor(settings.modelPath);
	addDescriptor(settings.diffusionModelPath);
	addDescriptor(settings.clipLPath);
	addDescriptor(settings.clipGPath);
	addDescriptor(settings.t5xxlPath);
	return descriptors;
}

inline ofxGgmlStableDiffusionModelFamily inferModelFamily(const ofxGgmlStableDiffusionContextSettings& settings) {
	const std::string primary = buildPrimaryModelDescriptor(settings);
	const std::vector<std::string> all = buildAllModelDescriptors(settings);
	const auto hasHint = [&primary, &all](std::initializer_list<const char*> hints) {
		if (containsAny(primary, hints)) {
			return true;
		}
		for (const auto& descriptor : all) {
			if (containsAny(descriptor, hints)) {
				return true;
			}
		}
		return false;
	};

	if (hasHint({"wan", "wanvideo"})) {
		if (hasHint({"vace"})) {
			return ofxGgmlStableDiffusionModelFamily::WANVACE;
		}
		if (hasHint({"flf2v"})) {
			return ofxGgmlStableDiffusionModelFamily::WANFLF2V;
		}
		if (hasHint({"ti2v"})) {
			return ofxGgmlStableDiffusionModelFamily::WANTI2V;
		}
		if (hasHint({"i2v"})) {
			return ofxGgmlStableDiffusionModelFamily::WANI2V;
		}
		return ofxGgmlStableDiffusionModelFamily::WAN;
	}

	if (hasHint({"flux2", "flux-2", "flux_2", "klein"})) {
		return ofxGgmlStableDiffusionModelFamily::FLUX2;
	}

	if (hasHint({"flux", "flex.2", "flex-2", "flex_2"})) {
		if (hasHint({"control", "controls", "flex.2", "flex-2", "flex_2"})) {
			return ofxGgmlStableDiffusionModelFamily::FLUXControl;
		}
		if (hasHint({"fill", "inpaint"})) {
			return ofxGgmlStableDiffusionModelFamily::FLUXFill;
		}
		return ofxGgmlStableDiffusionModelFamily::FLUX;
	}

	if (hasHint({"sd3", "stable-diffusion-3"})) {
		return ofxGgmlStableDiffusionModelFamily::SD3;
	}

	if (hasHint({"sdxl", "xl-base", "juggernautxl", "ponyxl"})) {
		return ofxGgmlStableDiffusionModelFamily::SDXL;
	}

	if (hasHint({"sd2", "sd-2", "v2-1", "v2.1"})) {
		return ofxGgmlStableDiffusionModelFamily::SD2;
	}

	if (hasHint({"sd1", "sd-1", "sd15", "sd-15", "v1-5", "v1.5"})) {
		return ofxGgmlStableDiffusionModelFamily::SD1;
	}

	if (!settings.diffusionModelPath.empty() &&
		(!settings.clipLPath.empty() || !settings.t5xxlPath.empty())) {
		return ofxGgmlStableDiffusionModelFamily::FLUX;
	}

	return ofxGgmlStableDiffusionModelFamily::Unknown;
}

inline bool familySupportsImageGeneration(ofxGgmlStableDiffusionModelFamily family) {
	switch (family) {
	case ofxGgmlStableDiffusionModelFamily::WANI2V:
	case ofxGgmlStableDiffusionModelFamily::WANTI2V:
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V:
	case ofxGgmlStableDiffusionModelFamily::WANVACE:
	case ofxGgmlStableDiffusionModelFamily::WAN:
		return false;
	case ofxGgmlStableDiffusionModelFamily::Unknown:
	default:
		return true;
	}
}

inline bool familySupportsVideo(ofxGgmlStableDiffusionModelFamily family) {
	switch (family) {
	case ofxGgmlStableDiffusionModelFamily::WAN:
	case ofxGgmlStableDiffusionModelFamily::WANI2V:
	case ofxGgmlStableDiffusionModelFamily::WANTI2V:
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V:
	case ofxGgmlStableDiffusionModelFamily::WANVACE:
		return true;
	default:
		return false;
	}
}

inline bool familyRequiresInputImageForVideo(ofxGgmlStableDiffusionModelFamily family) {
	switch (family) {
	case ofxGgmlStableDiffusionModelFamily::WANI2V:
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V:
	case ofxGgmlStableDiffusionModelFamily::WANVACE:
		return true;
	default:
		return false;
	}
}

inline bool familySupportsVideoEndFrame(ofxGgmlStableDiffusionModelFamily family) {
	switch (family) {
	case ofxGgmlStableDiffusionModelFamily::WANI2V:
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V:
		return true;
	default:
		return false;
	}
}

inline bool familySupportsWrapperVideoAnimation(ofxGgmlStableDiffusionModelFamily family) {
	switch (family) {
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V:
		return true;
	case ofxGgmlStableDiffusionModelFamily::WAN:
	case ofxGgmlStableDiffusionModelFamily::WANI2V:
	case ofxGgmlStableDiffusionModelFamily::WANTI2V:
	case ofxGgmlStableDiffusionModelFamily::WANVACE:
		return false;
	default:
		return false;
	}
}

inline bool familySupportsPhotoMaker(ofxGgmlStableDiffusionModelFamily family) {
	return family == ofxGgmlStableDiffusionModelFamily::SDXL;
}

inline bool familyHasNativeControlModel(ofxGgmlStableDiffusionModelFamily family) {
	return family == ofxGgmlStableDiffusionModelFamily::FLUXControl;
}

inline bool familySupportsExternalControlNet(ofxGgmlStableDiffusionModelFamily family) {
	switch (family) {
	case ofxGgmlStableDiffusionModelFamily::WANI2V:
	case ofxGgmlStableDiffusionModelFamily::WANTI2V:
	case ofxGgmlStableDiffusionModelFamily::WANFLF2V:
	case ofxGgmlStableDiffusionModelFamily::WANVACE:
	case ofxGgmlStableDiffusionModelFamily::WAN:
		return false;
	default:
		return true;
	}
}

inline ofxGgmlStableDiffusionCapabilities resolveCapabilities(
	const ofxGgmlStableDiffusionContextSettings& settings,
	const ofxGgmlStableDiffusionUpscalerSettings& upscalerSettings) {
	ofxGgmlStableDiffusionCapabilities capabilities;
	capabilities.contextConfigured = hasConfiguredModelPaths(settings);
	capabilities.runtimeResolved = capabilities.contextConfigured;
	capabilities.modelFamily = inferModelFamily(settings);
	capabilities.splitModelPaths =
		!settings.diffusionModelPath.empty() ||
		!settings.clipLPath.empty() ||
		!settings.clipGPath.empty() ||
		!settings.t5xxlPath.empty();
	capabilities.controlNetConfigured = !settings.controlNetPath.empty();
	capabilities.photoMakerConfigured = !settings.stackedIdEmbedDir.empty();
	capabilities.nativeControlModel = familyHasNativeControlModel(capabilities.modelFamily);
	capabilities.upscaling = upscalerSettings.enabled && !upscalerSettings.modelPath.empty();
	capabilities.mmap = settings.enableMmap;
	capabilities.flashAttention = settings.flashAttn;
	capabilities.videoMetadataExport = true;

	if (!capabilities.contextConfigured) {
		return capabilities;
	}

	const bool imageGeneration = familySupportsImageGeneration(capabilities.modelFamily);
	capabilities.textToImage = imageGeneration;
	capabilities.imageToImage = imageGeneration;
	capabilities.inpainting = imageGeneration;
	capabilities.imageToVideo = familySupportsVideo(capabilities.modelFamily);
	capabilities.videoRequiresInputImage =
		familyRequiresInputImageForVideo(capabilities.modelFamily);
	capabilities.videoEndFrame = familySupportsVideoEndFrame(capabilities.modelFamily);
	capabilities.videoAnimation = familySupportsWrapperVideoAnimation(capabilities.modelFamily);
	capabilities.lora = imageGeneration || capabilities.imageToVideo;
	capabilities.embeddings = imageGeneration;
	capabilities.controlNet =
		capabilities.nativeControlModel ||
		(capabilities.controlNetConfigured &&
			familySupportsExternalControlNet(capabilities.modelFamily));
	capabilities.photoMaker =
		capabilities.photoMakerConfigured &&
		familySupportsPhotoMaker(capabilities.modelFamily);

	return capabilities;
}

} // namespace ofxGgmlStableDiffusionCapabilityHelpers
