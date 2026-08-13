#include "ofApp.h"

#include "core/ofxGgmlStableDiffusionCapabilityHelpers.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

std::string displayFileName(const std::string& path) {
	const std::size_t slash = path.find_last_of("/\\");
	if (slash == std::string::npos) {
		return path;
	}
	return path.substr(slash + 1);
}

bool isSupportedModelPath(const std::string& path) {
	return ofxGgmlStableDiffusionExampleIsImageModelPath(path);
}

bool isImageSmokeEnabled() {
	std::string value = ofGetEnv("OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE");
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value == "1" || value == "true" || value == "yes";
}

uint64_t readImageSmokeTimeoutMillis() {
	const std::string value =
		ofGetEnv("OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_TIMEOUT_MS");
	if (value.empty()) {
		return 120000;
	}
	try {
		return std::max<uint64_t>(1000, std::stoull(value));
	} catch (const std::exception&) {
		return 120000;
	}
}

void writeImageSmokeStatus(const std::string& message) {
	const std::string path =
		ofGetEnv("OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_STATUS");
	if (path.empty()) {
		return;
	}
	std::ofstream output(path, std::ios::app);
	output << message << '\n';
}

std::string supportedModeLabel(const ofxGgmlStableDiffusionCapabilities& capabilities) {
	std::vector<std::string> modes;
	if (capabilities.textToImage) modes.emplace_back("text-to-image");
	if (capabilities.imageToImage) modes.emplace_back("image-to-image");
	if (capabilities.inpainting) modes.emplace_back("inpainting");
	if (capabilities.imageToVideo) modes.emplace_back("video");
	if (modes.empty()) return "none detected";
	std::string label = modes.front();
	for (std::size_t i = 1; i < modes.size(); ++i) label += ", " + modes[i];
	return label;
}

}

//--------------------------------------------------------------
void ofApp::setup() {
	writeImageSmokeStatus("setup:start");
	ofSetWindowTitle("ofxGgmlStableDiffusion starter");
	ofSetFrameRate(60);
	imageSmoke = isImageSmokeEnabled();
	imageSmokeStartMillis = ofGetElapsedTimeMillis();
	imageSmokeTimeoutMillis = readImageSmokeTimeoutMillis();
	imageSmokeOutputPath =
		ofGetEnv("OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_OUTPUT");
	ofSetLogLevel(imageSmoke ? OF_LOG_NOTICE : OF_LOG_WARNING);

	modelPath = findInitialModelPath();
	refreshModelList();
	loadRuntimePreferences();
	prompt = "A serene mountain landscape at sunset, photorealistic";
	negativePrompt = "blurry, low quality, distorted";
	if (imageSmoke) {
		const std::string configuredPrompt =
			ofGetEnv("OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_PROMPT");
		if (!configuredPrompt.empty()) {
			prompt = configuredPrompt;
		}
		negativePrompt.clear();
		width = 256;
		height = 256;
		sampleSteps = 1;
		seed = 1;
		cfgScale = 1.0f;
	}

	stableDiffusion.setProgressCallback([this](int step, int steps, float) {
		const float value = steps > 0 ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
		progress.store(value);
	});

	if (imageSmoke) {
		imGuiOk = false;
		if (modelPath.empty()) {
			finishImageSmoke(1, "Image smoke model was not found");
			return;
		}
		writeImageSmokeStatus("setup:image-smoke-configure");
		configureContext();
		return;
	}

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	if (!modelPath.empty()) {
		configureContext();
	} else {
		modelSummary = "No local image model found";
		statusMessage = "Paste a model path or use Browse...";
	}
}

//--------------------------------------------------------------
void ofApp::update() {
	const bool wasGenerating = generating;
	generating = stableDiffusion.isGenerating();
	if (imageSmoke) {
		updateImageSmoke();
		return;
	}

	if (contextLoading && !stableDiffusion.isBusy()) {
		contextLoading = false;
		if (stableDiffusion.hasLoadedContext()) {
			modelSummary = "Loaded: " + displayFileName(modelPath);
			statusMessage = "Model loaded";
			saveLoadedModelPath();
			saveRuntimePreferences();
		} else {
			const auto error = stableDiffusion.getLastErrorInfo();
			modelSummary = "Place a Stable Diffusion image model in bin/data/models/.";
			statusMessage = error.code == ofxGgmlStableDiffusionErrorCode::None ?
				"Model load failed" :
				"Error: " + error.message;
		}
	}

	if (wasGenerating && !generating) {
		if (stableDiffusion.wasCancelled()) {
			statusMessage = "Generation cancelled";
			return;
		}
		if (stableDiffusion.hasImageResult()) {
			const auto images = stableDiffusion.getImages();
			if (!images.empty()) {
				resultImage.setFromPixels(images.front().pixels);
			}
			statusMessage = "Image ready. Seed: " + ofToString(stableDiffusion.getLastUsedSeed());
			return;
		}
		const auto error = stableDiffusion.getLastErrorInfo();
		if (error.code != ofxGgmlStableDiffusionErrorCode::None) {
			statusMessage = "Error: " + error.message;
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(24);
	drawResultPreview();

	if (!imGuiOk) {
		ofSetColor(255);
		ofDrawBitmapString(statusMessage, 20, 20);
		return;
	}

	gui.begin();
	ImGui::SetNextWindowSize(ImVec2(600.0f, 680.0f), ImGuiCond_Once);
	if (ImGui::Begin("Stable Diffusion Starter")) {
		const bool busy = stableDiffusion.isBusy();
		const auto capabilities = stableDiffusion.getCapabilities();
		const std::string runtimeLabel =
			ofxGgmlStableDiffusionExampleRuntimeLabel(stableDiffusion);
		ImGui::TextWrapped("%s", statusMessage.c_str());
		ImGui::TextWrapped("%s", modelSummary.c_str());
		ImGui::Text("Backend: %s", runtimeLabel.c_str());
		ImGui::SameLine();
		ImGui::Text("| Context: %s", stableDiffusion.hasLoadedContext() ?
			"loaded" : (contextLoading ? "loading" : "not loaded"));
		ImGui::TextWrapped("VRAM policy: %s", memoryPolicySummary().c_str());
		if (!modelPath.empty() && ofFile::doesFileExist(modelPath)) {
			ImGui::Text("Model file: %s", ofxGgmlStableDiffusionExampleFileSizeLabel(modelPath).c_str());
		}
		if (capabilities.contextConfigured) {
			ImGui::Text("Model family: %s", ofxGgmlStableDiffusionModelFamilyLabel(capabilities.modelFamily));
			const std::string modes = supportedModeLabel(capabilities);
			ImGui::TextWrapped("Supported modes: %s", modes.c_str());
			if (!capabilities.textToImage && capabilities.imageToVideo) {
				ImGui::TextWrapped("This is a video model. Use ofxGgmlStableDiffusionVideoGenerationExample.");
			}
		}
		if (busy) {
			ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
		}
		ImGui::Separator();

		const std::string selectedLabel = selectedModelIndex >= 0 &&
			selectedModelIndex < static_cast<int>(availableModels.size()) ?
			displayFileName(availableModels[static_cast<std::size_t>(selectedModelIndex)]) :
			"Choose a discovered model";
		if (ImGui::BeginCombo("Local models", selectedLabel.c_str())) {
			for (int i = 0; i < static_cast<int>(availableModels.size()); ++i) {
				const bool selected = i == selectedModelIndex;
				const std::string label = displayFileName(availableModels[static_cast<std::size_t>(i)]) +
					" (" + ofxGgmlStableDiffusionExampleFileSizeLabel(
						availableModels[static_cast<std::size_t>(i)]) + ")";
				if (ImGui::Selectable(label.c_str(), selected)) {
					selectedModelIndex = i;
					modelPath = availableModels[static_cast<std::size_t>(i)];
					statusMessage = "Selected: " + displayFileName(modelPath);
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::InputText("Model path", &modelPath);
		if (busy) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Rescan models")) {
			refreshModelList();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse...")) {
			browseForModel();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Context")) {
			configureContext();
		}
		if (busy) {
			ImGui::EndDisabled();
		}
		if (ImGui::CollapsingHeader("Backend and memory policy")) {
			ImGui::InputText("Max VRAM", &maxVram);
			ImGui::InputText("Split mode", &splitMode);
			ImGui::Checkbox("Auto-fit devices", &autoFit);
			ImGui::Checkbox("Stream layers", &streamLayers);
			ImGui::Checkbox("Eager-load weights", &eagerLoad);
			ImGui::TextWrapped(
				"Max VRAM accepts values such as 6, -1, or cuda0=6,cuda1=8. "
				"Split mode may be layer, row, or a per-module assignment. "
				"These are placement limits, not measured live VRAM usage. "
				"Changes take effect on the next context load.");
		}

		ImGui::Separator();
		ImGui::InputTextMultiline("Prompt", &prompt, ImVec2(-1.0f, 90.0f));
		ImGui::InputTextMultiline("Negative", &negativePrompt, ImVec2(-1.0f, 56.0f));
		ImGui::InputInt("Width", &width, 64, 128);
		ImGui::InputInt("Height", &height, 64, 128);
		ImGui::SliderInt("Steps", &sampleSteps, 1, 80);
		ImGui::SliderFloat("CFG", &cfgScale, 1.0f, 15.0f);
		ImGui::InputInt("Seed", &seed);

		const bool canGenerate = stableDiffusion.hasLoadedContext() && capabilities.textToImage && !busy;
		if (!canGenerate) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Generate")) {
			startGeneration();
		}
		if (!canGenerate) {
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		if (!busy || stableDiffusion.isCancellationRequested()) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(stableDiffusion.isCancellationRequested() ? "Stopping..." : "Cancel")) {
			cancelGeneration();
		}
		if (!busy || stableDiffusion.isCancellationRequested()) {
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
	return ofToDataPath("last_model.txt", true);
}

//--------------------------------------------------------------
std::string ofApp::getRuntimePreferencesFile() const {
	return ofToDataPath("last_runtime_settings.txt", true);
}

//--------------------------------------------------------------
std::string ofApp::findInitialModelPath() const {
	const std::string configured = ofGetEnv("OFXGGML_STABLE_DIFFUSION_MODEL");
	if (isSupportedModelPath(configured) && ofFile::doesFileExist(configured)) {
		return configured;
	}

	std::ifstream saved(getLastModelPathFile());
	std::string savedPath;
	if (saved && std::getline(saved, savedPath) &&
		isSupportedModelPath(savedPath) && ofFile::doesFileExist(savedPath)) {
		return savedPath;
	}

	const auto candidates = ofxGgmlStableDiffusionExampleDiscoverModels(
		ofToDataPath("models", true));
	if (!candidates.empty()) {
		return candidates.front();
	}
	return "";
}

//--------------------------------------------------------------
void ofApp::refreshModelList() {
	availableModels = ofxGgmlStableDiffusionExampleDiscoverModels(
		ofToDataPath("models", true));
	if (isSupportedModelPath(modelPath) && ofFile::doesFileExist(modelPath)) {
		availableModels.push_back(modelPath);
	}
	std::sort(availableModels.begin(), availableModels.end());
	availableModels.erase(
		std::unique(availableModels.begin(), availableModels.end()),
		availableModels.end());
	selectedModelIndex = -1;
	for (int i = 0; i < static_cast<int>(availableModels.size()); ++i) {
		if (availableModels[static_cast<std::size_t>(i)] == modelPath) {
			selectedModelIndex = i;
			break;
		}
	}
	if (modelPath.empty() && !availableModels.empty()) {
		selectedModelIndex = 0;
		modelPath = availableModels.front();
	}
	statusMessage = availableModels.empty() ?
		"No models found in bin/data/models" :
		"Found " + ofToString(availableModels.size()) + " local model(s)";
}

//--------------------------------------------------------------
std::string ofApp::memoryPolicySummary() const {
	std::string summary = maxVram.empty() ?
		(autoFit ? "automatic device fit" : "runtime default") :
		"limit " + maxVram;
	if (!splitMode.empty()) summary += ", split " + splitMode;
	if (streamLayers) summary += ", layer streaming";
	if (eagerLoad) summary += ", eager weights";
	return summary;
}

//--------------------------------------------------------------
void ofApp::saveLoadedModelPath() {
	if (!stableDiffusion.hasLoadedContext() ||
		!isSupportedModelPath(modelPath) || !ofFile::doesFileExist(modelPath)) {
		return;
	}
	std::ofstream saved(getLastModelPathFile(), std::ios::trunc);
	if (saved) {
		saved << modelPath << '\n';
	}
}

//--------------------------------------------------------------
void ofApp::loadRuntimePreferences() {
	std::ifstream saved(getRuntimePreferencesFile());
	if (!saved) {
		return;
	}

	std::getline(saved, maxVram);
	std::getline(saved, splitMode);
	std::string value;
	if (std::getline(saved, value)) {
		autoFit = value == "1";
	}
	if (std::getline(saved, value)) {
		streamLayers = value == "1";
	}
	if (std::getline(saved, value)) {
		eagerLoad = value == "1";
	}
}

//--------------------------------------------------------------
void ofApp::saveRuntimePreferences() const {
	std::ofstream saved(getRuntimePreferencesFile(), std::ios::trunc);
	if (!saved) {
		return;
	}
	saved << maxVram << '\n'
		<< splitMode << '\n'
		<< (autoFit ? 1 : 0) << '\n'
		<< (streamLayers ? 1 : 0) << '\n'
		<< (eagerLoad ? 1 : 0) << '\n';
}

//--------------------------------------------------------------
void ofApp::configureContext() {
	if (stableDiffusion.isBusy()) {
		statusMessage = "Stable Diffusion is busy";
		return;
	}

	if (!isSupportedModelPath(modelPath)) {
		statusMessage = "Choose a .safetensors, .ckpt, .gguf, or .ggml model";
		return;
	}
	if (!ofFile::doesFileExist(modelPath)) {
		statusMessage = "Model file not found: " + modelPath;
		return;
	}
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = modelPath;
	settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	settings.maxVram = maxVram;
	settings.splitMode = splitMode;
	settings.autoFit = autoFit;
	settings.streamLayers = streamLayers;
	settings.eagerLoad = eagerLoad;
	const std::string configuredBackend = ofGetEnv("OFXGGML_STABLE_DIFFUSION_BACKEND");
	if (!configuredBackend.empty()) {
		settings.backend = configuredBackend;
		settings.paramsBackend = configuredBackend;
	} else if (ofxGgmlStableDiffusionExampleSelectedRuntimeLooksCuda()) {
		settings.backend = "cuda";
		settings.paramsBackend = "cuda";
	}
	stableDiffusion.configureContext(settings);

	contextLoading = stableDiffusion.isBusy();
	progress.store(0.0f);
	if (contextLoading) {
		modelSummary = "Loading: " + displayFileName(modelPath);
		statusMessage = "Loading model...";
	} else if (stableDiffusion.hasLoadedContext()) {
		modelSummary = "Loaded: " + displayFileName(modelPath);
		statusMessage = "Model loaded";
		saveLoadedModelPath();
		saveRuntimePreferences();
	} else {
		modelSummary = "Place a Stable Diffusion image model in bin/data/models/.";
		statusMessage = "Model not loaded";
	}
}

//--------------------------------------------------------------
void ofApp::browseForModel() {
	ofFileDialogResult result = ofSystemLoadDialog("Select Stable Diffusion model");
	if (!result.bSuccess) {
		return;
	}
	modelPath = result.getPath();
	refreshModelList();
	statusMessage = "Selected: " + displayFileName(modelPath);
}

//--------------------------------------------------------------
void ofApp::startGeneration() {
	if (stableDiffusion.isBusy()) {
		return;
	}
	if (!stableDiffusion.hasLoadedContext()) {
		statusMessage = "Load a model before generating.";
		return;
	}
	const auto capabilities = stableDiffusion.getCapabilities();
	if (!capabilities.textToImage) {
		statusMessage = capabilities.imageToVideo ?
			"This model supports video generation; open the VideoGeneration example." :
			"The loaded model does not support text-to-image generation.";
		return;
	}

	ofxGgmlStableDiffusionImageRequest request;
	request.mode = ofxGgmlStableDiffusionImageMode::TextToImage;
	request.prompt = prompt;
	request.negativePrompt = negativePrompt;
	request.width = width;
	request.height = height;
	request.sampleSteps = sampleSteps;
	request.cfgScale = cfgScale;
	request.seed = seed;

	resultImage.clear();
	progress.store(0.0f);
	statusMessage = "Generating...";
	stableDiffusion.generate(request);
}

//--------------------------------------------------------------
void ofApp::cancelGeneration() {
	ofxGgmlStableDiffusionExampleRequestCancel(stableDiffusion, statusMessage);
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
	ofxGgmlStableDiffusionExampleDrawImageFit(resultImage, 0.86f);
}

//--------------------------------------------------------------
void ofApp::updateImageSmoke() {
	const uint64_t elapsed = ofGetElapsedTimeMillis() - imageSmokeStartMillis;
	if (elapsed > imageSmokeTimeoutMillis) {
		cancelGeneration();
		finishImageSmoke(2, "Image smoke timed out");
		return;
	}

	if (!stableDiffusion.hasLoadedContext()) {
		if (stableDiffusion.isBusy()) {
			return;
		}
		contextLoading = false;
		const auto error = stableDiffusion.getLastErrorInfo();
		const std::string message =
			error.code == ofxGgmlStableDiffusionErrorCode::None ?
			"Image smoke context failed without an addon error" :
			"Image smoke context failed: " + error.message;
		finishImageSmoke(1, message);
		return;
	}

	contextLoading = false;
	if (!imageSmokeGenerationStarted) {
		writeImageSmokeStatus("generation:start");
		startGeneration();
		imageSmokeGenerationStarted = stableDiffusion.isBusy();
		if (!imageSmokeGenerationStarted) {
			const auto error = stableDiffusion.getLastErrorInfo();
			finishImageSmoke(1, "Image smoke generation did not start: " + error.message);
		}
		return;
	}

	if (stableDiffusion.isBusy()) {
		return;
	}
	if (!stableDiffusion.hasImageResult()) {
		const auto error = stableDiffusion.getLastErrorInfo();
		const std::string message =
			error.code == ofxGgmlStableDiffusionErrorCode::None ?
			"Image smoke generation finished without an image" :
			"Image smoke generation failed: " + error.message;
		finishImageSmoke(1, message);
		return;
	}

	const auto images = stableDiffusion.getImages();
	if (images.empty() || !images.front().pixels.isAllocated()) {
		finishImageSmoke(1, "Image smoke returned empty pixels");
		return;
	}
	if (imageSmokeOutputPath.empty()) {
		imageSmokeOutputPath = ofToDataPath("image-smoke.png", true);
	}
	const std::filesystem::path outputPath(imageSmokeOutputPath);
	if (!outputPath.parent_path().empty()) {
		std::error_code directoryError;
		std::filesystem::create_directories(outputPath.parent_path(), directoryError);
		if (directoryError) {
			finishImageSmoke(1, "Image smoke could not create output directory: " +
				directoryError.message());
			return;
		}
	}
	if (!ofSaveImage(images.front().pixels, imageSmokeOutputPath, OF_IMAGE_QUALITY_BEST)) {
		finishImageSmoke(1, "Image smoke could not save: " + imageSmokeOutputPath);
		return;
	}
	writeImageSmokeStatus("generation:saved:" + imageSmokeOutputPath);
	finishImageSmoke(0, "Image smoke generated successfully");
}

//--------------------------------------------------------------
void ofApp::finishImageSmoke(int exitCode, const std::string& message) {
	imageSmoke = false;
	writeImageSmokeStatus("finish:" + ofToString(exitCode) + ":" + message);
	if (exitCode == 0) {
		ofLogNotice("ofxGgmlStableDiffusionExample") << message
			<< " (" << imageSmokeOutputPath << ")";
	} else {
		ofLogError("ofxGgmlStableDiffusionExample") << message;
	}
#ifdef _WIN32
	TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exitCode));
#endif
	ofExit(exitCode);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == ' ') {
		startGeneration();
	}
	if (key == 27 || key == 'c' || key == 'C') {
		cancelGeneration();
	}
	if (key == 's' || key == 'S') {
		saveResult();
	}
}
