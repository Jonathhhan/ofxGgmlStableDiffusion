#include "ofApp.h"

#include <algorithm>

namespace {
const char* imageModeLabels[] = {
	"TextToImage",
	"ImageToImage",
	"Inpainting"
};
}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlStableDiffusion image workflow");
	ofSetFrameRate(60);
	ofSetLogLevel(OF_LOG_WARNING);

	prompt = "A cinematic portrait, soft light, detailed";
	negativePrompt = "blurry, low quality, distorted";
	modelPath = ofToDataPath("models/sd_v1.5.safetensors");
	controlNetPath = ofToDataPath("models/controlnet/control.safetensors");
	ofxGgmlStableDiffusionExampleCopyToInput(prompt, promptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(negativePrompt, negativePromptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(modelPath, modelPathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(controlNetPath, controlNetPathInput);
	statusMessage = "Ready";

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	configureContext();
	sd.setProgressCallback([this](int step, int steps, float time) {
		const float value = steps > 0 ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
		progress.store(value);
	});
}

//--------------------------------------------------------------
void ofApp::update() {
	const bool wasGenerating = generating;
	generating = sd.isGenerating();

	if (contextLoading && !sd.isBusy()) {
		contextLoading = false;
		if (sd.hasLoadedContext()) {
			modelSummary = "Context configured for image workflows.";
			statusMessage = "Model loaded";
		} else {
			const auto error = sd.getLastErrorInfo();
			modelSummary = "Place an image model in bin/data/models/ before running.";
			statusMessage = error.code == ofxGgmlStableDiffusionErrorCode::None ?
				"Model load failed" :
				"Error: " + error.message;
		}
	}

	if (wasGenerating && !generating) {
		if (sd.wasCancelled()) {
			statusMessage = "Generation cancelled";
			return;
		}
		if (sd.hasImageResult()) {
			const auto images = sd.getImages();
			if (!images.empty()) {
				resultImage.setFromPixels(images[0].pixels);
			}
			statusMessage = "Image ready. Seed: " + ofToString(sd.getLastUsedSeed());
			return;
		}
		const auto error = sd.getLastErrorInfo();
		if (error.code != ofxGgmlStableDiffusionErrorCode::None) {
			statusMessage = "Error: " + error.message;
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(26);
	drawResultPreview();

	if (!imGuiOk) {
		ofSetColor(255);
		ofDrawBitmapString(statusMessage, 20, 20);
		return;
	}

	gui.begin();
	ImGui::SetNextWindowSize(ImVec2(520.0f, 590.0f), ImGuiCond_Once);
	if (ImGui::Begin("Image Workflow")) {
		ImGui::TextWrapped("%s", statusMessage.c_str());
		ImGui::TextWrapped("%s", modelSummary.c_str());
		ImGui::TextWrapped("%s", ofxGgmlStableDiffusionExampleRuntimeLabel(sd).c_str());
		if (generating) {
			ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
		}
		ImGui::Separator();

		if (ImGui::InputText("Model", modelPathInput.data(), modelPathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##model")) {
			browseModelPath(modelPath, modelPathInput);
		}
		if (ImGui::InputText("ControlNet model", controlNetPathInput.data(), controlNetPathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##controlnet")) {
			browseModelPath(controlNetPath, controlNetPathInput);
		}
		const bool busy = sd.isBusy();
		if (busy) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Configure Context")) {
			configureContext();
		}
		if (busy) {
			ImGui::EndDisabled();
		}
		ImGui::Separator();

		ImGui::Text("Mode: %s", imageModeLabels[modeIndex]);
		if (ImGui::Button("TextToImage")) {
			modeIndex = 0;
			strength = 0.5f;
		}
		ImGui::SameLine();
		if (ImGui::Button("ImageToImage")) {
			modeIndex = 1;
			strength = 0.5f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Inpainting")) {
			modeIndex = 2;
			strength = 0.75f;
		}

		if (ImGui::InputTextMultiline("Prompt", promptInput.data(), promptInput.size(), ImVec2(-1.0f, 84.0f))) {
			syncRequestFromUi();
		}
		if (ImGui::InputTextMultiline("Negative", negativePromptInput.data(), negativePromptInput.size(), ImVec2(-1.0f, 54.0f))) {
			syncRequestFromUi();
		}

		if (modeIndex > 0) {
			if (ImGui::InputText("Input image path", inputPathInput.data(), inputPathInput.size())) {
				syncRequestFromUi();
			}
			if (ImGui::Button("Load Input")) {
				loadInputImage();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Input")) {
				clearInputImage();
			}
		}

		if (modeIndex == 2) {
			if (ImGui::InputText("Mask path", maskPathInput.data(), maskPathInput.size())) {
				syncRequestFromUi();
			}
			if (ImGui::Button("Load Mask")) {
				loadMaskImage();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Mask")) {
				clearMaskImage();
			}
		}

		ImGui::Checkbox("Use ControlNet guide", &useControlImage);
		if (useControlImage) {
			if (ImGui::InputText("Control image path", controlPathInput.data(), controlPathInput.size())) {
				syncRequestFromUi();
			}
			if (ImGui::Button("Load Control")) {
				loadControlImage();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Control")) {
				clearControlImage();
			}
			ImGui::SliderFloat("Control Strength", &controlStrength, 0.0f, 2.0f);
		}

		ImGui::InputInt("Width", &width, 64, 128);
		ImGui::InputInt("Height", &height, 64, 128);
		ImGui::SliderInt("Steps", &sampleSteps, 1, 80);
		ImGui::SliderFloat("CFG", &cfgScale, 1.0f, 15.0f);
		if (modeIndex > 0) {
			ImGui::SliderFloat("Strength", &strength, 0.0f, 1.0f);
		}
		ImGui::InputInt("Seed", &seed);
		ImGui::SliderInt("Batch", &batchCount, 1, 8);

		const bool canGenerate = sd.hasLoadedContext() && !generating && !busy;
		if (generating) {
			ImGui::BeginDisabled();
		}
		if (!canGenerate) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Generate")) {
			startGeneration();
		}
		if (!canGenerate) {
			ImGui::EndDisabled();
		}
		if (generating) {
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		if (!busy || sd.isCancellationRequested()) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(sd.isCancellationRequested() ? "Stopping..." : "Cancel")) {
			cancelGeneration();
		}
		if (!busy || sd.isCancellationRequested()) {
			ImGui::EndDisabled();
		}

		if (!resultImage.isAllocated()) {
			ImGui::BeginDisabled();
		}
		ImGui::SameLine();
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
void ofApp::syncRequestFromUi() {
	prompt = ofxGgmlStableDiffusionExampleInputString(promptInput);
	negativePrompt = ofxGgmlStableDiffusionExampleInputString(negativePromptInput);
	modelPath = ofxGgmlStableDiffusionExampleInputString(modelPathInput);
	controlNetPath = ofxGgmlStableDiffusionExampleInputString(controlNetPathInput);
	inputPath = ofxGgmlStableDiffusionExampleInputString(inputPathInput);
	maskPath = ofxGgmlStableDiffusionExampleInputString(maskPathInput);
	controlPath = ofxGgmlStableDiffusionExampleInputString(controlPathInput);
}

//--------------------------------------------------------------
void ofApp::configureContext() {
	syncRequestFromUi();
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = ofxGgmlStableDiffusionExampleResolveReadablePath(modelPath);
	const std::string resolvedControlNetPath =
		ofxGgmlStableDiffusionExampleResolveReadablePath(controlNetPath);
	if (ofFile::doesFileExist(resolvedControlNetPath)) {
		settings.controlNetPath = resolvedControlNetPath;
	}
	settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	settings.flashAttn = true;
	sd.configureContext(settings);
	const auto capabilities = sd.getCapabilities();
	contextLoading = sd.isBusy();
	if (contextLoading) {
		modelSummary = "Loading image model...";
		statusMessage = "Loading model...";
	} else {
		modelSummary = capabilities.contextConfigured ?
			"Context configured for image workflows." :
			"Place an image model in bin/data/models/ before running.";
		if (!capabilities.contextConfigured) {
			const auto error = sd.getLastErrorInfo();
			statusMessage = error.code == ofxGgmlStableDiffusionErrorCode::None ?
				"Model not loaded" :
				"Error: " + error.message;
		}
	}
}

//--------------------------------------------------------------
void ofApp::browseModelPath(std::string& path, std::array<char, 512>& input) {
	ofFileDialogResult result = ofSystemLoadDialog("Select model file");
	if (!result.bSuccess) {
		return;
	}
	path = result.getPath();
	ofxGgmlStableDiffusionExampleCopyToInput(path, input);
	statusMessage = "Selected model path";
}

//--------------------------------------------------------------
void ofApp::startGeneration() {
	if (generating) {
		return;
	}
	if (!sd.hasLoadedContext()) {
		statusMessage = sd.isBusy() ? "Model is still loading." : "Load a model before generating.";
		return;
	}
	syncRequestFromUi();
	const auto mode = currentMode();
	if (ofxGgmlStableDiffusionImageModeUsesInputImage(mode) && inputImage.data == nullptr) {
		statusMessage = "Load an input image for the selected mode.";
		return;
	}
	if (mode == ofxGgmlStableDiffusionImageMode::Inpainting && maskImage.data == nullptr) {
		statusMessage = "Load a mask image for inpainting.";
		return;
	}
	if (useControlImage && controlImage.data == nullptr) {
		statusMessage = "Load a control image or disable ControlNet guide.";
		return;
	}

	ofxGgmlStableDiffusionImageRequest request;
	request.mode = mode;
	request.initImage = inputImage;
	request.maskImage = maskImage;
	request.prompt = prompt;
	request.negativePrompt = negativePrompt;
	request.width = width;
	request.height = height;
	request.sampleSteps = sampleSteps;
	request.cfgScale = cfgScale;
	request.strength = strength;
	request.seed = seed;
	request.batchCount = batchCount;
	request.controlCond = useControlImage ? &controlImage : nullptr;
	request.controlStrength = controlStrength;

	progress.store(0.0f);
	statusMessage = "Generating...";
	sd.generate(request);
}

//--------------------------------------------------------------
void ofApp::cancelGeneration() {
	ofxGgmlStableDiffusionExampleRequestCancel(sd, statusMessage);
}

//--------------------------------------------------------------
bool ofApp::loadImageSlot(const std::string& path, ofImage& image, ofPixels& pixels, sd_image_t& view) {
	return ofxGgmlStableDiffusionExampleLoadImageView(path, width, height, image, pixels, view);
}

//--------------------------------------------------------------
void ofApp::loadInputImage() {
	syncRequestFromUi();
	statusMessage = loadImageSlot(inputPath, inputImagePreview, inputPixels, inputImage) ?
		"Input image loaded" :
		"Input image load failed";
}

//--------------------------------------------------------------
void ofApp::clearInputImage() {
	inputImagePreview.clear();
	inputPixels.clear();
	inputImage = {0, 0, 0, nullptr};
	statusMessage = "Input image cleared";
}

//--------------------------------------------------------------
void ofApp::loadMaskImage() {
	syncRequestFromUi();
	statusMessage = loadImageSlot(maskPath, maskImagePreview, maskPixels, maskImage) ?
		"Mask image loaded" :
		"Mask image load failed";
}

//--------------------------------------------------------------
void ofApp::clearMaskImage() {
	maskImagePreview.clear();
	maskPixels.clear();
	maskImage = {0, 0, 0, nullptr};
	statusMessage = "Mask image cleared";
}

//--------------------------------------------------------------
void ofApp::loadControlImage() {
	syncRequestFromUi();
	statusMessage = loadImageSlot(controlPath, controlImagePreview, controlPixels, controlImage) ?
		"Control image loaded" :
		"Control image load failed";
}

//--------------------------------------------------------------
void ofApp::clearControlImage() {
	controlImagePreview.clear();
	controlPixels.clear();
	controlImage = {0, 0, 0, nullptr};
	statusMessage = "Control image cleared";
}

//--------------------------------------------------------------
void ofApp::saveResult() {
	if (!resultImage.isAllocated()) {
		return;
	}
	const std::string filename = "output_" + ofGetTimestampString() + ".png";
	resultImage.save(filename);
	statusMessage = "Saved: " + filename;
}

//--------------------------------------------------------------
void ofApp::drawResultPreview() {
	ofxGgmlStableDiffusionExampleDrawImageFit(resultImage);
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionImageMode ofApp::currentMode() const {
	switch (modeIndex) {
	case 1:
		return ofxGgmlStableDiffusionImageMode::ImageToImage;
	case 2:
		return ofxGgmlStableDiffusionImageMode::Inpainting;
	default:
		return ofxGgmlStableDiffusionImageMode::TextToImage;
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == ' ') {
		startGeneration();
	}
	if (key == 27 || key == 'c' || key == 'C') {
		cancelGeneration();
	}
}
