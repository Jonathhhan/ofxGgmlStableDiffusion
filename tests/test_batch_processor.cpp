#include "core/ofxGgmlStableDiffusionBatchProcessor.h"
#include "ofxGgmlStableDiffusion.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

std::filesystem::path batchTestRoot() {
	return std::filesystem::temp_directory_path() / "ofxGgmlStableDiffusion-batch-test";
}

bool expect(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "FAIL: " << message << std::endl;
	}
	return condition;
}

ofxGgmlStableDiffusionResult makeImageResult(bool success, int64_t seed, unsigned char color) {
	ofxGgmlStableDiffusionResult result;
	result.success = success;
	result.actualSeedUsed = seed;
	result.selectedImageIndex = 0;
	if (!success) {
		result.error = "generation failed";
		return result;
	}

	std::vector<unsigned char> pixels = {
		color, 0, 0,
		0, color, 0,
		0, 0, color,
		color, color, color
	};
	ofxGgmlStableDiffusionImageFrame frame;
	frame.index = 0;
	frame.isSelected = true;
	frame.seed = seed;
	frame.pixels.setFromPixels(pixels.data(), 2, 2, OF_IMAGE_COLOR);
	result.images.push_back(frame);
	return result;
}

bool testProcessBatchCollectsResults() {
	bool ok = true;
	const auto outputRoot = batchTestRoot() / "process";
	const auto metadataPath = outputRoot / "metadata.json";
	ofxGgmlStableDiffusion generator;
	generator.setQueuedResults({
		makeImageResult(true, 11, 64),
		makeImageResult(false, 12, 0)
	});

	ofxGgmlStableDiffusionBatchProcessor processor;
	processor.setGenerator(&generator);

	std::vector<ofxGgmlStableDiffusionImageRequest> requests(2);
	requests[0].prompt = "first";
	requests[1].prompt = "second";

	const auto result = processor.processBatch(
		requests,
		outputRoot.string());

	ok &= expect(result.results.size() == 2, "processBatch returns both results");
	ok &= expect(result.successCount == 1, "processBatch counts successes");
	ok &= expect(result.failureCount == 1, "processBatch counts failures");
	ok &= expect(generator.getImageRequests().size() == 2, "processBatch invoked generator twice");
	ok &= expect(
		result.exportMetadata(metadataPath.string()),
		"processBatch exports metadata");
	ok &= expect(
		std::filesystem::exists(metadataPath),
		"metadata file exists");
	return ok;
}

bool testParameterSweepTracksBestScore() {
	bool ok = true;
	ofxGgmlStableDiffusion generator;
	generator.setQueuedResults({
		makeImageResult(true, 10, 32),
		makeImageResult(true, 30, 96),
		makeImageResult(true, 20, 64)
	});

	ofxGgmlStableDiffusionBatchProcessor processor;
	processor.setGenerator(&generator);
	processor.setQualityScoringFunction([](const ofxGgmlStableDiffusionResult& result) {
		return static_cast<float>(result.actualSeedUsed);
	});

	ofxGgmlStableDiffusionSweepSettings settings;
	settings.baseRequest.prompt = "sweep";
	settings.parameter = ofxGgmlStableDiffusionParameter::CfgScale;
	settings.rangeMin = 1.0f;
	settings.rangeMax = 3.0f;
	settings.steps = 3;

	const auto sweep = processor.parameterSweep(settings);

	ok &= expect(sweep.results.size() == 3, "parameterSweep returns all entries");
	ok &= expect(sweep.bestIndex == 1, "parameterSweep selects the best score");
	ok &= expect(sweep.bestValue == 2.0f, "parameterSweep tracks the best parameter value");
	return ok;
}

bool testGenerateGridAppliesParametersAndComparisonExports() {
	bool ok = true;
	const auto outputRoot = batchTestRoot() / "grid";
	const auto gridPath = outputRoot / "grid.png";
	const auto comparisonPath = outputRoot / "comparison.png";
	ofxGgmlStableDiffusion generator;
	generator.setQueuedResults({
		makeImageResult(true, 1, 80),
		makeImageResult(true, 2, 120),
		makeImageResult(true, 3, 160),
		makeImageResult(true, 4, 200)
	});

	ofxGgmlStableDiffusionBatchProcessor processor;
	processor.setGenerator(&generator);

	ofxGgmlStableDiffusionGridSettings settings;
	settings.baseRequest.prompt = "grid";
	settings.baseRequest.sampleSteps = 8;
	settings.xAxis = ofxGgmlStableDiffusionParameter::CfgScale;
	settings.xValues = {2.5f, 4.0f};
	settings.yAxis = ofxGgmlStableDiffusionParameter::SampleSteps;
	settings.yValues = {16.0f};
	settings.outputPath = outputRoot.string();

	const auto grid = processor.generateGrid(settings);

	ok &= expect(grid.successCount == 2, "generateGrid runs all cells");
	ok &= expect(generator.getImageRequests().size() >= 2, "generateGrid submitted requests");
	ok &= expect(generator.getImageRequests()[0].cfgScale == 2.5f, "grid x-axis applied");
	ok &= expect(generator.getImageRequests()[0].sampleSteps == 16, "grid y-axis applied");
	ok &= expect(
		std::filesystem::exists(gridPath),
		"generateGrid exports a composed grid image");

	const auto comparison = processor.compareAB(
		generator.getImageRequests()[0],
		generator.getImageRequests()[1]);
	ok &= expect(
		comparison.exportComparison(comparisonPath.string()),
		"compareAB exports a side-by-side image");
	ok &= expect(
		std::filesystem::exists(comparisonPath),
		"comparison image exists");
	return ok;
}

} // namespace

int main() {
	std::filesystem::remove_all(batchTestRoot());
	bool ok = true;
	ok &= testProcessBatchCollectsResults();
	ok &= testParameterSweepTracksBestScore();
	ok &= testGenerateGridAppliesParametersAndComparisonExports();
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
