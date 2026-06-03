#include "ofApp.h"

#include <algorithm>

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlStableDiffusion LoRA embeddings");
	ofSetFrameRate(60);
	ofSetLogLevel(OF_LOG_WARNING);

	prompt = "A detailed portrait in a custom style";
	negativePrompt = "blurry, low quality, distorted";
	modelPath = ofToDataPath("models/sd_v1.5.safetensors");
	loraDir = ofToDataPath("models/lora");
	embedDir = ofToDataPath("embeddings");
	ofxGgmlStableDiffusionExampleCopyToInput(prompt, promptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(negativePrompt, negativePromptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(modelPath, modelPathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(loraDir, loraDirInput);
	ofxGgmlStableDiffusionExampleCopyToInput(embedDir, embedDirInput);
	statusMessage = "Ready";

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	configureContext();
	scanLoras();
	reloadEmbeddings();
	sd.setProgressCallback([this](int step, int steps, float) {
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
			modelSummary = "Context configured for LoRA and embedding workflows.";
			statusMessage = "Model loaded";
		} else {
			const auto error = sd.getLastErrorInfo();
			modelSummary = "Place a model in bin/data/models/ before running.";
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
	ofBackground(24);
	drawResultPreview();

	if (!imGuiOk) {
		ofSetColor(255);
		ofDrawBitmapString(statusMessage, 20, 20);
		return;
	}

	gui.begin();
	ImGui::SetNextWindowSize(ImVec2(540.0f, 650.0f), ImGuiCond_Once);
	if (ImGui::Begin("LoRA Embeddings")) {
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
		if (ImGui::InputText("LoRA dir", loraDirInput.data(), loraDirInput.size())) {
			syncRequestFromUi();
		}
		if (ImGui::InputText("Embeddings dir", embedDirInput.data(), embedDirInput.size())) {
			syncRequestFromUi();
		}

		if (ImGui::Button("Configure Context")) {
			configureContext();
		}
		ImGui::SameLine();
		if (ImGui::Button("Scan LoRAs")) {
			scanLoras();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload Embeddings")) {
			reloadEmbeddings();
		}

		ImGui::Text("LoRAs: %s", ofToString(discoveredLoras.size()).c_str());
		ImGui::InputInt("Selected LoRA", &selectedLoraIndex);
		ImGui::SliderFloat("LoRA Strength", &loraStrength, -2.0f, 2.0f);
		ImGui::Checkbox("High noise LoRA", &selectedLoraHighNoise);
		if (!discoveredLoras.empty()) {
			const int clampedIndex = ofClamp(selectedLoraIndex, 0, static_cast<int>(discoveredLoras.size()) - 1);
			ImGui::TextWrapped("%s", discoveredLoras[static_cast<std::size_t>(clampedIndex)].first.c_str());
		}
		if (ImGui::Button("Apply Selected")) {
			applySelectedLora();
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply All")) {
			applyAllLoras();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear LoRAs")) {
			clearLoras();
		}
		ImGui::Text("Active LoRAs: %s", ofToString(activeLoras.size()).c_str());

		ImGui::Separator();
		ImGui::Text("Embeddings: %s", ofToString(discoveredEmbeddings.size()).c_str());
		if (!discoveredEmbeddings.empty()) {
			ImGui::TextWrapped("%s", discoveredEmbeddings.front().first.c_str());
		}

		ImGui::Separator();
		if (ImGui::InputTextMultiline("Prompt", promptInput.data(), promptInput.size(), ImVec2(-1.0f, 80.0f))) {
			syncRequestFromUi();
		}
		if (ImGui::InputTextMultiline("Negative", negativePromptInput.data(), negativePromptInput.size(), ImVec2(-1.0f, 52.0f))) {
			syncRequestFromUi();
		}
		ImGui::InputInt("Width", &width, 64, 128);
		ImGui::InputInt("Height", &height, 64, 128);
		ImGui::SliderInt("Steps", &sampleSteps, 1, 80);
		ImGui::SliderFloat("CFG", &cfgScale, 1.0f, 15.0f);
		ImGui::InputInt("Seed", &seed);

		const bool busy = sd.isBusy();
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
	loraDir = ofxGgmlStableDiffusionExampleInputString(loraDirInput);
	embedDir = ofxGgmlStableDiffusionExampleInputString(embedDirInput);
}

//--------------------------------------------------------------
void ofApp::configureContext() {
	syncRequestFromUi();
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = modelPath;
	settings.loraModelDir = loraDir;
	settings.embedDir = embedDir;
	settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	settings.flashAttn = true;
	sd.configureContext(settings);
	sd.setLoras(activeLoras);
	const auto capabilities = sd.getCapabilities();
	contextLoading = sd.isBusy();
	if (contextLoading) {
		modelSummary = "Loading image model...";
		statusMessage = "Loading model...";
	} else {
		modelSummary = capabilities.contextConfigured ?
			"Context configured for LoRA and embedding workflows." :
			"Place a model in bin/data/models/ before running.";
		statusMessage = capabilities.contextConfigured ? "Context configured" : "Model not loaded";
	}
}

//--------------------------------------------------------------
void ofApp::scanLoras() {
	syncRequestFromUi();
	discoveredLoras = listFiles(loraDir, {"safetensors", "ckpt", "pt", "bin"});
	selectedLoraIndex = discoveredLoras.empty() ? 0 : ofClamp(selectedLoraIndex, 0, static_cast<int>(discoveredLoras.size()) - 1);
	statusMessage = "LoRAs found: " + ofToString(discoveredLoras.size());
}

//--------------------------------------------------------------
void ofApp::reloadEmbeddings() {
	syncRequestFromUi();
	if (!embedDir.empty() && sd.hasLoadedContext() && !sd.isBusy()) {
		sd.reloadEmbeddings(embedDir);
	}
	discoveredEmbeddings = sd.listEmbeddings();
	if (discoveredEmbeddings.empty()) {
		discoveredEmbeddings = listFiles(embedDir, {"pt", "ckpt", "safetensors", "bin", "gguf"});
	}
	statusMessage = "Embeddings found: " + ofToString(discoveredEmbeddings.size());
}

//--------------------------------------------------------------
void ofApp::applySelectedLora() {
	if (discoveredLoras.empty()) {
		statusMessage = "No LoRAs discovered";
		return;
	}
	selectedLoraIndex = ofClamp(selectedLoraIndex, 0, static_cast<int>(discoveredLoras.size()) - 1);
	ofxGgmlStableDiffusionLora lora;
	lora.path = discoveredLoras[static_cast<std::size_t>(selectedLoraIndex)].second;
	lora.strength = loraStrength;
	lora.isHighNoise = selectedLoraHighNoise;
	activeLoras = {lora};
	sd.setLoras(activeLoras);
	statusMessage = "Applied LoRA: " + discoveredLoras[static_cast<std::size_t>(selectedLoraIndex)].first;
}

//--------------------------------------------------------------
void ofApp::applyAllLoras() {
	activeLoras.clear();
	activeLoras.reserve(discoveredLoras.size());
	for (const auto& entry : discoveredLoras) {
		ofxGgmlStableDiffusionLora lora;
		lora.path = entry.second;
		lora.strength = loraStrength;
		lora.isHighNoise = selectedLoraHighNoise;
		activeLoras.push_back(lora);
	}
	sd.setLoras(activeLoras);
	statusMessage = "Applied LoRAs: " + ofToString(activeLoras.size());
}

//--------------------------------------------------------------
void ofApp::clearLoras() {
	activeLoras.clear();
	sd.setLoras(activeLoras);
	statusMessage = "LoRA stack cleared";
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
	ofxGgmlStableDiffusionImageRequest request;
	request.mode = ofxGgmlStableDiffusionImageMode::TextToImage;
	request.prompt = prompt;
	request.negativePrompt = negativePrompt;
	request.width = width;
	request.height = height;
	request.sampleSteps = sampleSteps;
	request.cfgScale = cfgScale;
	request.seed = seed;
	request.loras = activeLoras;

	progress.store(0.0f);
	statusMessage = "Generating with " + ofToString(activeLoras.size()) + " LoRAs...";
	sd.generate(request);
}

//--------------------------------------------------------------
void ofApp::cancelGeneration() {
	ofxGgmlStableDiffusionExampleRequestCancel(sd, statusMessage);
}

//--------------------------------------------------------------
void ofApp::saveResult() {
	if (!resultImage.isAllocated()) {
		return;
	}
	const std::string filename = "lora_embedding_" + ofGetTimestampString() + ".png";
	resultImage.save(filename);
	statusMessage = "Saved: " + filename;
}

//--------------------------------------------------------------
void ofApp::drawResultPreview() {
	ofxGgmlStableDiffusionExampleDrawImageFit(resultImage);
}

//--------------------------------------------------------------
std::vector<std::pair<std::string, std::string>> ofApp::listFiles(
	const std::string& directory,
	const std::vector<std::string>& extensions) const {
	std::vector<std::pair<std::string, std::string>> results;
	if (directory.empty()) {
		return results;
	}

	ofDirectory dir(directory);
	if (!dir.exists()) {
		return results;
	}
	for (const auto& extension : extensions) {
		dir.allowExt(extension);
	}
	dir.listDir();
	for (std::size_t i = 0; i < dir.size(); ++i) {
		const ofFile& file = dir.getFile(static_cast<int>(i));
		if (file.isFile()) {
			results.emplace_back(file.getBaseName(), file.getAbsolutePath());
		}
	}
	return results;
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
