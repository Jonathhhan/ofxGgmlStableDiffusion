#include "ofApp.h"
#include "ofxGgmlStableDiffusionExampleHelpers.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace {

const char* kImageModes[] = {
    "Text to Image",
    "Image to Image",
    "Inpainting"
};

const char* kSelectionModes[] = {
    "Keep Order",
    "Rerank",
    "Best Only"
};

const char* kSampleMethods[] = {
    "Euler",
    "Euler A",
    "Heun",
    "DPM2",
    "DPM++ 2S A",
    "DPM++ 2M",
    "DPM++ 2M v2",
    "IPNDM",
    "IPNDM V",
    "LCM",
    "DDIM Trailing",
    "TCD",
    "Res Multistep",
    "Res 2S",
    "ER SDE"
};

const char* kSchedulers[] = {
    "Discrete",
    "Karras",
    "Exponential",
    "AYS",
    "GITS",
    "SGM Uniform",
    "Simple",
    "Smoothstep",
    "KL Optimal",
    "LCM",
    "Bong Tangent"
};

std::string displayFileName(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string lowerExtension(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }

    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

bool isSupportedModelPath(const std::string& path) {
    const std::string extension = lowerExtension(path);
    return extension == "safetensors" || extension == "ckpt" ||
        extension == "gguf" || extension == "ggml";
}

bool isSupportedLoraPath(const std::string& path) {
    const std::string extension = lowerExtension(path);
    return extension == "safetensors" || extension == "ckpt" ||
        extension == "pt" || extension == "bin";
}

ofxGgmlStableDiffusionImageMode imageModeFromIndex(int index) {
    switch (index) {
    case 1: return ofxGgmlStableDiffusionImageMode::ImageToImage;
    case 2: return ofxGgmlStableDiffusionImageMode::Inpainting;
    case 0:
    default:
        return ofxGgmlStableDiffusionImageMode::TextToImage;
    }
}

ofxGgmlStableDiffusionImageSelectionMode selectionModeFromIndex(int index) {
    switch (index) {
    case 1: return ofxGgmlStableDiffusionImageSelectionMode::Rerank;
    case 2: return ofxGgmlStableDiffusionImageSelectionMode::BestOnly;
    case 0:
    default:
        return ofxGgmlStableDiffusionImageSelectionMode::KeepOrder;
    }
}

bool browseImagePath(const std::string& title, std::string& selectedPath) {
    ofFileDialogResult result = ofSystemLoadDialog(title);
    if (!result.bSuccess) {
        return false;
    }

    selectedPath = result.getPath();
    return true;
}

bool drawResolvedCombo(
    const char* label,
    int& selectedIndex,
    const char* const* items,
    int itemCount,
    int resolvedIndex) {

    const char* preview = "";
    if (selectedIndex >= 0 && selectedIndex < itemCount) {
        preview = items[selectedIndex];
    } else if (resolvedIndex >= 0 && resolvedIndex < itemCount) {
        preview = items[resolvedIndex];
    }

    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        for (int i = 0; i < itemCount; ++i) {
            const bool selected = selectedIndex == i;
            if (ImGui::Selectable(items[i], selected)) {
                selectedIndex = i;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

}

//--------------------------------------------------------------
void ofApp::setup() {
	const char* smokeModel = std::getenv("OFXGGML_SD_SMOKE_MODEL");
	const char* smokeOutput = std::getenv("OFXGGML_SD_SMOKE_OUTPUT");
	smokeMode = smokeModel && *smokeModel && smokeOutput && *smokeOutput;
	if (smokeMode) {
		smokeOutputPath = smokeOutput;
		width = 256;
		height = 256;
		sampleSteps = 2;
		batchCount = 1;
	}
    progress.store(0.0f);
    statusMessage = "Ready";
    prompt = "A serene mountain landscape at sunset, photorealistic";
    negativePrompt = "blurry, low quality, distorted";
    std::copy(prompt.begin(), prompt.end(), promptInput.begin());
    std::copy(negativePrompt.begin(), negativePrompt.end(), negativePromptInput.begin());
    std::copy(inputIdImagesPath.begin(), inputIdImagesPath.end(), inputIdImagesPathInput.begin());

    ofSetLogLevel(OF_LOG_WARNING);
    auto window = ofGetCurrentWindow();
    const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
    if (!(setupState & ofxImGui::SetupState::Success)) {
        imGuiOk = false;
        statusMessage = "ImGui setup failed";
    }

    // Copy lightweight progress state from the worker thread;
    // handle UI/image work later from update()/draw().
    sd.setProgressCallback([this](int step, int steps, float time) {
        const float value = steps > 0 ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
        progress.store(value);
    });

	const std::string lastModelPath = loadSavedModelPath();
	loadModel(smokeMode ? std::string(smokeModel) :
		(lastModelPath.empty() ? ofToDataPath("models/sd_v1.5.safetensors") : lastModelPath));

    ofLogNotice() << "Ready. Use the ImGui panel or press SPACE to generate an image.";
}

//--------------------------------------------------------------
void ofApp::update() {
    // Track the previous state so result handling only runs once per completed generation.
    const bool wasGenerating = generating;
    generating = sd.isGenerating();
    modelLoaded = sd.hasLoadedContext();
    if (modelLoaded) {
        const sample_method_t requestedSampleMethod =
            sampleMethodIndex >= 0 && sampleMethodIndex < static_cast<int>(SAMPLE_METHOD_COUNT)
                ? static_cast<sample_method_t>(sampleMethodIndex)
                : SAMPLE_METHOD_COUNT;
        lastResolvedSampleMethodIndex = static_cast<int>(sd.getResolvedSampleMethod(SAMPLE_METHOD_COUNT));
        lastResolvedSchedulerIndex = static_cast<int>(sd.getResolvedScheduler(requestedSampleMethod, SCHEDULER_COUNT));
    }

	if (modelLoadInProgress && !sd.isBusy()) {
        modelLoadInProgress = false;
        modelLoaded = sd.hasLoadedContext();
        if (modelLoaded) {
            const sample_method_t requestedSampleMethod =
                sampleMethodIndex >= 0 && sampleMethodIndex < static_cast<int>(SAMPLE_METHOD_COUNT)
                    ? static_cast<sample_method_t>(sampleMethodIndex)
                    : SAMPLE_METHOD_COUNT;
            lastResolvedSampleMethodIndex = static_cast<int>(sd.getResolvedSampleMethod(SAMPLE_METHOD_COUNT));
            lastResolvedSchedulerIndex = static_cast<int>(sd.getResolvedScheduler(requestedSampleMethod, SCHEDULER_COUNT));
            saveLoadedModelPath(modelPath);
            statusMessage = "Model loaded: " + displayFileName(modelPath);
        } else {
            statusMessage.clear();
	}
	if (smokeMode && modelLoaded && !smokeStarted && !sd.isBusy()) {
		smokeStarted = true;
		startGeneration();
	}
    }

    // Check if generation just completed
    if (wasGenerating && !generating && sd.hasImageResult()) {
        // Get the first generated image
        auto images = sd.getImages();
		if (!images.empty()) {
			resultImage.setFromPixels(images[0].pixels);
            statusMessage = "Generation complete. Seed: " + ofToString(sd.getLastUsedSeed());
            ofLogNotice() << statusMessage;
		}
		if (smokeMode && resultImage.isAllocated()) {
			const bool saved = resultImage.save(smokeOutputPath);
			ofLogNotice("ofxGgmlStableDiffusionSmoke")
				<< "OF_WRAPPER_CUDA_SMOKE=" << (saved ? "PASS" : "FAIL")
				<< " output=" << smokeOutputPath;
			ofExit(saved ? 0 : 2);
		}

        // Check for errors
        auto error = sd.getLastErrorInfo();
        if (error.code != ofxGgmlStableDiffusionErrorCode::None) {
            statusMessage = "Error: " + error.message;
            ofLogError() << "Error: " << error.message;
            ofLogNotice() << "Suggestion: " << error.suggestion;
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
    ofBackground(30);
    drawResultPreview();

    if (!imGuiOk) {
        ofSetColor(255);
        ofDrawBitmapString(statusMessage, 20, 20);
        return;
    }

    gui.begin();
    ImGui::SetNextWindowSize(ImVec2(520.0f, 650.0f), ImGuiCond_Once);
    if (ImGui::Begin("Basic Generation")) {
        const bool busy = sd.isBusy();
        ImGui::TextWrapped("%s", statusMessage.c_str());
        const bool showModelName = (modelLoaded || generating) && !modelPath.empty();
        ImGui::TextWrapped("Model: %s", showModelName ? displayFileName(modelPath).c_str() : "");
        ImGui::TextWrapped("%s", ofxGgmlStableDiffusionExampleRuntimeLabel(sd).c_str());
        if (busy) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Load Model...")) {
            browseForModel();
        }
        if (busy) {
            ImGui::EndDisabled();
        }
        ImGui::Separator();
        if (ImGui::InputTextMultiline("Prompt", promptInput.data(), promptInput.size(), ImVec2(-1.0f, 84.0f))) {
            syncRequestFromUi();
        }
        if (ImGui::InputTextMultiline("Negative", negativePromptInput.data(), negativePromptInput.size(), ImVec2(-1.0f, 54.0f))) {
            syncRequestFromUi();
        }
        ImGui::Combo("Mode", &imageModeIndex, kImageModes, IM_ARRAYSIZE(kImageModes));
        ImGui::Combo("Selection", &selectionModeIndex, kSelectionModes, IM_ARRAYSIZE(kSelectionModes));
        const sample_method_t requestedSampleMethod =
            sampleMethodIndex >= 0 && sampleMethodIndex < static_cast<int>(SAMPLE_METHOD_COUNT)
                ? static_cast<sample_method_t>(sampleMethodIndex)
                : SAMPLE_METHOD_COUNT;
        const int resolvedSampleMethodIndex = modelLoaded
            ? static_cast<int>(sd.getResolvedSampleMethod(SAMPLE_METHOD_COUNT))
            : lastResolvedSampleMethodIndex;
        const int resolvedSchedulerIndex = modelLoaded
            ? static_cast<int>(sd.getResolvedScheduler(requestedSampleMethod, SCHEDULER_COUNT))
            : lastResolvedSchedulerIndex;
        drawResolvedCombo("Sampler", sampleMethodIndex, kSampleMethods, IM_ARRAYSIZE(kSampleMethods), resolvedSampleMethodIndex);
        drawResolvedCombo("Scheduler", schedulerIndex, kSchedulers, IM_ARRAYSIZE(kSchedulers), resolvedSchedulerIndex);
        ImGui::InputInt("Width", &width, 64, 128);
        ImGui::InputInt("Height", &height, 64, 128);
        ImGui::SliderInt("Steps", &sampleSteps, 1, 80);
        ImGui::InputInt("Clip Skip", &clipSkip);
        ImGui::SliderFloat("CFG", &cfgScale, 1.0f, 15.0f);
        ImGui::SliderFloat("Flow Shift", &flowShift, 0.0f, 10.0f);
        ImGui::SliderFloat("Strength", &strength, 0.0f, 1.0f);
        ImGui::InputInt("Batch", &batchCount);
        ImGui::InputInt("Seed", &seed);

        if (ImGui::CollapsingHeader("Image Inputs")) {
            ImGui::TextWrapped("Input: %s", inputImage.isAllocated() ? displayFileName(inputImagePath).c_str() : "");
            if (ImGui::Button("Load Input Image...")) {
                browseForInputImage();
            }
            ImGui::SameLine();
            if (!inputImage.isAllocated()) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Clear Input")) {
                clearInputImage();
            }
            if (!inputImage.isAllocated()) {
                ImGui::EndDisabled();
            }

            ImGui::TextWrapped("Mask: %s", maskImage.isAllocated() ? displayFileName(maskImagePath).c_str() : "");
            if (ImGui::Button("Load Mask Image...")) {
                browseForMaskImage();
            }
            ImGui::SameLine();
            if (!maskImage.isAllocated()) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Clear Mask")) {
                clearMaskImage();
            }
            if (!maskImage.isAllocated()) {
                ImGui::EndDisabled();
            }

            ImGui::TextWrapped("Control: %s", controlImage.isAllocated() ? displayFileName(controlImagePath).c_str() : "");
            if (ImGui::Button("Load Control Image...")) {
                browseForControlImage();
            }
            ImGui::SameLine();
            if (!controlImage.isAllocated()) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Clear Control")) {
                clearControlImage();
            }
            if (!controlImage.isAllocated()) {
                ImGui::EndDisabled();
            }
            ImGui::SliderFloat("Control Strength", &controlStrength, 0.0f, 2.0f);
            ImGui::SliderFloat("Style Strength", &styleStrength, 0.0f, 50.0f);
            if (ImGui::InputText("ID Images Path", inputIdImagesPathInput.data(), inputIdImagesPathInput.size())) {
                syncRequestFromUi();
            }

            ImGui::TextWrapped("LoRA: %s", loraPath.empty() ? "" : displayFileName(loraPath).c_str());
            if (ImGui::Button("Load LoRA...")) {
                browseForLora();
            }
            ImGui::SameLine();
            if (loraPath.empty()) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Clear LoRA")) {
                clearLora();
            }
            if (loraPath.empty()) {
                ImGui::EndDisabled();
            }
            ImGui::SliderFloat("LoRA Strength", &loraStrength, -2.0f, 2.0f);
            ImGui::Checkbox("LoRA High Noise", &loraHighNoise);
        }

        if (generating) {
            ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
        }

        if (generating || !modelLoaded || busy) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Generate")) {
            startGeneration();
        }
        if (generating || !modelLoaded || busy) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (!busy) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(sd.isCancellationRequested() ? "Stopping..." : "Cancel")) {
            cancelGeneration();
        }
        if (!busy) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (!resultImage.isAllocated()) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Save")) {
            saveResult();
        }
        if (!resultImage.isAllocated()) {
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
    gui.end();
}

//--------------------------------------------------------------
std::string ofApp::getLastModelPathFile() const {
    return ofToDataPath("last_model.txt");
}

//--------------------------------------------------------------
std::string ofApp::loadSavedModelPath() const {
    std::ifstream input(getLastModelPathFile());
    if (!input.is_open()) {
        return "";
    }

    std::string path;
    std::getline(input, path);
    return isSupportedModelPath(path) ? path : "";
}

//--------------------------------------------------------------
void ofApp::saveLoadedModelPath(const std::string& path) const {
    if (!isSupportedModelPath(path)) {
        return;
    }

    std::ofstream output(getLastModelPathFile(), std::ios::trunc);
    if (output.is_open()) {
        output << path << '\n';
    }
}

//--------------------------------------------------------------
void ofApp::loadModel(const std::string& path) {
    if (sd.isBusy()) {
        statusMessage = "Model or generation task is already running";
        return;
    }

    modelPath = path;
    modelLoaded = false;
    modelLoadInProgress = true;
    statusMessage = "Loading model: " + displayFileName(modelPath);

    ofxGgmlStableDiffusionContextSettings settings;
    settings.modelPath = modelPath;
    settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	if (smokeMode) {
		settings.backend = "cuda";
		settings.paramsBackend = "cuda";
	}
    sd.configureContext(settings);

    if (!sd.isBusy()) {
        modelLoadInProgress = false;
        if (sd.hasLoadedContext()) {
            modelLoaded = true;
            saveLoadedModelPath(modelPath);
            statusMessage = "Model loaded: " + displayFileName(modelPath);
        } else {
            statusMessage.clear();
        }
    }
}

//--------------------------------------------------------------
void ofApp::browseForModel() {
    ofFileDialogResult result = ofSystemLoadDialog("Select Stable Diffusion model (.safetensors, .ckpt, .gguf, .ggml)");
    if (!result.bSuccess) {
        return;
    }

    const std::string selectedPath = result.getPath();
    if (!isSupportedModelPath(selectedPath)) {
        statusMessage = "Choose a .safetensors, .ckpt, .gguf, or .ggml model";
        return;
    }

    loadModel(selectedPath);
}

//--------------------------------------------------------------
void ofApp::browseForInputImage() {
    std::string selectedPath;
    if (browseImagePath("Select input image", selectedPath)) {
        loadUiImage(selectedPath, inputImage, inputImagePath, "Input image");
    }
}

//--------------------------------------------------------------
void ofApp::browseForMaskImage() {
    std::string selectedPath;
    if (browseImagePath("Select mask image", selectedPath)) {
        loadUiImage(selectedPath, maskImage, maskImagePath, "Mask image");
    }
}

//--------------------------------------------------------------
void ofApp::browseForControlImage() {
    std::string selectedPath;
    if (browseImagePath("Select control image", selectedPath)) {
        loadUiImage(selectedPath, controlImage, controlImagePath, "Control image");
    }
}

//--------------------------------------------------------------
void ofApp::browseForLora() {
    ofFileDialogResult result = ofSystemLoadDialog("Select LoRA (.safetensors, .ckpt, .pt, .bin)");
    if (!result.bSuccess) {
        return;
    }

    const std::string selectedPath = result.getPath();
    if (!isSupportedLoraPath(selectedPath)) {
        statusMessage = "Choose a .safetensors, .ckpt, .pt, or .bin LoRA";
        return;
    }

    loraPath = selectedPath;
    statusMessage = "LoRA: " + displayFileName(loraPath);
}

//--------------------------------------------------------------
void ofApp::clearInputImage() {
    inputImage.clear();
    inputImagePath.clear();
}

//--------------------------------------------------------------
void ofApp::clearMaskImage() {
    maskImage.clear();
    maskImagePath.clear();
}

//--------------------------------------------------------------
void ofApp::clearControlImage() {
    controlImage.clear();
    controlImagePath.clear();
}

//--------------------------------------------------------------
void ofApp::clearLora() {
    loraPath.clear();
    loraHighNoise = false;
}

//--------------------------------------------------------------
bool ofApp::loadUiImage(const std::string& path, ofImage& image, std::string& imagePath, const std::string& label) {
    ofImage loaded;
    if (!loaded.load(path)) {
        statusMessage = label + " failed to load";
        return false;
    }

    image = loaded;
    imagePath = path;
    statusMessage = label + ": " + displayFileName(path);
    return true;
}

//--------------------------------------------------------------
void ofApp::syncRequestFromUi() {
    prompt = std::string(promptInput.data());
    negativePrompt = std::string(negativePromptInput.data());
    inputIdImagesPath = std::string(inputIdImagesPathInput.data());
}

//--------------------------------------------------------------
void ofApp::startGeneration() {
    if (generating) {
        return;
    }
    if (!sd.hasLoadedContext()) {
        modelLoaded = false;
        return;
    }

    syncRequestFromUi();
    ofxGgmlStableDiffusionImageRequest request;
    request.mode = imageModeFromIndex(imageModeIndex);
    request.selectionMode = selectionModeFromIndex(selectionModeIndex);
    request.initImage = inputImage.isAllocated() ? ofxGgmlStableDiffusionExampleImageView(inputImage.getPixels()) : sd_image_t{0, 0, 0, nullptr};
    request.maskImage = maskImage.isAllocated() ? ofxGgmlStableDiffusionExampleImageView(maskImage.getPixels()) : sd_image_t{0, 0, 0, nullptr};
    request.prompt = prompt;
    request.negativePrompt = negativePrompt;
    request.clipSkip = clipSkip;
    request.width = width;
    request.height = height;
    request.sampleMethod = static_cast<sample_method_t>(sampleMethodIndex);
    request.schedule = static_cast<scheduler_t>(schedulerIndex);
    request.sampleSteps = sampleSteps;
    request.flowShift = flowShift;
    request.cfgScale = cfgScale;
    if (request.mode != ofxGgmlStableDiffusionImageMode::TextToImage) {
        request.strength = strength;
    }
    request.seed = seed;
    request.batchCount = batchCount;
    sd_image_t controlImageView{0, 0, 0, nullptr};
    if (controlImage.isAllocated()) {
        controlImageView = ofxGgmlStableDiffusionExampleImageView(controlImage.getPixels());
        request.controlCond = &controlImageView;
        request.controlStrength = controlStrength;
    }
    request.styleStrength = styleStrength;
    request.inputIdImagesPath = inputIdImagesPath;
    if (!loraPath.empty()) {
        request.loras.push_back({loraPath, loraStrength, loraHighNoise});
    }

    progress.store(0.0f);
    statusMessage = "Generating...";
    sd.generate(request);
    ofLogNotice() << "Starting generation...";
}

//--------------------------------------------------------------
void ofApp::cancelGeneration() {
    ofxGgmlStableDiffusionExampleRequestCancel(sd, statusMessage, "Cancellation requested...");
}

//--------------------------------------------------------------
void ofApp::saveResult() {
    if (!resultImage.isAllocated()) {
        return;
    }
    std::string filename = "output_" + ofGetTimestampString() + ".png";
    resultImage.save(filename);
    statusMessage = "Saved: " + filename;
    ofLogNotice() << statusMessage;
}

//--------------------------------------------------------------
void ofApp::drawResultPreview() {
    if (!resultImage.isAllocated()) {
        return;
    }

    float scale = std::min(
        ofGetWidth() / resultImage.getWidth(),
        ofGetHeight() / resultImage.getHeight()
    ) * 0.9f;

    float w = resultImage.getWidth() * scale;
    float h = resultImage.getHeight() * scale;
    float x = (ofGetWidth() - w) * 0.5f;
    float y = (ofGetHeight() - h) * 0.5f;

    resultImage.draw(x, y, w, h);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
    if (key == ' ') {
        startGeneration();
    }

    if (key == 27 || key == 'c' || key == 'C') {
        cancelGeneration();
    }

    if (key == 's' && resultImage.isAllocated()) {
        saveResult();
    }
}
