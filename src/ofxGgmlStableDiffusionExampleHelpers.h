#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"

#include <algorithm>
#include <array>
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
	const std::string& message = "Cancelling after current step...") {
	if (sd.isGenerating() && sd.requestCancellation()) {
		statusMessage = message;
		return true;
	}
	return false;
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
