#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>

namespace {
const char* imageModeLabels[] = {
	"TextToImage",
	"ImageToImage",
	"Inpainting"
};
const char* backendLabels[] = {"Auto", "CPU", "CUDA", "Vulkan", "Metal"};
}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlStableDiffusion image workflow");
	ofSetFrameRate(60);
	ofSetLogLevel(OF_LOG_WARNING);

	prompt = "A cinematic portrait, soft light, detailed";
	negativePrompt = "blurry, low quality, distorted";
	modelPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_MODEL"},
		{"models/sd_v1.5.safetensors"});
	controlNetPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_CONTROL_NET"},
		{"models/controlnet/control.safetensors"});
	statusMessage = "Ready";

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	sd.setProgressCallback([this](int step, int steps, float time) {
		const float value = steps > 0 ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
		progress.store(value);
	});
	if (ofFile::doesFileExist(modelPath)) {
		configureContext();
	} else {
		modelSummary = "Choose a local image model to begin.";
		statusMessage = "Browse or paste a model path";
	}
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
	ImGui::SetNextWindowSize(ImVec2(620.0f, 720.0f), ImGuiCond_Once);
	if (ImGui::Begin("Image Workflow")) {
		ImGui::TextWrapped("%s", statusMessage.c_str());
		ImGui::TextWrapped("%s", modelSummary.c_str());
		ImGui::TextWrapped("%s", ofxGgmlStableDiffusionExampleRuntimeLabel(sd).c_str());
		if (generating) {
			ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
		}
		ImGui::Separator();

		ImGui::InputText("Model", &modelPath);
		ImGui::SameLine();
		if (ImGui::Button("Browse##model")) {
			browseModelPath(modelPath);
		}
		ImGui::InputText("ControlNet model", &controlNetPath);
		ImGui::SameLine();
		if (ImGui::Button("Browse##controlnet")) {
			browseModelPath(controlNetPath);
		}
		ImGui::Combo("Backend", &backendIndex, backendLabels, IM_ARRAYSIZE(backendLabels));
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

		ImGui::Text("Workflow: %s", imageModeLabels[modeIndex]);
		if (ImGui::Button("Text to image")) {
			modeIndex = 0;
			strength = 0.5f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Image to image")) {
			modeIndex = 1;
			strength = 0.5f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Inpainting")) {
			modeIndex = 2;
			strength = 0.75f;
		}

		ImGui::InputTextMultiline("Prompt", &prompt, ImVec2(-1.0f, 84.0f));
		ImGui::InputTextMultiline("Negative", &negativePrompt, ImVec2(-1.0f, 54.0f));

		if (modeIndex > 0) {
			ImGui::InputText("Input image path", &inputPath);
			if (ImGui::Button("Browse Input...")) {
				if (browseImagePath("Select input image", inputPath)) {
					loadInputImage();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Load Input")) {
				loadInputImage();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Input")) {
				clearInputImage();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Match input size", &autoMatchInputSize);
			if (inputImage.data != nullptr) {
				ImGui::Text("Input ready: %ux%u", inputImage.width, inputImage.height);
			}
		}

		if (modeIndex == 2) {
			ImGui::InputText("Mask path", &maskPath);
			if (ImGui::Button("Browse Mask...")) {
				if (browseImagePath("Select mask image", maskPath)) {
					loadMaskImage();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Load Mask")) {
				loadMaskImage();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Mask")) {
				clearMaskImage();
			}
			if (maskImage.data != nullptr) {
				ImGui::Text("Mask ready: white = repaint, black = preserve");
			} else {
				ImGui::Text("Inpainting requires a mask");
			}
		}

		ImGui::Checkbox("Use ControlNet guide", &useControlImage);
		if (useControlImage) {
			ImGui::InputText("Control image path", &controlPath);
			if (ImGui::Button("Browse Control...")) {
				if (browseImagePath("Select control image", controlPath)) {
					loadControlImage();
				}
			}
			ImGui::SameLine();
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

		std::string readinessReason;
		const bool workflowIsReady = workflowReady(readinessReason);
		const bool canGenerate = workflowIsReady && !generating && !busy;
		ImGui::Text("Workflow status: %s", readinessReason.c_str());
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
void ofApp::configureContext() {
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = ofxGgmlStableDiffusionExampleResolveReadablePath(modelPath);
	if (!ofFile::doesFileExist(settings.modelPath)) {
		statusMessage = "Model file not found";
		modelSummary = "Browse or paste a readable local image model.";
		return;
	}
	const std::string resolvedControlNetPath =
		ofxGgmlStableDiffusionExampleResolveReadablePath(controlNetPath);
	if (ofFile::doesFileExist(resolvedControlNetPath)) {
		settings.controlNetPath = resolvedControlNetPath;
	}
	settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	settings.flashAttn = true;
	if (backendIndex > 0) {
		settings.backend = ofxGgmlStableDiffusionExampleLower(backendLabels[backendIndex]);
		settings.paramsBackend = settings.backend;
	} else if (ofxGgmlStableDiffusionExampleSelectedRuntimeLooksCuda()) {
		settings.backend = "cuda";
		settings.paramsBackend = "cuda";
	}
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
void ofApp::browseModelPath(std::string& path) {
	ofFileDialogResult result = ofSystemLoadDialog("Select model file");
	if (!result.bSuccess) {
		return;
	}
	path = result.getPath();
	statusMessage = "Selected model path";
}

//--------------------------------------------------------------
bool ofApp::browseImagePath(
	const std::string& title,
	std::string& path) {
	ofFileDialogResult result = ofSystemLoadDialog(title, false, path);
	if (!result.bSuccess) {
		return false;
	}
	path = result.getPath();
	return true;
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
	std::string readinessReason;
	if (!workflowReady(readinessReason)) {
		statusMessage = readinessReason;
		return;
	}
	if (!reloadRequiredImages(readinessReason)) {
		statusMessage = readinessReason;
		return;
	}
	const auto mode = currentMode();

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
	const std::string resolved =
		ofxGgmlStableDiffusionExampleResolveReadablePath(inputPath);
	ofImage source;
	if (autoMatchInputSize && source.load(resolved)) {
		width = ofxGgmlStableDiffusionExampleAlignedDimension(
			static_cast<int>(source.getWidth()));
		height = ofxGgmlStableDiffusionExampleAlignedDimension(
			static_cast<int>(source.getHeight()));
	}
	statusMessage = loadImageSlot(inputPath, inputImagePreview, inputPixels, inputImage) ?
		"Input image loaded at " + ofToString(width) + "x" + ofToString(height) :
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
	statusMessage = loadImageSlot(maskPath, maskImagePreview, maskPixels, maskImage) ?
		"Mask loaded: white repaints, black preserves" :
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
bool ofApp::workflowReady(std::string& reason) const {
	if (!sd.hasLoadedContext()) {
		reason = sd.isBusy() ? "Loading model..." : "Configure a local model first";
		return false;
	}
	const auto mode = currentMode();
	const auto capabilities = sd.getCapabilities();
	if (!capabilities.supportsImageMode(mode)) {
		reason = "The loaded model does not support " +
			std::string(imageModeLabels[modeIndex]);
		return false;
	}
	if (prompt.empty()) {
		reason = "Enter a prompt";
		return false;
	}
	if (ofxGgmlStableDiffusionImageModeUsesInputImage(mode) && inputImage.data == nullptr) {
		reason = "Load an input image for this workflow";
		return false;
	}
	if (mode == ofxGgmlStableDiffusionImageMode::Inpainting && maskImage.data == nullptr) {
		reason = "Load a mask: white repaints, black preserves";
		return false;
	}
	if (useControlImage && controlImage.data == nullptr) {
		reason = "Load a control image or disable ControlNet guide";
		return false;
	}
	if (useControlImage && !capabilities.controlNetConfigured &&
		!capabilities.nativeControlModel) {
		reason = "Configure a ControlNet model for the guide image";
		return false;
	}
	reason = "Ready to generate " + std::string(imageModeLabels[modeIndex]);
	return true;
}

//--------------------------------------------------------------
bool ofApp::reloadRequiredImages(std::string& errorMessage) {
	width = ofxGgmlStableDiffusionExampleAlignedDimension(width);
	height = ofxGgmlStableDiffusionExampleAlignedDimension(height);
	const auto mode = currentMode();
	if (ofxGgmlStableDiffusionImageModeUsesInputImage(mode) &&
		!loadImageSlot(inputPath, inputImagePreview, inputPixels, inputImage)) {
		errorMessage = "Could not resize the input image for generation";
		return false;
	}
	if (mode == ofxGgmlStableDiffusionImageMode::Inpainting &&
		!loadImageSlot(maskPath, maskImagePreview, maskPixels, maskImage)) {
		errorMessage = "Could not resize the mask for generation";
		return false;
	}
	if (useControlImage &&
		!loadImageSlot(controlPath, controlImagePreview, controlPixels, controlImage)) {
		errorMessage = "Could not resize the control image for generation";
		return false;
	}
	return true;
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
