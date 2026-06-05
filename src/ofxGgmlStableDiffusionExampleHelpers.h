#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

inline std::string ofxGgmlStableDiffusionExampleLower(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
}

inline std::string ofxGgmlStableDiffusionExampleCurrentExeDir() {
#ifdef _WIN32
	char path[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) {
		return "";
	}
	const std::string exePath(path);
	const auto separator = exePath.find_last_of("\\/");
	if (separator == std::string::npos) {
		return "";
	}
	return exePath.substr(0, separator + 1);
#else
	return "";
#endif
}

inline std::string ofxGgmlStableDiffusionExampleJoinPath(
	const std::string& directory,
	const std::string& filename) {
	if (directory.empty()) {
		return filename;
	}
	const char last = directory[directory.size() - 1];
	if (last == '/' || last == '\\') {
		return directory + filename;
	}
#ifdef _WIN32
	return directory + "\\" + filename;
#else
	return directory + "/" + filename;
#endif
}

inline bool ofxGgmlStableDiffusionExampleFileContainsAny(
	const std::string& path,
	std::initializer_list<std::string> needles) {
	if (path.empty()) {
		return false;
	}
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		return false;
	}

	std::string carry;
	char buffer[65536];
	while (input) {
		input.read(buffer, sizeof(buffer));
		const auto count = input.gcount();
		if (count <= 0) {
			break;
		}
		std::string chunk = carry + std::string(buffer, static_cast<std::size_t>(count));
		chunk = ofxGgmlStableDiffusionExampleLower(std::move(chunk));
		for (const auto& needle : needles) {
			if (chunk.find(needle) != std::string::npos) {
				return true;
			}
		}
		carry = chunk.size() > 64 ? chunk.substr(chunk.size() - 64) : chunk;
	}
	return false;
}

inline bool ofxGgmlStableDiffusionExampleSelectedRuntimeLooksCuda() {
	static bool checked = false;
	static bool looksCuda = false;
	if (checked) {
		return looksCuda;
	}

	const std::array<std::string, 3> candidates = {
		ofxGgmlStableDiffusionExampleJoinPath(
			ofxGgmlStableDiffusionExampleCurrentExeDir(),
			"stable-diffusion.dll"),
		ofToDataPath("stable-diffusion.dll", true),
		"stable-diffusion.dll"
	};
	for (const auto& path : candidates) {
		if (ofxGgmlStableDiffusionExampleFileContainsAny(
				path,
				{"cublas64_", "cudart64_", "ggml_cuda_init", "ggml-cuda"})) {
			looksCuda = true;
			break;
		}
	}
	checked = true;
	return looksCuda;
}

inline std::string ofxGgmlStableDiffusionExampleRuntimeLabel(
	ofxGgmlStableDiffusion& sd) {
	const char* rawInfo = sd.getSystemInfo();
	const std::string info = rawInfo ? rawInfo : "";
	const std::string lower = ofxGgmlStableDiffusionExampleLower(info);

	if (lower.find("cuda") != std::string::npos) {
		return "Runtime: CUDA";
	}
	if (lower.find("vulkan") != std::string::npos) {
		return "Runtime: Vulkan";
	}
	if (lower.find("metal") != std::string::npos) {
		return "Runtime: Metal";
	}
	const std::string envBackend =
		ofxGgmlStableDiffusionExampleLower(ofGetEnv("OFXGGML_STABLE_DIFFUSION_BACKEND"));
	if (envBackend.find("cuda") != std::string::npos ||
		ofxGgmlStableDiffusionExampleSelectedRuntimeLooksCuda()) {
		return "Runtime: CUDA";
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
