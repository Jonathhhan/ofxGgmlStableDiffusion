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
	void syncRequestFromUi();
	void configureContext();
	void browseModelPath(std::string& path, std::array<char, 512>& input);
	bool browseImagePath(const std::string& title, std::string& path, std::array<char, 512>& input);
	void startGeneration();
	void cancelGeneration();
	bool loadImageSlot(const std::string& path, ofImage& image, ofPixels& pixels, sd_image_t& view);
	void loadInputImage();
	void clearInputImage();
	void loadMaskImage();
	void clearMaskImage();
	void loadControlImage();
	void clearControlImage();
	void saveResult();
	void drawResultPreview();
	ofxGgmlStableDiffusionImageMode currentMode() const;

	ofxGgmlStableDiffusion sd;
	ofxImGui::Gui gui;
	ofImage resultImage;
	ofImage inputImagePreview;
	ofImage maskImagePreview;
	ofImage controlImagePreview;
	ofPixels inputPixels;
	ofPixels maskPixels;
	ofPixels controlPixels;
	sd_image_t inputImage{0, 0, 0, nullptr};
	sd_image_t maskImage{0, 0, 0, nullptr};
	sd_image_t controlImage{0, 0, 0, nullptr};
	std::array<char, 512> promptInput{};
	std::array<char, 512> negativePromptInput{};
	std::array<char, 512> modelPathInput{};
	std::array<char, 512> controlNetPathInput{};
	std::array<char, 512> inputPathInput{};
	std::array<char, 512> maskPathInput{};
	std::array<char, 512> controlPathInput{};
	std::string prompt;
	std::string negativePrompt;
	std::string modelPath;
	std::string controlNetPath;
	std::string inputPath;
	std::string maskPath;
	std::string controlPath;
	std::string statusMessage;
	std::string modelSummary;
	int modeIndex = 0;
	int width = 512;
	int height = 512;
	int sampleSteps = 20;
	int seed = -1;
	int batchCount = 1;
	float cfgScale = 7.0f;
	float strength = 0.5f;
	float controlStrength = 0.9f;
	bool useControlImage = false;
	bool imGuiOk = true;
	bool generating = false;
	bool contextLoading = false;
	std::atomic<float> progress{0.0f};
};
