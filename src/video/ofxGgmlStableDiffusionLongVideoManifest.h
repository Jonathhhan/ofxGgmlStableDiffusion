#pragma once

#include "ofMain.h"
#include "../core/ofxGgmlStableDiffusionTypes.h"

#include <cstdint>
#include <string>
#include <vector>

enum class ofxGgmlStableDiffusionLongVideoPreset {
	FastPreview = 0,
	LowVram,
	Balanced,
	Quality,
	BatchStoryboard
};

struct ofxGgmlStableDiffusionLongVideoChunk {
	std::string id;
	std::string title;
	std::string prompt;
	std::string negativePrompt;
	std::string sectionGoal;
	std::string continuityNote;
	int width = 640;
	int height = 384;
	int frameCount = 49;
	int fps = 12;
	int sampleSteps = 20;
	float cfgScale = 6.5f;
	float strength = 0.65f;
	int64_t seed = -1;
	bool usePreviousLastFrame = true;
	std::string outputPrefix = "chunk";
};

struct ofxGgmlStableDiffusionLongVideoManifest {
	std::string projectName;
	std::string conceptText;
	std::string continuityBible;
	std::string outputDirectory;
	ofxGgmlStableDiffusionLongVideoPreset preset =
		ofxGgmlStableDiffusionLongVideoPreset::Balanced;
	bool lowVram = false;
	bool resumeEnabled = true;
	std::vector<ofxGgmlStableDiffusionLongVideoChunk> chunks;
};

struct ofxGgmlStableDiffusionLongVideoValidation {
	bool ok = true;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

struct ofxGgmlStableDiffusionLongVideoChunkResult {
	std::string chunkId;
	bool success = false;
	std::string error;
	std::string clipDirectory;
	std::string metadataPath;
	std::string manifestPath;
	int64_t actualSeed = -1;
	int renderedFrameCount = 0;
};

struct ofxGgmlStableDiffusionLongVideoRunResult {
	bool success = false;
	std::string error;
	std::vector<ofxGgmlStableDiffusionLongVideoChunkResult> chunks;
	std::string playlistManifestJson;
};
