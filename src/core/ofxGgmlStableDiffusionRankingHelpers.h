#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

enum class ofxGgmlStableDiffusionImageSelectionMode {
	KeepOrder = 0,
	Rerank,
	BestOnly
};

struct ofxGgmlStableDiffusionImageScore {
	bool valid = false;
	float score = 0.0f;
	std::string scorer;
	std::string summary;
	std::vector<std::pair<std::string, std::string>> metadata;
};

inline const char * ofxGgmlStableDiffusionImageSelectionModeName(
	ofxGgmlStableDiffusionImageSelectionMode mode) {
	switch (mode) {
	case ofxGgmlStableDiffusionImageSelectionMode::Rerank: return "Rerank";
	case ofxGgmlStableDiffusionImageSelectionMode::BestOnly: return "BestOnly";
	case ofxGgmlStableDiffusionImageSelectionMode::KeepOrder:
	default:
		return "KeepOrder";
	}
}

inline std::vector<std::size_t> ofxGgmlStableDiffusionBuildRankedImageOrder(
	const std::vector<ofxGgmlStableDiffusionImageScore> & scores) {
	std::vector<std::size_t> order(scores.size());
	for (std::size_t i = 0; i < scores.size(); ++i) {
		order[i] = i;
	}

	std::stable_sort(order.begin(), order.end(), [&scores](std::size_t a, std::size_t b) {
		const auto & left = scores[a];
		const auto & right = scores[b];
		const float leftScore = left.valid ? left.score : -std::numeric_limits<float>::infinity();
		const float rightScore = right.valid ? right.score : -std::numeric_limits<float>::infinity();
		return leftScore > rightScore;
	});

	return order;
}
