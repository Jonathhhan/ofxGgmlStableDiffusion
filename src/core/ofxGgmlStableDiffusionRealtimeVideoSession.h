#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusionTypes.h"
#include <cmath>
#include <functional>
#include <limits>
#include <mutex>
#include <string>

class ofxGgmlStableDiffusion;

enum class ofxGgmlStableDiffusionRealtimeVideoQuality {
	Preview,
	Refine
};

struct ofxGgmlStableDiffusionRealtimeVideoSettings {
	int previewWidth = 512;
	int previewHeight = 512;
	int previewSteps = 4;
	int refineSteps = 16;
	float previewStrength = 0.68f;
	float refineStrength = 0.35f;
	float cfgScale = 1.5f;
	sample_method_t sampleMethod = EULER_A_SAMPLE_METHOD;
	int64_t seed = -1;
	bool lockSeed = false;
	bool usePreviousFrameFeedback = true;
	bool coalescePromptUpdates = true;
	bool dropIfBusy = false;
	bool enableRefineOnIdle = true;
	uint64_t refineAfterStableMs = 700;
};

struct ofxGgmlStableDiffusionRealtimeVideoRequest {
	std::string prompt;
	std::string negativePrompt;
	int width = 0;
	int height = 0;
	int previewSteps = -1;
	int refineSteps = -1;
	float cfgScale = std::numeric_limits<float>::infinity();
	float previewStrength = std::numeric_limits<float>::infinity();
	float refineStrength = std::numeric_limits<float>::infinity();
	sample_method_t sampleMethod = SAMPLE_METHOD_COUNT;
	int64_t seed = std::numeric_limits<int64_t>::min();
	bool lockSeed = false;
};

struct ofxGgmlStableDiffusionRealtimeVideoStats {
	bool isActive = false;
	int promptUpdates = 0;
	int coalescedUpdates = 0;
	int droppedUpdates = 0;
	int framesGenerated = 0;
	int previewFrames = 0;
	int refineFrames = 0;
	float averageLatencyMs = 0.0f;
	float lastLatencyMs = 0.0f;
	ofxGgmlStableDiffusionRealtimeVideoQuality lastQuality =
		ofxGgmlStableDiffusionRealtimeVideoQuality::Preview;
};

struct ofxGgmlStableDiffusionRealtimeVideoFrame {
	ofxGgmlStableDiffusionResult result;
	ofxGgmlStableDiffusionImageFrame frame;
	ofxGgmlStableDiffusionRealtimeVideoQuality quality =
		ofxGgmlStableDiffusionRealtimeVideoQuality::Preview;
	std::string prompt;
	float latencyMs = 0.0f;
	int frameIndex = -1;
};

using ofxSdRealtimeVideoFrameCallback =
	std::function<void(const ofxGgmlStableDiffusionRealtimeVideoFrame&)>;
using ofxSdRealtimeVideoLatencyCallback =
	std::function<void(float latencyMs, ofxGgmlStableDiffusionRealtimeVideoQuality quality)>;

inline const char * ofxGgmlStableDiffusionRealtimeVideoQualityLabel(
	ofxGgmlStableDiffusionRealtimeVideoQuality quality) {
	switch (quality) {
	case ofxGgmlStableDiffusionRealtimeVideoQuality::Preview: return "preview";
	case ofxGgmlStableDiffusionRealtimeVideoQuality::Refine: return "refine";
	}
	return "preview";
}

inline bool ofxGgmlStableDiffusionFrameHasPixels(
	const ofxGgmlStableDiffusionImageFrame * frame) {
	return frame != nullptr && frame->pixels.isAllocated() &&
		frame->pixels.getData() != nullptr;
}

inline sd_image_t ofxGgmlStableDiffusionFrameToSdImage(
	const ofxGgmlStableDiffusionImageFrame & frame) {
	return sd_image_t{
		static_cast<uint32_t>(frame.pixels.getWidth()),
		static_cast<uint32_t>(frame.pixels.getHeight()),
		static_cast<uint32_t>(frame.pixels.getNumChannels()),
		const_cast<unsigned char *>(frame.pixels.getData())
	};
}

inline ofxGgmlStableDiffusionImageRequest ofxGgmlStableDiffusionBuildRealtimeVideoImageRequest(
	const ofxGgmlStableDiffusionRealtimeVideoSettings & settings,
	const ofxGgmlStableDiffusionRealtimeVideoRequest & request,
	ofxGgmlStableDiffusionRealtimeVideoQuality quality,
	const ofxGgmlStableDiffusionImageFrame * feedbackFrame = nullptr) {
	const bool isRefine = quality == ofxGgmlStableDiffusionRealtimeVideoQuality::Refine;
	const bool hasFeedback =
		settings.usePreviousFrameFeedback && ofxGgmlStableDiffusionFrameHasPixels(feedbackFrame);

	ofxGgmlStableDiffusionImageRequest imageRequest;
	imageRequest.mode = hasFeedback
		? ofxGgmlStableDiffusionImageMode::ImageToImage
		: ofxGgmlStableDiffusionImageMode::TextToImage;
	imageRequest.prompt = request.prompt;
	imageRequest.negativePrompt = request.negativePrompt;
	imageRequest.width = request.width > 0 ? request.width : settings.previewWidth;
	imageRequest.height = request.height > 0 ? request.height : settings.previewHeight;
	imageRequest.sampleMethod = request.sampleMethod != SAMPLE_METHOD_COUNT
		? request.sampleMethod
		: settings.sampleMethod;
	imageRequest.sampleSteps = isRefine
		? (request.refineSteps > 0 ? request.refineSteps : settings.refineSteps)
		: (request.previewSteps > 0 ? request.previewSteps : settings.previewSteps);
	imageRequest.cfgScale = std::isfinite(request.cfgScale)
		? request.cfgScale
		: settings.cfgScale;
	imageRequest.strength = isRefine
		? (std::isfinite(request.refineStrength) ? request.refineStrength : settings.refineStrength)
		: (std::isfinite(request.previewStrength) ? request.previewStrength : settings.previewStrength);
	const bool lockSeed = request.lockSeed || settings.lockSeed;
	if (request.seed != std::numeric_limits<int64_t>::min()) {
		imageRequest.seed = request.seed;
	} else {
		imageRequest.seed = lockSeed ? settings.seed : -1;
	}
	imageRequest.batchCount = 1;
	if (hasFeedback) {
		imageRequest.initImage = ofxGgmlStableDiffusionFrameToSdImage(*feedbackFrame);
	}
	return imageRequest;
}

class ofxGgmlStableDiffusionRealtimeVideoSession {
public:
	bool start(const ofxGgmlStableDiffusionRealtimeVideoSettings & settings);
	bool start(const ofxGgmlStableDiffusionRealtimeVideoSettings & settings, ofxGgmlStableDiffusion & sd);
	void stop();
	bool isActive() const;

	void setGenerator(ofxGgmlStableDiffusion * sd);
	bool submit(const ofxGgmlStableDiffusionRealtimeVideoRequest & request);
	void updatePrompt(const std::string & prompt);
	void updateNegativePrompt(const std::string & negativePrompt);
	/// Poll for completion and dispatch frame/latency callbacks.
	/// Call this from the application's update() loop; callbacks run on that same thread.
	void update();

	bool isGenerating() const;
	bool hasPendingRequest() const;
	void clearFeedbackFrame();

	ofxGgmlStableDiffusionRealtimeVideoStats getStats() const;
	ofxGgmlStableDiffusionRealtimeVideoFrame getLastFrame() const;
	ofxGgmlStableDiffusionRealtimeVideoSettings getSettings() const;

	/// Set the frame callback dispatched from update().
	void setFrameCallback(ofxSdRealtimeVideoFrameCallback callback);
	/// Set the latency callback dispatched from update().
	void setLatencyCallback(ofxSdRealtimeVideoLatencyCallback callback);

private:
	bool shouldStartRefine(uint64_t nowMicros) const;
	void processNext();
	void updateStats(float latencyMs, ofxGgmlStableDiffusionRealtimeVideoQuality quality);

	mutable std::mutex mutex_;
	ofxGgmlStableDiffusion * generator_ = nullptr;
	ofxGgmlStableDiffusionRealtimeVideoSettings settings_;
	ofxGgmlStableDiffusionRealtimeVideoRequest pendingRequest_;
	ofxGgmlStableDiffusionRealtimeVideoRequest activeRequest_;
	ofxGgmlStableDiffusionRealtimeVideoStats stats_;
	ofxGgmlStableDiffusionRealtimeVideoFrame lastFrame_;
	ofxSdRealtimeVideoFrameCallback frameCallback_;
	ofxSdRealtimeVideoLatencyCallback latencyCallback_;
	bool active_ = false;
	bool generationInFlight_ = false;
	bool hasPendingRequest_ = false;
	bool hasFeedbackFrame_ = false;
	bool refinedActiveRequest_ = false;
	uint64_t generationStartMicros_ = 0;
	uint64_t lastPromptChangeMicros_ = 0;
	ofxGgmlStableDiffusionRealtimeVideoQuality activeQuality_ =
		ofxGgmlStableDiffusionRealtimeVideoQuality::Preview;
};
