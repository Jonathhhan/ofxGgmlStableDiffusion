#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusionTypes.h"
#include <functional>
#include <map>
#include <vector>

class ofxGgmlStableDiffusion;

/// Parameter types for batch processing
enum class ofxGgmlStableDiffusionParameter {
	CfgScale,
	SampleSteps,
	Strength,
	Seed,
	Width,
	Height,
	SamplerMethod,
	Schedule,
	BatchCount
};

/// Step mode for parameter sweeps
enum class ofxGgmlStableDiffusionStepMode {
	Linear,       // Equal steps
	Logarithmic   // Logarithmic steps
};

/// Result from batch processing
struct ofxGgmlStableDiffusionBatchResult {
	std::vector<ofxGgmlStableDiffusionResult> results;
	std::map<std::string, std::string> metadata;
	float totalTimeSeconds = 0.0f;
	int successCount = 0;
	int failureCount = 0;

	/// Export metadata to JSON file
	/// @param path Output file path
	/// @return True if successful
	bool exportMetadata(const std::string& path) const;
};

/// Settings for X/Y/Z grid generation
struct ofxGgmlStableDiffusionGridSettings {
	ofxGgmlStableDiffusionImageRequest baseRequest;
	ofxGgmlStableDiffusionParameter xAxis;
	std::vector<float> xValues;
	ofxGgmlStableDiffusionParameter yAxis;
	std::vector<float> yValues;
	std::string outputPath;
	int gridCellWidth = 512;
	int gridCellHeight = 512;
	bool addLabels = true;
};

/// Settings for parameter sweeps
struct ofxGgmlStableDiffusionSweepSettings {
	ofxGgmlStableDiffusionImageRequest baseRequest;
	ofxGgmlStableDiffusionParameter parameter;
	float rangeMin = 0.0f;
	float rangeMax = 1.0f;
	int steps = 10;
	ofxGgmlStableDiffusionStepMode stepMode = ofxGgmlStableDiffusionStepMode::Linear;
};

/// Result from parameter sweep
struct ofxGgmlStableDiffusionSweepResult {
	struct Entry {
		float parameterValue;
		ofxGgmlStableDiffusionResult result;
		float qualityScore = 0.0f;
	};

	std::vector<Entry> results;
	ofxGgmlStableDiffusionParameter parameter;
	float bestValue = 0.0f;
	int bestIndex = -1;
};

/// Result from A/B comparison
struct ofxGgmlStableDiffusionComparisonResult {
	ofxGgmlStableDiffusionResult resultA;
	ofxGgmlStableDiffusionResult resultB;
	std::string nameA;
	std::string nameB;
	float scoreA = 0.0f;
	float scoreB = 0.0f;

	/// Export side-by-side comparison image
	/// @param path Output file path
	/// @return True if successful
	bool exportComparison(const std::string& path) const;
};

/// Experimental batch-processing scaffold for systematic parameter exploration.
///
/// The request/result structs, parameter helpers, and metadata export are available
/// today, but the generation methods below do not run native image generation yet.
/// They currently return placeholder/empty results while logging a warning.
class ofxGgmlStableDiffusionBatchProcessor {
public:
	ofxGgmlStableDiffusionBatchProcessor();
	~ofxGgmlStableDiffusionBatchProcessor();

	/// Attach the generator used to execute batch requests.
	/// @param sd Generator instance to use (not owned)
	void setGenerator(ofxGgmlStableDiffusion* sd);

	/// Get the attached generator.
	ofxGgmlStableDiffusion* getGenerator() const;

	/// Check whether a generator is attached.
	bool hasGenerator() const;

	/// Generate X/Y parameter grid
	/// @param settings Grid generation settings
	/// @return Batch result containing one entry per grid cell.
	ofxGgmlStableDiffusionBatchResult generateGrid(const ofxGgmlStableDiffusionGridSettings& settings);

	/// Perform parameter sweep
	/// @param settings Sweep settings
	/// @return Sweep result ordered by sweep value.
	ofxGgmlStableDiffusionSweepResult parameterSweep(const ofxGgmlStableDiffusionSweepSettings& settings);

	/// Compare two requests side-by-side
	/// @param requestA First request
	/// @param requestB Second request
	/// @return Comparison result with generated outputs and quality scores.
	ofxGgmlStableDiffusionComparisonResult compareAB(
		const ofxGgmlStableDiffusionImageRequest& requestA,
		const ofxGgmlStableDiffusionImageRequest& requestB);

	/// Process multiple requests in batch
	/// @param requests Vector of requests to process
	/// @param outputDirectory Directory for output files
	/// @return Batch result containing every generated request result.
	ofxGgmlStableDiffusionBatchResult processBatch(
		const std::vector<ofxGgmlStableDiffusionImageRequest>& requests,
		const std::string& outputDirectory = "");

	/// Set progress callback for batch operations
	/// @param callback Progress callback function
	void setProgressCallback(std::function<void(int current, int total, const std::string& status)> callback);

	/// Set quality scoring function for ranking results
	/// @param scoreFunc Function that scores a result (higher is better)
	void setQualityScoringFunction(std::function<float(const ofxGgmlStableDiffusionResult&)> scoreFunc);

	/// Cancel current batch operation
	void cancel();

	/// Check if batch operation is running
	/// @return True if running
	bool isRunning() const;

	/// Set how often blocking batch waits poll the async generator.
	void setPollIntervalMs(int pollIntervalMs);
	int getPollIntervalMs() const;

	/// Set the maximum wait time for one batch request before it is marked failed.
	void setExecutionTimeoutMs(int timeoutMs);
	int getExecutionTimeoutMs() const;

private:
	bool runImageRequest(
		const ofxGgmlStableDiffusionImageRequest& request,
		ofxGgmlStableDiffusionResult& result,
		std::string& errorMessage);
	bool waitForCurrentGeneration(
		ofxGgmlStableDiffusionResult& result,
		std::string& errorMessage);
	float scoreResult(const ofxGgmlStableDiffusionResult& result) const;
	void recordBatchEntry(
		ofxGgmlStableDiffusionBatchResult& batchResult,
		const ofxGgmlStableDiffusionResult& result,
		int entryIndex,
		const std::map<std::string, std::string>& entryMetadata);
	bool exportResultImage(
		const ofxGgmlStableDiffusionResult& result,
		const std::string& outputPath) const;

	void applyParameterValue(
		ofxGgmlStableDiffusionImageRequest& request,
		ofxGgmlStableDiffusionParameter param,
		float value);

	float getParameterValue(
		const ofxGgmlStableDiffusionImageRequest& request,
		ofxGgmlStableDiffusionParameter param) const;

	std::string getParameterName(ofxGgmlStableDiffusionParameter param) const;

	std::vector<float> generateStepValues(
		float minVal,
		float maxVal,
		int steps,
		ofxGgmlStableDiffusionStepMode mode) const;

	ofImage createGridImage(
		const std::vector<std::vector<ofxGgmlStableDiffusionResult>>& grid,
		const ofxGgmlStableDiffusionGridSettings& settings) const;

	std::function<void(int, int, const std::string&)> progressCallback;
	std::function<float(const ofxGgmlStableDiffusionResult&)> qualityScoreFunc;
	ofxGgmlStableDiffusion* generator = nullptr;
	bool running = false;
	bool cancelRequested = false;
	int pollIntervalMs = 10;
	int executionTimeoutMs = 600000;
};
