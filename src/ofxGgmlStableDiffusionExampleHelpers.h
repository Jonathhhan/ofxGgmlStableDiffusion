#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <string>

inline std::string ofxGgmlStableDiffusionExampleInputString(
	const std::array<char, 512>& input) {
	return std::string(input.data());
}

inline void ofxGgmlStableDiffusionExampleCopyToInput(
	const std::string& value,
	std::array<char, 512>& input) {
	std::fill(input.begin(), input.end(), '\0');
	const auto count = std::min(value.size(), input.size() - 1);
	std::copy_n(value.begin(), count, input.begin());
}

inline std::string ofxGgmlStableDiffusionExampleResolveReadablePath(
	const std::string& path) {
	if (path.empty()) {
		return "";
	}
	if (ofFile::doesFileExist(path)) {
		return path;
	}
	const auto dataPath = ofToDataPath(path, true);
	if (ofFile::doesFileExist(dataPath)) {
		return dataPath;
	}
	return path;
}

inline std::string ofxGgmlStableDiffusionExampleEnvOrReadablePath(
	std::initializer_list<const char*> envNames,
	std::initializer_list<std::string> candidates) {
	for (const char* envName : envNames) {
		if (envName == nullptr) {
			continue;
		}
		const std::string value = ofGetEnv(envName);
		if (!value.empty()) {
			return ofxGgmlStableDiffusionExampleResolveReadablePath(value);
		}
	}
	for (const auto& candidate : candidates) {
		const auto resolved =
			ofxGgmlStableDiffusionExampleResolveReadablePath(candidate);
		if (ofFile::doesFileExist(resolved)) {
			return resolved;
		}
	}
	if (candidates.size() == 0) {
		return "";
	}
	return ofxGgmlStableDiffusionExampleResolveReadablePath(*candidates.begin());
}

inline sd_image_t ofxGgmlStableDiffusionExampleImageView(ofPixels& pixels) {
	return {
		static_cast<uint32_t>(pixels.getWidth()),
		static_cast<uint32_t>(pixels.getHeight()),
		static_cast<uint32_t>(pixels.getNumChannels()),
		pixels.getData()
	};
}

inline bool ofxGgmlStableDiffusionExampleLoadImageView(
	const std::string& path,
	int width,
	int height,
	ofImage& preview,
	ofPixels& pixels,
	sd_image_t& view) {
	const auto resolvedPath =
		ofxGgmlStableDiffusionExampleResolveReadablePath(path);
	if (!ofFile::doesFileExist(resolvedPath) || !preview.load(resolvedPath)) {
		return false;
	}
	preview.resize(width, height);
	pixels = preview.getPixels();
	view = ofxGgmlStableDiffusionExampleImageView(pixels);
	return true;
}

inline bool ofxGgmlStableDiffusionExampleRequestCancel(
	ofxGgmlStableDiffusion& sd,
	std::string& statusMessage,
	const std::string& message = "Cancelling at the next safe checkpoint...") {
	if (sd.requestCancellation()) {
		statusMessage = message;
		return true;
	}
	return false;
}

inline std::string ofxGgmlStableDiffusionExampleRuntimeLabel(
	ofxGgmlStableDiffusion& sd) {
	const char* rawInfo = sd.getSystemInfo();
	const std::string info = rawInfo ? rawInfo : "";
	std::string lower = info;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	if (lower.find("cuda") != std::string::npos) {
		return "Runtime: CUDA";
	}
	if (lower.find("vulkan") != std::string::npos) {
		return "Runtime: Vulkan";
	}
	if (lower.find("metal") != std::string::npos) {
		return "Runtime: Metal";
	}
	if (!info.empty()) {
		return "Runtime: CPU / native";
	}
	return "Runtime: unknown";
}

inline void ofxGgmlStableDiffusionExampleDrawImageFit(
	const ofImage& image,
	float scaleFactor = 0.82f) {
	if (!image.isAllocated()) {
		return;
	}
	const float scale = std::min(
		ofGetWidth() / image.getWidth(),
		ofGetHeight() / image.getHeight()) * scaleFactor;
	const float w = image.getWidth() * scale;
	const float h = image.getHeight() * scale;
	const float x = (ofGetWidth() - w) * 0.5f;
	const float y = (ofGetHeight() - h) * 0.5f;
	image.draw(x, y, w, h);
}
