#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"
#include "ofxImGui.h"

#include <atomic>
#include <string>

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
	void saveLoadedModelPath();
	void loadRuntimePreferences();
	void saveRuntimePreferences() const;
	void configureContext();
	void browseForModel();
	void startGeneration();
	void cancelGeneration();
	void saveResult();
	void drawResultPreview();

	ofxGgmlStableDiffusion stableDiffusion;
	ofxImGui::Gui gui;
	ofImage resultImage;

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
	std::string maxVram;
	std::string splitMode;
	bool streamLayers = false;
	bool eagerLoad = false;
	bool autoFit = false;

	bool imGuiOk = true;
	bool contextLoading = false;
	bool generating = false;
	std::atomic<float> progress{0.0f};
};
