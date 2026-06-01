#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"
#include "ofxImGui.h"

#include <array>
#include <atomic>
#include <string>

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	void keyPressed(int key);

private:
	void configureContext();
	void browseForModel();
	void syncRequestFromUi();
	void startGeneration();
	void cancelGeneration();
	void saveResult();
	void drawResultPreview();

	ofxGgmlStableDiffusion stableDiffusion;
	ofxImGui::Gui gui;
	ofImage resultImage;

	std::array<char, 512> modelPathInput{};
	std::array<char, 512> promptInput{};
	std::array<char, 512> negativePromptInput{};
	std::string modelPath;
	std::string prompt;
	std::string negativePrompt;
	std::string statusMessage = "Ready";
	std::string modelSummary = "No model loaded";

	int width = 512;
	int height = 512;
	int sampleSteps = 20;
	int seed = -1;
	float cfgScale = 7.0f;

	bool imGuiOk = true;
	bool contextLoading = false;
	bool generating = false;
	std::atomic<float> progress{0.0f};
};
