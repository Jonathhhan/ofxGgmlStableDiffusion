#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"
#include "ofxImGui.h"

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
	std::string getLastModelPathFile() const;
	std::string getRuntimePreferencesFile() const;
	std::string findInitialModelPath() const;
	void refreshModelList();
	std::string memoryPolicySummary() const;
	void saveLoadedModelPath();
	void loadRuntimePreferences();
	void saveRuntimePreferences() const;
	void configureContext();
	void browseForModel();
	void startGeneration();
	void cancelGeneration();
	void saveResult();
	void drawResultPreview();
	void updateImageSmoke();
	void finishImageSmoke(int exitCode, const std::string& message);

	ofxGgmlStableDiffusion stableDiffusion;
	ofxImGui::Gui gui;
	ofImage resultImage;

	std::string modelPath;
	std::string prompt;
	std::string negativePrompt;
	std::string statusMessage = "Ready";
	std::string modelSummary = "No model loaded";
	std::vector<std::string> availableModels;
	int selectedModelIndex = -1;

	int width = 512;
	int height = 512;
	int sampleSteps = 20;
	int seed = -1;
	float cfgScale = 7.0f;
	std::string maxVram;
	std::string splitMode;
	bool streamLayers = false;
	bool eagerLoad = false;
	bool autoFit = false;

	bool imGuiOk = true;
	bool contextLoading = false;
	bool generating = false;
	bool imageSmoke = false;
	bool imageSmokeGenerationStarted = false;
	uint64_t imageSmokeStartMillis = 0;
	uint64_t imageSmokeTimeoutMillis = 120000;
	std::string imageSmokeOutputPath;
	std::atomic<float> progress{0.0f};
};
