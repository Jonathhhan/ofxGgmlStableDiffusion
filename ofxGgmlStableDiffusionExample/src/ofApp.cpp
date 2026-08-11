#include "ofApp.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <fstream>
#include <vector>

namespace {

std::string displayFileName(const std::string& path) {
	const std::size_t slash = path.find_last_of("/\\");
	if (slash == std::string::npos) {
		return path;
	}
	return path.substr(slash + 1);
}

bool isSupportedModelPath(const std::string& path) {
	const std::size_t dot = path.find_last_of('.');
	if (dot == std::string::npos) {
		return false;
	}
	std::string extension = ofxGgmlStableDiffusionExampleLower(path.substr(dot + 1));
	return extension == "safetensors" || extension == "ckpt" ||
		extension == "gguf" || extension == "ggml";
}

}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlStableDiffusion starter");
	ofSetFrameRate(60);
	ofSetLogLevel(OF_LOG_WARNING);

	modelPath = findInitialModelPath();
	loadRuntimePreferences();
	prompt = "A serene mountain landscape at sunset, photorealistic";
	negativePrompt = "blurry, low quality, distorted";

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	stableDiffusion.setProgressCallback([this](int step, int steps, float) {
		const float value = steps > 0 ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
		progress.store(value);
	});

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
	ImGui::SetNextWindowSize(ImVec2(560.0f, 620.0f), ImGuiCond_Once);
	if (ImGui::Begin("Stable Diffusion Starter")) {
		const bool busy = stableDiffusion.isBusy();
		ImGui::TextWrapped("%s", statusMessage.c_str());
		ImGui::TextWrapped("%s", modelSummary.c_str());
		ImGui::TextWrapped("%s", ofxGgmlStableDiffusionExampleRuntimeLabel(stableDiffusion).c_str());
		if (busy) {
			ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
		}
		ImGui::Separator();

		ImGui::InputText("Model", &modelPath);
		if (busy) {
			ImGui::BeginDisabled();
		}
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
		if (ImGui::CollapsingHeader("Runtime and GPU memory")) {
			ImGui::InputText("Max VRAM", &maxVram);
			ImGui::InputText("Split mode", &splitMode);
			ImGui::Checkbox("Auto-fit devices", &autoFit);
			ImGui::Checkbox("Stream layers", &streamLayers);
			ImGui::Checkbox("Eager-load weights", &eagerLoad);
			ImGui::TextWrapped(
				"Max VRAM accepts values such as 6, -1, or cuda0=6,cuda1=8. "
				"Split mode may be layer, row, or a per-module assignment. "
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

		const bool canGenerate = stableDiffusion.hasLoadedContext() && !busy;
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

	ofDirectory models(ofToDataPath("models", true));
	models.allowExt("safetensors");
	models.allowExt("ckpt");
	models.allowExt("gguf");
	models.allowExt("ggml");
	models.listDir();
	std::vector<std::string> candidates;
	for (std::size_t i = 0; i < models.size(); ++i) {
		const auto& file = models.getFile(static_cast<int>(i));
		if (file.isFile()) {
			candidates.push_back(file.getAbsolutePath());
		}
	}
	std::sort(candidates.begin(), candidates.end());
	if (!candidates.empty()) {
		return candidates.front();
	}
	return "";
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
