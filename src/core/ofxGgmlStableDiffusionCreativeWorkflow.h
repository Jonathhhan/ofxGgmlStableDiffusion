#pragma once

#include "ofxGgmlStableDiffusionParameterTuningHelpers.h"
#include "ofxGgmlStableDiffusionQueue.h"
#include "ofxGgmlStableDiffusionRealtimeSession.h"
#include "ofxGgmlStableDiffusionRealtimeVideoSession.h"

#include <cmath>
#include <limits>
#include <mutex>
#include <string>

class ofxGgmlStableDiffusion;

struct ofxGgmlStableDiffusionCreativeWorkflowSettings {
	ofxGgmlStableDiffusionRealtimeSettings imagePreviewSettings;
	ofxGgmlStableDiffusionRealtimeVideoSettings videoPreviewSettings;
	ofxGgmlStableDiffusionPriority queuedRenderPriority =
		ofxGgmlStableDiffusionPriority::High;
	int renderSampleSteps = -1;
	float renderCfgScale = std::numeric_limits<float>::infinity();
	bool autoApplyModelDefaults = true;
	bool pausePreviewWhileQueueDrains = false;
	std::string queueTag = "creative-workflow";
};

struct ofxGgmlStableDiffusionCreativeWorkflowSnapshot {
	ofxGgmlStableDiffusionContextSettings contextSettings;
	ofxGgmlStableDiffusionRealtimeRequest lastImagePreviewRequest;
	ofxGgmlStableDiffusionRealtimeVideoRequest lastVideoPreviewRequest;
	bool imagePreviewActive = false;
	bool videoPreviewActive = false;
	int queuedImageRequests = 0;
	int queuedVideoRequests = 0;
};

inline ofxGgmlStableDiffusionImageRequest ofxGgmlStableDiffusionBuildCreativeRenderRequest(
	const ofxGgmlStableDiffusionRealtimeRequest& previewRequest,
	const ofxGgmlStableDiffusionContextSettings& contextSettings,
	const ofxGgmlStableDiffusionCreativeWorkflowSettings& workflowSettings,
	const ofxGgmlStableDiffusionCapabilities* capabilities = nullptr) {
	ofxGgmlStableDiffusionImageRequest request;
	request.mode = ofxGgmlStableDiffusionImageMode::TextToImage;
	request.prompt = previewRequest.prompt;
	request.negativePrompt = previewRequest.negativePrompt;
	request.width = previewRequest.width;
	request.height = previewRequest.height;
	request.seed = previewRequest.seed;
	request.sampleMethod = previewRequest.sampleMethod;
	request.cfgScale = previewRequest.cfgScale;
	request.sampleSteps = previewRequest.sampleSteps;
	request.strength = previewRequest.strength;
	request.batchCount = 1;

	if (workflowSettings.autoApplyModelDefaults) {
		ofxGgmlStableDiffusionParameterTuningHelpers::applyRecommendedImageRequest(
			contextSettings,
			request,
			capabilities,
			false);
	}

	if (workflowSettings.renderSampleSteps > 0) {
		request.sampleSteps = workflowSettings.renderSampleSteps;
	}
	if (std::isfinite(workflowSettings.renderCfgScale)) {
		request.cfgScale = workflowSettings.renderCfgScale;
	}

	const auto profile = ofxGgmlStableDiffusionParameterTuningHelpers::resolveImageProfile(
		contextSettings,
		request.mode);
	ofxGgmlStableDiffusionParameterTuningHelpers::clampImageParametersToProfile(
		profile,
		request.cfgScale,
		request.sampleSteps,
		request.strength,
		request.clipSkip);
	return request;
}

class ofxGgmlStableDiffusionCreativeWorkflow {
public:
	ofxGgmlStableDiffusionCreativeWorkflow();

	void setGenerator(ofxGgmlStableDiffusion* generator);
	ofxGgmlStableDiffusion* getGenerator() const;

	bool start(
		const ofxGgmlStableDiffusionCreativeWorkflowSettings& settings,
		ofxGgmlStableDiffusion& generator);
	void stop();
	bool isActive() const;

	bool submitImagePreview(const ofxGgmlStableDiffusionRealtimeRequest& request);
	bool submitVideoPreview(const ofxGgmlStableDiffusionRealtimeVideoRequest& request);

	int queueImageRender(
		const ofxGgmlStableDiffusionImageRequest& request,
		ofxGgmlStableDiffusionPriority priority = ofxGgmlStableDiffusionPriority::High,
		const std::string& tag = "");
	int queueImageRenderFromPreview();
	int queueVideoRender(
		const ofxGgmlStableDiffusionVideoRequest& request,
		ofxGgmlStableDiffusionPriority priority = ofxGgmlStableDiffusionPriority::High,
		const std::string& tag = "");

	void update();

	ofxGgmlStableDiffusionQueue& getQueue();
	const ofxGgmlStableDiffusionQueue& getQueue() const;
	ofxGgmlStableDiffusionRealtimeSession& getImagePreviewSession();
	const ofxGgmlStableDiffusionRealtimeSession& getImagePreviewSession() const;
	ofxGgmlStableDiffusionRealtimeVideoSession& getVideoPreviewSession();
	const ofxGgmlStableDiffusionRealtimeVideoSession& getVideoPreviewSession() const;

	ofxGgmlStableDiffusionCreativeWorkflowSnapshot getSnapshot() const;
	bool saveSession(const std::string& path) const;
	bool loadSession(const std::string& path);

private:
	void processQueue();

	mutable std::mutex mutex_;
	ofxGgmlStableDiffusion* generator_ = nullptr;
	ofxGgmlStableDiffusionCreativeWorkflowSettings settings_;
	ofxGgmlStableDiffusionQueue queue_;
	ofxGgmlStableDiffusionRealtimeSession imagePreview_;
	ofxGgmlStableDiffusionRealtimeVideoSession videoPreview_;
	ofxGgmlStableDiffusionRealtimeRequest lastImagePreviewRequest_;
	ofxGgmlStableDiffusionRealtimeVideoRequest lastVideoPreviewRequest_;
	int activeQueueRequestId_ = -1;
	bool active_ = false;
	bool queueGenerationInFlight_ = false;
};
