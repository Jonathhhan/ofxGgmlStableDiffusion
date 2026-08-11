#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"
#include "ofxImGui.h"

#include <array>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	void keyPressed(int key);

private:
	void syncRequestFromUi();
	void browsePath(
		const std::string& title,
		bool selectFolder,
		std::array<char, 512>& input);
	void configureContext();
	void scanLoras();
	void reloadEmbeddings();
	void applySelectedLora();
	void applyAllLoras();
	void clearLoras();
	void startGeneration();
	void cancelGeneration();
	void saveResult();
	void drawResultPreview();
	std::vector<std::pair<std::string, std::string>> listFiles(
		const std::string& directory,
		const std::vector<std::string>& extensions) const;

	ofxGgmlStableDiffusion sd;
	ofxImGui::Gui gui;
	ofImage resultImage;

	std::array<char, 512> promptInput{};
	std::array<char, 512> negativePromptInput{};
	std::array<char, 512> modelPathInput{};
	std::array<char, 512> loraDirInput{};
	std::array<char, 512> embedDirInput{};

	std::string prompt;
	std::string negativePrompt;
	std::string modelPath;
	std::string loraDir;
	std::string embedDir;
	std::string statusMessage;
	std::string modelSummary;

	std::vector<std::pair<std::string, std::string>> discoveredLoras;
	std::vector<std::pair<std::string, std::string>> discoveredEmbeddings;
	std::vector<ofxGgmlStableDiffusionLora> activeLoras;

	int selectedLoraIndex = 0;
	int width = 512;
	int height = 512;
	int sampleSteps = 20;
	int seed = -1;
	float cfgScale = 7.0f;
	float loraStrength = 0.8f;
	bool selectedLoraHighNoise = false;
	bool imGuiOk = true;
	bool generating = false;
	bool contextLoading = false;
	std::atomic<float> progress{0.0f};
};
