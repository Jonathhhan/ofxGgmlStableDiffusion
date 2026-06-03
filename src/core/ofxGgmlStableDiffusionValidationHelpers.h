#pragma once

#include <cmath>
#include <filesystem>
#include <string>

inline bool ofxSdOptionalFloatUsesAutoValue(float value) {
	return std::isinf(value) && value > 0.0f;
}

inline bool ofxSdOptionalFloatIsFiniteWhenProvided(float value) {
	return ofxSdOptionalFloatUsesAutoValue(value) || std::isfinite(value);
}

inline bool ofxSdPathHasParentTraversal(const std::string& value) {
	if (value.empty()) {
		return false;
	}

	std::size_t segmentStart = 0;
	for (std::size_t i = 0; i <= value.size(); ++i) {
		if (i == value.size() || value[i] == '/' || value[i] == '\\') {
			if (i - segmentStart == 2 &&
				value[segmentStart] == '.' &&
				value[segmentStart + 1] == '.') {
				return true;
			}
			segmentStart = i + 1;
		}
	}

	const std::filesystem::path path(value);
	for (const auto& part : path) {
		if (part == "..") {
			return true;
		}
	}
	return false;
}

inline bool ofxSdIsSafeChildPathComponent(const std::string& value) {
	if (value.empty()) {
		return false;
	}
	if (ofxSdPathHasParentTraversal(value)) {
		return false;
	}
	if (value.find('/') != std::string::npos ||
		value.find('\\') != std::string::npos ||
		value.find(':') != std::string::npos) {
		return false;
	}

	const std::filesystem::path path(value);
	return !path.has_parent_path() &&
		path.filename() == path &&
		path.filename() != "." &&
		path.filename() != "..";
}
