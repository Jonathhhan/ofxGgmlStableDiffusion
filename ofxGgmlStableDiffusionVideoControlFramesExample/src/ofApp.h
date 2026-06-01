#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"
#include "ofxImGui.h"

#include <array>
#include <atomic>
#include <string>
#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	void keyPressed(int key);

private:
	void syncRequestFromUi();
	void configureContext();
	void loadControlFrames();
	void clearControlFrames();
	void startGeneration();
	void cancelGeneration();
	void saveFrames();
	void saveVideo();
	void drawFramePreview();
	sd_cache_mode_t selectedCacheMode() const;
	void rebuildControlFrameViews();

	ofxGgmlStableDiffusion sd;
	ofxImGui::Gui gui;
	ofImage framePreview;
	std::vector<ofImage> controlImagePreviews;
	std::vector<ofPixels> controlPixels;
	std::vector<sd_image_t> controlFrames;

	std::array<char, 512> promptInput{};
	std::array<char, 512> negativePromptInput{};
	std::array<char, 512> modelPathInput{};
	std::array<char, 512> controlFrameDirInput{};

	std::string prompt;
	std::string negativePrompt;
	std::string modelPath;
	std::string controlFrameDir;
	std::string statusMessage;
	std::string modelSummary;

	int width = 832;
	int height = 480;
	int frameCount = 6;
	int fps = 6;
	int sampleSteps = 12;
	int highNoiseSampleSteps = -1;
	int seed = -1;
	int currentFrame = 0;
	int cacheModeIndex = 0;
	float cfgScale = 5.0f;
	float guidance = 5.0f;
	float eta = 0.0f;
	float flowShift = 5.0f;
	float moeBoundary = 0.875f;
	float vaceStrength = 1.0f;
	float highNoiseCfgScale = 5.0f;
	float highNoiseGuidance = 5.0f;
	float highNoiseEta = 0.0f;
	float highNoiseFlowShift = 5.0f;
	float cacheThreshold = 0.2f;
	float cacheStartPercent = 0.1f;
	float cacheEndPercent = 0.9f;
	bool useHighNoiseOverrides = false;
	bool imGuiOk = true;
	bool generating = false;
	bool contextLoading = false;
	std::atomic<float> progress{0.0f};
};
