#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"
#include "ofxImGui.h"

#include <array>
#include <atomic>
#include <future>
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
	void loadModelPathSettings();
	void saveModelPathSettings() const;
	void applyModelDefaults(bool updatePrompt);
	void applyPromptPreset(int presetIndex);
	ofxGgmlStableDiffusionContextSettings makeCurrentContextSettings() const;
	void browseModelPath(std::string& path, std::array<char, 512>& input, bool persistModelPaths = false);
	bool browseImagePath(const std::string& title, std::string& path, std::array<char, 512>& input);
	void startGeneration();
	void cancelGeneration();
	void loadInputImage();
	void clearInputImage();
	void loadEndFrame();
	void clearEndFrame();
	void saveFrames();
	void saveVideo();
	void drawFramePreview();
	void updatePlayback();
	void updateExportJob();
	void updateContextSmoke();
	void finishContextSmoke(int exitCode, const std::string& message);

	struct ExportJobResult {
		bool success = false;
		std::string message;
	};

	ofxGgmlStableDiffusion sd;
	ofxImGui::Gui gui;
	ofImage inputImagePreview;
	ofImage endFramePreview;
	ofImage framePreview;
	ofPixels inputPixels;
	ofPixels endFramePixels;
	sd_image_t inputImage{0, 0, 0, nullptr};
	sd_image_t endFrame{0, 0, 0, nullptr};
	std::array<char, 512> promptInput{};
	std::array<char, 512> promptBInput{};
	std::array<char, 512> negativePromptInput{};
	std::array<char, 512> modelPathInput{};
	std::array<char, 512> t5xxlPathInput{};
	std::array<char, 512> vaePathInput{};
	std::array<char, 512> imagePathInput{};
	std::array<char, 512> endFramePathInput{};
	std::string prompt;
	std::string promptB;
	std::string negativePrompt;
	std::string modelPath;
	std::string t5xxlPath;
	std::string vaePath;
	std::string imagePath;
	std::string endFramePath;
	std::string statusMessage;
	std::string modelSummary;
	int width = 832;
	int height = 480;
	int frameCount = 6;
	int fps = 6;
	int sampleSteps = 12;
	int seed = -1;
	int seedIncrement = 1;
	int currentFrame = 0;
	int previewFps = 6;
	int promptPresetIndex = 0;
	int exportFormatIndex = 0;
	uint64_t lastPreviewFrameMillis = 0;
	float cfgScale = 5.0f;
	float guidance = 5.0f;
	float strength = 0.75f;
	float eta = 0.0f;
	float flowShift = 5.0f;
	float moeBoundary = 0.875f;
	float vaceStrength = 1.0f;
	bool useInputImage = false;
	bool useEndFrame = false;
	bool enablePromptInterpolation = false;
	bool useSeedSequence = false;
	bool imGuiOk = true;
	bool generating = false;
	bool contextLoading = false;
	bool contextSmoke = false;
	bool exportInProgress = false;
	bool previewPlaying = false;
	uint64_t contextSmokeStartMillis = 0;
	uint64_t contextSmokeTimeoutMillis = 900000;
	std::atomic<float> progress{0.0f};
	std::future<ExportJobResult> exportFuture;
};
