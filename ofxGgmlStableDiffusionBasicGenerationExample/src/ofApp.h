#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
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
    std::string getLastModelPathFile() const;
    std::string loadSavedModelPath() const;
    void saveLoadedModelPath(const std::string& path) const;
    void loadModel(const std::string& path);
    void browseForModel();
    void browseForInputImage();
    void browseForMaskImage();
    void browseForControlImage();
    void browseForLora();
    void clearInputImage();
    void clearMaskImage();
    void clearControlImage();
    void clearLora();
    bool loadUiImage(const std::string& path, ofImage& image, std::string& imagePath, const std::string& label);
    void syncRequestFromUi();
    void startGeneration();
    void cancelGeneration();
    void saveResult();
    void drawResultPreview();

    ofxGgmlStableDiffusion sd;
    ofxImGui::Gui gui;
    ofImage resultImage;
    std::array<char, 512> promptInput{};
    std::array<char, 512> negativePromptInput{};
    std::string prompt;
    std::string negativePrompt;
    std::string modelPath;
    std::string statusMessage;
    int width = 512;
    int height = 512;
    int imageModeIndex = 0;
    int selectionModeIndex = 0;
    int sampleMethodIndex = static_cast<int>(SAMPLE_METHOD_COUNT);
    int schedulerIndex = static_cast<int>(SCHEDULER_COUNT);
    int lastResolvedSampleMethodIndex = -1;
    int lastResolvedSchedulerIndex = -1;
    int clipSkip = -1;
    int sampleSteps = 20;
    int batchCount = 1;
    int seed = -1;
    float cfgScale = 7.0f;
    float flowShift = 1.0f;
    float strength = 0.75f;
    float controlStrength = 0.9f;
    float styleStrength = 20.0f;
    float loraStrength = 1.0f;
    std::array<char, 512> inputIdImagesPathInput{};
    std::string inputIdImagesPath;
    ofImage inputImage;
    ofImage maskImage;
    ofImage controlImage;
    std::string inputImagePath;
    std::string maskImagePath;
    std::string controlImagePath;
    std::string loraPath;
    bool loraHighNoise = false;
    bool imGuiOk = true;
    bool generating = false;
    bool modelLoadInProgress = false;
    bool modelLoaded = false;
	bool smokeMode = false;
	bool smokeStarted = false;
	std::string smokeOutputPath;
    std::atomic<float> progress{0.0f};
};
