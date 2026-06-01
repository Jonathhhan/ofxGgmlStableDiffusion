#include "ofxGgmlStableDiffusionRealtimeVideoSession.h"
#include "../ofxGgmlStableDiffusion.h"
#include <algorithm>
#include <utility>

bool ofxGgmlStableDiffusionRealtimeVideoSession::start(
	const ofxGgmlStableDiffusionRealtimeVideoSettings & settings) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (active_) {
		ofLogWarning("ofxGgmlStableDiffusionRealtimeVideoSession")
			<< "Session already active";
		return false;
	}
	settings_ = settings;
	settings_.previewWidth = std::max(64, settings_.previewWidth);
	settings_.previewHeight = std::max(64, settings_.previewHeight);
	settings_.previewSteps = std::max(1, settings_.previewSteps);
	settings_.refineSteps = std::max(settings_.previewSteps, settings_.refineSteps);
	stats_ = {};
	stats_.isActive = true;
	pendingRequest_ = {};
	activeRequest_ = {};
	lastFrame_ = {};
	active_ = true;
	generationInFlight_ = false;
	hasPendingRequest_ = false;
	hasFeedbackFrame_ = false;
	refinedActiveRequest_ = false;
	lastPromptChangeMicros_ = ofGetElapsedTimeMicros();
	return true;
}

bool ofxGgmlStableDiffusionRealtimeVideoSession::start(
	const ofxGgmlStableDiffusionRealtimeVideoSettings & settings,
	ofxGgmlStableDiffusion & sd) {
	setGenerator(&sd);
	return start(settings);
}

void ofxGgmlStableDiffusionRealtimeVideoSession::stop() {
	ofxGgmlStableDiffusion * generator = nullptr;
	bool generationInFlight = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		generator = generator_;
		generationInFlight = generationInFlight_;
		active_ = false;
		stats_.isActive = false;
		generationInFlight_ = false;
		hasPendingRequest_ = false;
		refinedActiveRequest_ = false;
	}
	if (generationInFlight && generator != nullptr) {
		generator->requestCancellation();
	}
}

bool ofxGgmlStableDiffusionRealtimeVideoSession::isActive() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return active_;
}

void ofxGgmlStableDiffusionRealtimeVideoSession::setGenerator(ofxGgmlStableDiffusion * sd) {
	std::lock_guard<std::mutex> lock(mutex_);
	generator_ = sd;
}

bool ofxGgmlStableDiffusionRealtimeVideoSession::submit(
	const ofxGgmlStableDiffusionRealtimeVideoRequest & request) {
	bool shouldProcess = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!active_ || generator_ == nullptr) {
			stats_.droppedUpdates++;
			return false;
		}
		if (generationInFlight_ && settings_.dropIfBusy) {
			stats_.droppedUpdates++;
			return false;
		}
		if (hasPendingRequest_ && settings_.coalescePromptUpdates) {
			stats_.coalescedUpdates++;
		}
		pendingRequest_ = request;
		hasPendingRequest_ = true;
		refinedActiveRequest_ = false;
		stats_.promptUpdates++;
		lastPromptChangeMicros_ = ofGetElapsedTimeMicros();
		shouldProcess = !generationInFlight_;
	}
	if (shouldProcess) {
		processNext();
	}
	return true;
}

void ofxGgmlStableDiffusionRealtimeVideoSession::updatePrompt(const std::string & prompt) {
	ofxGgmlStableDiffusionRealtimeVideoRequest request;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		request = hasPendingRequest_ ? pendingRequest_ : activeRequest_;
	}
	request.prompt = prompt;
	(void)submit(request);
}

void ofxGgmlStableDiffusionRealtimeVideoSession::updateNegativePrompt(
	const std::string & negativePrompt) {
	ofxGgmlStableDiffusionRealtimeVideoRequest request;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		request = hasPendingRequest_ ? pendingRequest_ : activeRequest_;
	}
	request.negativePrompt = negativePrompt;
	(void)submit(request);
}

void ofxGgmlStableDiffusionRealtimeVideoSession::update() {
	ofxGgmlStableDiffusion * generator = nullptr;
	bool generationInFlight = false;
	uint64_t generationStartMicros = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!active_ || generator_ == nullptr) {
			return;
		}
		generator = generator_;
		generationInFlight = generationInFlight_;
		generationStartMicros = generationStartMicros_;
	}

	if (generationInFlight) {
		if (generator->isGenerating()) {
			return;
		}

		ofxGgmlStableDiffusionRealtimeVideoFrame callbackFrame;
		ofxSdRealtimeVideoFrameCallback frameCallback;
		ofxSdRealtimeVideoLatencyCallback latencyCallback;
		ofxGgmlStableDiffusionRealtimeVideoQuality completedQuality =
			ofxGgmlStableDiffusionRealtimeVideoQuality::Preview;
		const float latencyMs =
			static_cast<float>(ofGetElapsedTimeMicros() - generationStartMicros) / 1000.0f;
		const ofxGgmlStableDiffusionResult result = generator->getLastResult();
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!generationInFlight_) {
				return;
			}
			generationInFlight_ = false;
			completedQuality = activeQuality_;
			updateStats(latencyMs, completedQuality);
			lastFrame_.result = result;
			lastFrame_.quality = completedQuality;
			lastFrame_.prompt = activeRequest_.prompt;
			lastFrame_.latencyMs = latencyMs;
			lastFrame_.frameIndex = stats_.framesGenerated - 1;
			if (result.success && !result.images.empty()) {
				int selected = result.selectedImageIndex;
				if (selected < 0 || selected >= static_cast<int>(result.images.size())) {
					selected = 0;
				}
				lastFrame_.frame = result.images[static_cast<std::size_t>(selected)];
				hasFeedbackFrame_ = lastFrame_.frame.pixels.isAllocated();
			}
			refinedActiveRequest_ =
				completedQuality == ofxGgmlStableDiffusionRealtimeVideoQuality::Refine;
			callbackFrame = lastFrame_;
			frameCallback = frameCallback_;
			latencyCallback = latencyCallback_;
		}
		if (latencyCallback) {
			latencyCallback(latencyMs, completedQuality);
		}
		if (frameCallback && result.success) {
			frameCallback(callbackFrame);
		}
	}

	processNext();
}

bool ofxGgmlStableDiffusionRealtimeVideoSession::isGenerating() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return generationInFlight_;
}

bool ofxGgmlStableDiffusionRealtimeVideoSession::hasPendingRequest() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return hasPendingRequest_;
}

void ofxGgmlStableDiffusionRealtimeVideoSession::clearFeedbackFrame() {
	std::lock_guard<std::mutex> lock(mutex_);
	lastFrame_.frame = {};
	hasFeedbackFrame_ = false;
}

ofxGgmlStableDiffusionRealtimeVideoStats
ofxGgmlStableDiffusionRealtimeVideoSession::getStats() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return stats_;
}

ofxGgmlStableDiffusionRealtimeVideoFrame
ofxGgmlStableDiffusionRealtimeVideoSession::getLastFrame() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return lastFrame_;
}

ofxGgmlStableDiffusionRealtimeVideoSettings
ofxGgmlStableDiffusionRealtimeVideoSession::getSettings() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return settings_;
}

void ofxGgmlStableDiffusionRealtimeVideoSession::setFrameCallback(
	ofxSdRealtimeVideoFrameCallback callback) {
	std::lock_guard<std::mutex> lock(mutex_);
	frameCallback_ = std::move(callback);
}

void ofxGgmlStableDiffusionRealtimeVideoSession::setLatencyCallback(
	ofxSdRealtimeVideoLatencyCallback callback) {
	std::lock_guard<std::mutex> lock(mutex_);
	latencyCallback_ = std::move(callback);
}

bool ofxGgmlStableDiffusionRealtimeVideoSession::shouldStartRefine(uint64_t nowMicros) const {
	if (!settings_.enableRefineOnIdle || refinedActiveRequest_ ||
		activeRequest_.prompt.empty() || !hasFeedbackFrame_) {
		return false;
	}
	const uint64_t stableMicros = settings_.refineAfterStableMs * 1000ULL;
	return nowMicros >= lastPromptChangeMicros_ + stableMicros;
}

void ofxGgmlStableDiffusionRealtimeVideoSession::processNext() {
	ofxGgmlStableDiffusion * generator = nullptr;
	ofxGgmlStableDiffusionRealtimeVideoRequest request;
	ofxGgmlStableDiffusionRealtimeVideoSettings settings;
	ofxGgmlStableDiffusionImageFrame feedbackFrame;
	ofxGgmlStableDiffusionRealtimeVideoQuality quality =
		ofxGgmlStableDiffusionRealtimeVideoQuality::Preview;
	bool useFeedback = false;

	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!active_ || generator_ == nullptr || generationInFlight_ ||
			generator_->isGenerating()) {
			return;
		}
		if (hasPendingRequest_) {
			request = pendingRequest_;
			activeRequest_ = request;
			hasPendingRequest_ = false;
			refinedActiveRequest_ = false;
			quality = ofxGgmlStableDiffusionRealtimeVideoQuality::Preview;
		} else if (shouldStartRefine(ofGetElapsedTimeMicros())) {
			request = activeRequest_;
			quality = ofxGgmlStableDiffusionRealtimeVideoQuality::Refine;
		} else {
			return;
		}
		generator = generator_;
		settings = settings_;
		if (hasFeedbackFrame_) {
			feedbackFrame = lastFrame_.frame;
			useFeedback = true;
		}
		activeQuality_ = quality;
		generationInFlight_ = true;
		generationStartMicros_ = ofGetElapsedTimeMicros();
	}

	const ofxGgmlStableDiffusionImageRequest imageRequest =
		ofxGgmlStableDiffusionBuildRealtimeVideoImageRequest(
			settings,
			request,
			quality,
			useFeedback ? &feedbackFrame : nullptr);
	generator->generate(imageRequest);
}

void ofxGgmlStableDiffusionRealtimeVideoSession::updateStats(
	float latencyMs,
	ofxGgmlStableDiffusionRealtimeVideoQuality quality) {
	stats_.framesGenerated++;
	stats_.lastLatencyMs = latencyMs;
	stats_.lastQuality = quality;
	if (quality == ofxGgmlStableDiffusionRealtimeVideoQuality::Refine) {
		stats_.refineFrames++;
	} else {
		stats_.previewFrames++;
	}
	if (stats_.framesGenerated <= 1) {
		stats_.averageLatencyMs = latencyMs;
	} else {
		const float previousTotal =
			stats_.averageLatencyMs * static_cast<float>(stats_.framesGenerated - 1);
		stats_.averageLatencyMs =
			(previousTotal + latencyMs) / static_cast<float>(stats_.framesGenerated);
	}
}
