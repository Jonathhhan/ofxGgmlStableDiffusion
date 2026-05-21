#pragma once

#include "bridges/ofxGgmlStableDiffusionHoloscanTypes.h"

#include <memory>
#include <string>
#include <vector>

class ofxGgmlStableDiffusion;

class ofxGgmlStableDiffusionHoloscanBridge {
public:
	ofxGgmlStableDiffusionHoloscanBridge();
	~ofxGgmlStableDiffusionHoloscanBridge();

	bool setup(
		ofxGgmlStableDiffusion* diffusion,
		const ofxGgmlStableDiffusionHoloscanSettings& settings = {});
	void shutdown();

	bool startImagePipeline();
	void stop();
	void update();

	void submitFrame(
		const ofPixels& pixels,
		double timestampSeconds,
		const std::string& sourceLabel = "frame");

	void submitPrompt(
		const std::string& prompt,
		const std::string& negativePrompt = "");

	bool hasPreviewFrame() const;
	const ofTexture& getPreviewTexture() const;
	ofxGgmlStableDiffusionHoloscanPreviewFrame getPreviewFrameCopy() const;
	std::vector<ofxGgmlStableDiffusionImageFrame> consumeFinishedImages();

	bool isConfigured() const;
	bool isRunning() const;
	bool isHoloscanAvailable() const;
	std::string getLastError() const;
	const ofxGgmlStableDiffusionHoloscanSettings& getSettings() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
