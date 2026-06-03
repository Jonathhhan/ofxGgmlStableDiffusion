#include "ofApp.h"

#include <algorithm>

namespace {

std::string displayFileName(const std::string& path) {
	const std::size_t slash = path.find_last_of("/\\");
	if (slash == std::string::npos) {
		return path;
	}
	return path.substr(slash + 1);
}

}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlStableDiffusion starter");
	ofSetFrameRate(60);
	ofSetLogLevel(OF_LOG_WARNING);

	modelPath = ofToDataPath("models/sd_v1.5.safetensors");
	prompt = "A serene mountain landscape at sunset, photorealistic";
	negativePrompt = "blurry, low quality, distorted";
	ofxGgmlStableDiffusionExampleCopyToInput(modelPath, modelPathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(prompt, promptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(negativePrompt, negativePromptInput);

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

	configureContext();
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
	ImGui::SetNextWindowSize(ImVec2(520.0f, 520.0f), ImGuiCond_Once);
	if (ImGui::Begin("Stable Diffusion Starter")) {
		const bool busy = stableDiffusion.isBusy();
		ImGui::TextWrapped("%s", statusMessage.c_str());
		ImGui::TextWrapped("%s", modelSummary.c_str());
		ImGui::TextWrapped("%s", ofxGgmlStableDiffusionExampleRuntimeLabel(stableDiffusion).c_str());
		if (busy) {
			ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
		}
		ImGui::Separator();

		if (ImGui::InputText("Model", modelPathInput.data(), modelPathInput.size())) {
			syncRequestFromUi();
		}
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

		ImGui::Separator();
		if (ImGui::InputTextMultiline("Prompt", promptInput.data(), promptInput.size(), ImVec2(-1.0f, 90.0f))) {
			syncRequestFromUi();
		}
		if (ImGui::InputTextMultiline("Negative", negativePromptInput.data(), negativePromptInput.size(), ImVec2(-1.0f, 56.0f))) {
			syncRequestFromUi();
		}
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
void ofApp::configureContext() {
	if (stableDiffusion.isBusy()) {
		statusMessage = "Stable Diffusion is busy";
		return;
	}

	syncRequestFromUi();
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = modelPath;
	settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	stableDiffusion.configureContext(settings);

	contextLoading = stableDiffusion.isBusy();
	progress.store(0.0f);
	if (contextLoading) {
		modelSummary = "Loading: " + displayFileName(modelPath);
		statusMessage = "Loading model...";
	} else if (stableDiffusion.hasLoadedContext()) {
		modelSummary = "Loaded: " + displayFileName(modelPath);
		statusMessage = "Model loaded";
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
	ofxGgmlStableDiffusionExampleCopyToInput(modelPath, modelPathInput);
	statusMessage = "Selected: " + displayFileName(modelPath);
}

//--------------------------------------------------------------
void ofApp::syncRequestFromUi() {
	modelPath = ofxGgmlStableDiffusionExampleInputString(modelPathInput);
	prompt = ofxGgmlStableDiffusionExampleInputString(promptInput);
	negativePrompt = ofxGgmlStableDiffusionExampleInputString(negativePromptInput);
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

	syncRequestFromUi();
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
