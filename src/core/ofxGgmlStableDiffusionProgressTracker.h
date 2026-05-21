#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

/// Generation phase enum
enum class ofxGgmlStableDiffusionPhase {
	Idle = 0,
	LoadingModel,
	Encoding,
	Diffusing,
	Decoding,
	Upscaling,
	Finalizing
};

/// Advanced progress information with ETA
struct ofxGgmlStableDiffusionProgressInfo {
	// Basic progress
	int currentStep = 0;
	int totalSteps = 0;
	float elapsedTimeSeconds = 0.0f;

	// Current phase
	ofxGgmlStableDiffusionPhase phase = ofxGgmlStableDiffusionPhase::Idle;
	std::string phaseDescription;

	// ETA calculation
	float estimatedTimeRemainingSeconds = 0.0f;
	float estimatedTotalTimeSeconds = 0.0f;
	float percentComplete = 0.0f;

	// Performance metrics
	float stepsPerSecond = 0.0f;
	float avgStepTimeSeconds = 0.0f;

	// Memory usage (if available)
	uint64_t memoryUsedBytes = 0;
	uint64_t memoryTotalBytes = 0;
	float memoryUsagePercent = 0.0f;

	// Batch progress
	int currentBatch = 0;
	int totalBatches = 1;

	// Additional info
	bool hasValidETA = false;
	std::string statusMessage;

	// Helper methods
	bool isComplete() const {
		return currentStep >= totalSteps && phase != ofxGgmlStableDiffusionPhase::Idle;
	}

	float getProgress() const {
		if (totalSteps == 0) return 0.0f;
		return static_cast<float>(currentStep) / totalSteps;
	}

	std::string getPhaseLabel() const {
		switch (phase) {
			case ofxGgmlStableDiffusionPhase::Idle: return "Idle";
			case ofxGgmlStableDiffusionPhase::LoadingModel: return "Loading Model";
			case ofxGgmlStableDiffusionPhase::Encoding: return "Encoding";
			case ofxGgmlStableDiffusionPhase::Diffusing: return "Diffusing";
			case ofxGgmlStableDiffusionPhase::Decoding: return "Decoding";
			case ofxGgmlStableDiffusionPhase::Upscaling: return "Upscaling";
			case ofxGgmlStableDiffusionPhase::Finalizing: return "Finalizing";
			default: return "Unknown";
		}
	}

	std::string getFormattedETA() const {
		if (!hasValidETA) return "Calculating...";

		int remainingSeconds = static_cast<int>(estimatedTimeRemainingSeconds);
		if (remainingSeconds < 60) {
			return std::to_string(remainingSeconds) + "s";
		} else if (remainingSeconds < 3600) {
			int mins = remainingSeconds / 60;
			int secs = remainingSeconds % 60;
			return std::to_string(mins) + "m " + std::to_string(secs) + "s";
		} else {
			int hours = remainingSeconds / 3600;
			int mins = (remainingSeconds % 3600) / 60;
			return std::to_string(hours) + "h " + std::to_string(mins) + "m";
		}
	}
};

/// Progress tracker for ETA calculation
class ofxGgmlStableDiffusionProgressTracker {
public:
	ofxGgmlStableDiffusionProgressTracker();

	/// Reset tracker for a new generation
	void reset(int totalSteps, int totalBatches = 1);

	/// Update progress
	void update(int currentStep, int currentBatch, float elapsedSeconds);

	/// Set current phase
	void setPhase(ofxGgmlStableDiffusionPhase phase, const std::string& description = "");

	/// Set memory usage
	void setMemoryUsage(uint64_t used, uint64_t total);

	/// Get current progress info
	ofxGgmlStableDiffusionProgressInfo getProgressInfo() const;

	/// Check if ETA is available
	bool hasValidETA() const;

	/// Get estimated time remaining
	float getEstimatedTimeRemaining() const;

private:
	ofxGgmlStableDiffusionProgressInfo currentInfo;

	// Historical data for ETA calculation
	// std::deque is used instead of std::vector so pop_front() is O(1) when
	// the history window is full, rather than O(n) with erase(begin()).
	std::deque<float> stepTimes;
	static constexpr std::size_t maxHistorySize = 20;

	// Timing
	uint64_t startTimeMicros = 0;
	uint64_t lastUpdateMicros = 0;

	// Internal methods
	void calculateETA();
	float getAverageStepTime() const;
};
