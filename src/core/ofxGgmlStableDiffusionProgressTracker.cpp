#include "ofxGgmlStableDiffusionProgressTracker.h"
#include "ofMain.h"
#include <algorithm>
#include <numeric>

//--------------------------------------------------------------
ofxGgmlStableDiffusionProgressTracker::ofxGgmlStableDiffusionProgressTracker() {
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionProgressTracker::reset(int totalSteps, int totalBatches) {
	currentInfo = ofxGgmlStableDiffusionProgressInfo();
	currentInfo.totalSteps = totalSteps;
	currentInfo.totalBatches = totalBatches;
	currentInfo.currentBatch = 1;

	stepTimes.clear();
	startTimeMicros = ofGetElapsedTimeMicros();
	lastUpdateMicros = startTimeMicros;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionProgressTracker::update(int currentStep, int currentBatch, float elapsedSeconds) {
	currentInfo.currentStep = currentStep;
	currentInfo.currentBatch = currentBatch;
	currentInfo.elapsedTimeSeconds = elapsedSeconds;

	const uint64_t nowMicros = ofGetElapsedTimeMicros();

	// Calculate step time
	if (currentStep > 0 && lastUpdateMicros > 0) {
		const float stepTime = (nowMicros - lastUpdateMicros) / 1000000.0f;
		stepTimes.push_back(stepTime);

		// Keep only recent history
		if (stepTimes.size() > maxHistorySize) {
			stepTimes.pop_front();
		}
	}

	lastUpdateMicros = nowMicros;

	// Calculate performance metrics
	if (currentStep > 0 && elapsedSeconds > 0.0f) {
		currentInfo.stepsPerSecond = currentStep / elapsedSeconds;
		currentInfo.avgStepTimeSeconds = elapsedSeconds / currentStep;
	}

	// Calculate progress percentage
	if (currentInfo.totalSteps > 0) {
		float stepProgress = static_cast<float>(currentStep) / currentInfo.totalSteps;
		float batchProgress = currentInfo.totalBatches > 1 ?
			static_cast<float>(currentBatch - 1) / currentInfo.totalBatches : 0.0f;

		currentInfo.percentComplete = (batchProgress + stepProgress / currentInfo.totalBatches) * 100.0f;
	}

	// Calculate ETA
	calculateETA();
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionProgressTracker::setPhase(ofxGgmlStableDiffusionPhase phase, const std::string& description) {
	currentInfo.phase = phase;
	currentInfo.phaseDescription = description.empty() ? currentInfo.getPhaseLabel() : description;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionProgressTracker::setMemoryUsage(uint64_t used, uint64_t total) {
	currentInfo.memoryUsedBytes = used;
	currentInfo.memoryTotalBytes = total;

	if (total > 0) {
		currentInfo.memoryUsagePercent = (static_cast<float>(used) / total) * 100.0f;
	}
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionProgressInfo ofxGgmlStableDiffusionProgressTracker::getProgressInfo() const {
	return currentInfo;
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionProgressTracker::hasValidETA() const {
	return currentInfo.hasValidETA;
}

//--------------------------------------------------------------
float ofxGgmlStableDiffusionProgressTracker::getEstimatedTimeRemaining() const {
	return currentInfo.estimatedTimeRemainingSeconds;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionProgressTracker::calculateETA() {
	// Need at least 3 samples for reasonable ETA
	if (stepTimes.size() < 3) {
		currentInfo.hasValidETA = false;
		currentInfo.estimatedTimeRemainingSeconds = 0.0f;
		currentInfo.estimatedTotalTimeSeconds = 0.0f;
		return;
	}

	// Calculate average step time from recent history
	float avgStepTime = getAverageStepTime();

	if (avgStepTime <= 0.0f) {
		currentInfo.hasValidETA = false;
		return;
	}

	// Calculate remaining steps (including batches)
	int remainingStepsInCurrentBatch = currentInfo.totalSteps - currentInfo.currentStep;
	int remainingBatches = currentInfo.totalBatches - currentInfo.currentBatch;
	int totalRemainingSteps = remainingStepsInCurrentBatch + (remainingBatches * currentInfo.totalSteps);

	// Estimate remaining time
	currentInfo.estimatedTimeRemainingSeconds = totalRemainingSteps * avgStepTime;
	currentInfo.estimatedTotalTimeSeconds = currentInfo.elapsedTimeSeconds + currentInfo.estimatedTimeRemainingSeconds;
	currentInfo.hasValidETA = true;

	// Add phase-specific overhead estimates
	switch (currentInfo.phase) {
		case ofxGgmlStableDiffusionPhase::Encoding:
			// Encoding is usually quick, add minimal overhead
			currentInfo.estimatedTimeRemainingSeconds += 2.0f;
			break;
		case ofxGgmlStableDiffusionPhase::Diffusing:
			// Main computation phase, ETA is most accurate here
			break;
		case ofxGgmlStableDiffusionPhase::Decoding:
			// Decoding takes some time
			currentInfo.estimatedTimeRemainingSeconds += 5.0f;
			break;
		case ofxGgmlStableDiffusionPhase::Upscaling:
			// Upscaling can be slow
			currentInfo.estimatedTimeRemainingSeconds += 10.0f;
			break;
		default:
			break;
	}

	// Status message
	currentInfo.statusMessage = currentInfo.getPhaseLabel() + " - " +
		std::to_string(currentInfo.currentStep) + "/" + std::to_string(currentInfo.totalSteps) +
		" steps - ETA: " + currentInfo.getFormattedETA();
}

//--------------------------------------------------------------
float ofxGgmlStableDiffusionProgressTracker::getAverageStepTime() const {
	if (stepTimes.empty()) {
		return 0.0f;
	}

	// Use exponentially weighted moving average to give more weight to recent samples
	float weightedSum = 0.0f;
	float weightSum = 0.0f;
	float alpha = 0.7f;  // Decay factor

	for (std::size_t i = 0; i < stepTimes.size(); ++i) {
		float weight = std::pow(alpha, stepTimes.size() - i - 1);
		weightedSum += stepTimes[i] * weight;
		weightSum += weight;
	}

	return weightSum > 0.0f ? weightedSum / weightSum : 0.0f;
}
