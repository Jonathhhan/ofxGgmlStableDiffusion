#pragma once

#include "ofMain.h"
#include "core/ofxGgmlStableDiffusionTypes.h"

#include <cstdint>
#include <memory>
#include <string>

struct ofxGgmlStableDiffusionHoloscanSettings {
	bool enabled = false;
	bool useEventScheduler = true;
	int workerThreads = 2;
	bool enableRerankStage = false;
	bool enableUpscaleStage = false;
	bool pinWorkers = false;
};

struct ofxGgmlStableDiffusionHoloscanFramePacket {
	uint64_t frameIndex = 0;
	double timestampSeconds = 0.0;
	std::shared_ptr<ofPixels> pixels;
	std::string sourceLabel;

	bool isValid() const {
		return pixels != nullptr && pixels->isAllocated();
	}
};

struct ofxGgmlStableDiffusionHoloscanConditioningPacket {
	uint64_t frameIndex = 0;
	double timestampSeconds = 0.0;
	std::string prompt;
	std::string negativePrompt;
	std::shared_ptr<ofPixels> initImage;
	float strength = 0.35f;

	bool isValid() const {
		return initImage != nullptr && initImage->isAllocated() && !prompt.empty();
	}
};

struct ofxGgmlStableDiffusionHoloscanImagePacket {
	uint64_t frameIndex = 0;
	double timestampSeconds = 0.0;
	ofxGgmlStableDiffusionImageFrame imageFrame;
};

struct ofxGgmlStableDiffusionHoloscanPreviewFrame {
	bool valid = false;
	uint64_t frameIndex = 0;
	double timestampSeconds = 0.0;
	ofPixels pixels;
};
