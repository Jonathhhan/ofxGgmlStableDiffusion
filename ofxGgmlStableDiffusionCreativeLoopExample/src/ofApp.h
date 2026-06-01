#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"
#include "ofxImGui.h"

#include <array>
#include <string>

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	void keyPressed(int key);

private:
	void syncRequestFromUi();
	void configureContext();
	ofxGgmlStableDiffusionRealtimeVideoSettings buildLoopSettings() const;
	ofxGgmlStableDiffusionRealtimeVideoRequest buildLoopRequest() const;
	void startLoop();
	void stopLoop();
	void submitPrompt();
	void clearFeedback();
	void drawPreview();

	ofxGgmlStableDiffusion sd;
	ofxGgmlStableDiffusionRealtimeVideoSession loop;
	ofxImGui::Gui gui;
	ofImage previewImage;
	std::array<char, 512> promptInput{};
	std::array<char, 512> negativePromptInput{};
	std::string prompt;
	std::string negativePrompt;
	std::string statusMessage;
	std::string modelSummary;
	int width = 512;
	int height = 512;
	int previewSteps = 4;
	int refineSteps = 16;
	int seed = -1;
	float cfgScale = 1.5f;
	float previewStrength = 0.68f;
	float refineStrength = 0.35f;
	float lastLatencyMs = 0.0f;
	int lastFrameIndex = -1;
	bool lockSeed = false;
	bool useFeedback = true;
	bool refineOnIdle = true;
	bool dropIfBusy = false;
	bool imGuiOk = true;
	bool loopRunning = false;
	bool contextLoading = false;
	ofxGgmlStableDiffusionRealtimeVideoQuality lastQuality =
		ofxGgmlStableDiffusionRealtimeVideoQuality::Preview;
};
