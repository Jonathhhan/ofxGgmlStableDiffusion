#include "ofApp.h"

#include <algorithm>

namespace {
bool cacheModeUsesThresholdWindow(sd_cache_mode_t mode) {
	return mode == SD_CACHE_EASYCACHE ||
		mode == SD_CACHE_UCACHE ||
		mode == SD_CACHE_DBCACHE ||
		mode == SD_CACHE_TAYLORSEER ||
		mode == SD_CACHE_CACHE_DIT ||
		mode == SD_CACHE_SPECTRUM;
}
}

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlStableDiffusion video control frames");
	ofSetFrameRate(60);
	ofSetLogLevel(OF_LOG_WARNING);

	prompt = "A guided cinematic character walk cycle";
	negativePrompt = "blurry, low quality, distorted";
	modelPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_VACE_MODEL", "OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL", "OFXGGML_STABLE_DIFFUSION_MODEL"},
		{"models/wan2.1-vace-1.3b-q8_0.gguf", "models/video/wan2.1-vace-1.3b.gguf"});
	t5xxlPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER", "OFXGGML_STABLE_DIFFUSION_T5XXL"},
		{"models/umt5-xxl-encoder-Q8_0.gguf", "models/text/umt5-xxl-encoder-Q8_0.gguf"});
	vaePath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_VAE"},
		{"models/wan_2.1_vae.safetensors", "models/vae/wan_2.1_vae.safetensors"});
	controlFrameDir = ofToDataPath("control_frames");
	ofxGgmlStableDiffusionExampleCopyToInput(prompt, promptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(negativePrompt, negativePromptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(modelPath, modelPathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(t5xxlPath, t5xxlPathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(vaePath, vaePathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(controlFrameDir, controlFrameDirInput);
	statusMessage = "Ready";

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	configureContext();
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
		const auto capabilities = sd.getCapabilities();
		if (capabilities.imageToVideo || capabilities.videoAnimation) {
			modelSummary = "Context configured for guided video workflows.";
			statusMessage = "Model loaded";
		} else {
			const auto error = sd.getLastErrorInfo();
			modelSummary = "Load a VACE/video model before generating.";
			statusMessage = error.code == ofxGgmlStableDiffusionErrorCode::None ?
				"Model load failed" :
				"Error: " + error.message;
		}
	}

	if (wasGenerating && !generating) {
		if (sd.wasCancelled()) {
			statusMessage = "Video generation cancelled";
			return;
		}
		if (sd.hasVideoResult()) {
			const int outputCount = sd.getOutputCount();
			currentFrame = outputCount > 0 ? 0 : -1;
			if (currentFrame >= 0) {
				ofPixels pixels;
				if (sd.copyVideoFramePixels(currentFrame, pixels) && pixels.isAllocated()) {
					framePreview.setFromPixels(pixels);
				}
			}
			statusMessage = "Guided video ready: " + ofToString(outputCount) + " frames";
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
	ofBackground(22);
	drawFramePreview();

	if (!imGuiOk) {
		ofSetColor(255);
		ofDrawBitmapString(statusMessage, 20, 20);
		return;
	}

	gui.begin();
	ImGui::SetNextWindowSize(ImVec2(640.0f, 760.0f), ImGuiCond_Once);
	if (ImGui::Begin("Video Control Frames")) {
		ImGui::TextWrapped("%s", statusMessage.c_str());
		ImGui::TextWrapped("%s", modelSummary.c_str());
		ImGui::TextWrapped("%s", ofxGgmlStableDiffusionExampleRuntimeLabel(sd).c_str());
		if (generating) {
			ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
		}
		ImGui::Separator();

		const bool busy = sd.isBusy();
		if (busy) {
			ImGui::BeginDisabled();
		}
		if (ImGui::InputText("Diffusion model", modelPathInput.data(), modelPathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##diffusion")) {
			browseModelPath(modelPath, modelPathInput);
		}
		if (ImGui::InputText("UMT5 / T5XXL", t5xxlPathInput.data(), t5xxlPathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##t5xxl")) {
			browseModelPath(t5xxlPath, t5xxlPathInput);
		}
		if (ImGui::InputText("VAE", vaePathInput.data(), vaePathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##vae")) {
			browseModelPath(vaePath, vaePathInput);
		}
		if (ImGui::InputText("Control folder", controlFrameDirInput.data(), controlFrameDirInput.size())) {
			syncRequestFromUi();
		}
		if (ImGui::Button("Configure Context")) {
			configureContext();
		}
		if (busy) {
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Control Frames")) {
			loadControlFrames();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Control Frames")) {
			clearControlFrames();
		}
		const int loadedControlFrameCount = static_cast<int>(controlFrames.size());
		const bool controlFrameCountMismatch =
			loadedControlFrameCount > 0 && loadedControlFrameCount != frameCount;
		ImGui::Text("Loaded control frames: %s", ofToString(loadedControlFrameCount).c_str());
		if (controlFrameCountMismatch) {
			ImGui::TextWrapped("Warning: loaded frames do not match requested frames.");
			ImGui::SameLine();
			if (ImGui::Button("Use Loaded Count")) {
				frameCount = loadedControlFrameCount;
				statusMessage = "Frame count matched to loaded control frames";
			}
		}

		if (ImGui::InputTextMultiline("Prompt", promptInput.data(), promptInput.size(), ImVec2(-1.0f, 80.0f))) {
			syncRequestFromUi();
		}
		if (ImGui::InputTextMultiline("Negative", negativePromptInput.data(), negativePromptInput.size(), ImVec2(-1.0f, 52.0f))) {
			syncRequestFromUi();
		}

		ImGui::InputInt("Width", &width, 64, 128);
		ImGui::InputInt("Height", &height, 64, 128);
		ImGui::SliderInt("Frames", &frameCount, 1, 300);
		ImGui::SliderInt("FPS", &fps, 1, 60);
		ImGui::SliderInt("Steps", &sampleSteps, 1, 60);
		ImGui::SliderFloat("CFG", &cfgScale, 1.0f, 15.0f);
		ImGui::SliderFloat("Guidance", &guidance, 1.0f, 15.0f);
		ImGui::SliderFloat("Eta", &eta, 0.0f, 1.0f);
		ImGui::SliderFloat("Flow Shift", &flowShift, 0.0f, 12.0f);
		ImGui::SliderFloat("MoE Boundary", &moeBoundary, 0.0f, 1.0f);
		ImGui::SliderFloat("VACE Strength", &vaceStrength, 0.0f, 1.0f);
		ImGui::InputInt("Seed", &seed);

		ImGui::Checkbox("High-noise overrides", &useHighNoiseOverrides);
		if (useHighNoiseOverrides) {
			ImGui::InputInt("High-noise Steps", &highNoiseSampleSteps);
			ImGui::SliderFloat("High-noise CFG", &highNoiseCfgScale, 1.0f, 15.0f);
			ImGui::SliderFloat("High-noise Guidance", &highNoiseGuidance, 1.0f, 15.0f);
			ImGui::SliderFloat("High-noise Eta", &highNoiseEta, 0.0f, 1.0f);
			ImGui::SliderFloat("High-noise Flow Shift", &highNoiseFlowShift, 0.0f, 12.0f);
		}

		ImGui::InputInt("Cache Mode", &cacheModeIndex);
		cacheModeIndex = ofClamp(cacheModeIndex, 0, 6);
		ImGui::Text("Cache: %s", ofxGgmlStableDiffusionCacheModeName(selectedCacheMode()));
		if (cacheModeUsesThresholdWindow(selectedCacheMode())) {
			ImGui::SliderFloat("Cache Threshold", &cacheThreshold, 0.0f, 1.0f);
			ImGui::SliderFloat("Cache Start", &cacheStartPercent, 0.0f, 0.99f);
			ImGui::SliderFloat("Cache End", &cacheEndPercent, 0.01f, 1.0f);
		}

		const bool canGenerate = sd.hasLoadedContext() && !generating && !busy;
		if (generating) {
			ImGui::BeginDisabled();
		}
		if (!canGenerate) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Generate Guided Video")) {
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

		const bool hasVideo = sd.hasVideoResult();
		if (!hasVideo) {
			ImGui::BeginDisabled();
		}
		if (hasVideo) {
			const int maxFrame = std::max(0, sd.getOutputCount() - 1);
			if (ImGui::SliderInt("Preview Frame", &currentFrame, 0, maxFrame)) {
				ofPixels pixels;
				if (sd.copyVideoFramePixels(currentFrame, pixels) && pixels.isAllocated()) {
					framePreview.setFromPixels(pixels);
				}
			}
		}
		if (ImGui::Button("Save Frames")) {
			saveFrames();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save Video")) {
			saveVideo();
		}
		if (!hasVideo) {
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
	t5xxlPath = ofxGgmlStableDiffusionExampleInputString(t5xxlPathInput);
	vaePath = ofxGgmlStableDiffusionExampleInputString(vaePathInput);
	controlFrameDir = ofxGgmlStableDiffusionExampleInputString(controlFrameDirInput);
}

//--------------------------------------------------------------
void ofApp::configureContext() {
	if (sd.isBusy()) {
		statusMessage = "Stable Diffusion is busy";
		return;
	}
	syncRequestFromUi();
	ofxGgmlStableDiffusionContextSettings settings;
	settings.diffusionModelPath = modelPath;
	settings.t5xxlPath = t5xxlPath;
	settings.vaePath = vaePath;
	settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	settings.diffusionFlashAttn = true;
	settings.enableMmap = false;
	sd.configureContext(settings);
	const auto capabilities = sd.getCapabilities();
	contextLoading = sd.isBusy();
	if (contextLoading) {
		modelSummary = "Loading video model...";
		statusMessage = "Loading model...";
	} else {
		modelSummary = capabilities.imageToVideo || capabilities.videoAnimation ?
			"Context configured for guided video workflows." :
			"Load a VACE/video model before generating.";
		statusMessage = (capabilities.imageToVideo || capabilities.videoAnimation) ?
			"Context configured" :
			"Model not loaded";
		if (!(capabilities.imageToVideo || capabilities.videoAnimation)) {
			const auto error = sd.getLastErrorInfo();
			if (error.code != ofxGgmlStableDiffusionErrorCode::None) {
				statusMessage = "Error: " + error.message;
			}
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
void ofApp::loadControlFrames() {
	syncRequestFromUi();
	controlImagePreviews.clear();
	controlPixels.clear();
	controlFrames.clear();

	ofDirectory dir(controlFrameDir);
	if (!dir.exists()) {
		statusMessage = "Control frame folder not found";
		return;
	}
	for (const auto& extension : {"png", "jpg", "jpeg", "bmp"}) {
		dir.allowExt(extension);
	}
	dir.listDir();
	for (std::size_t i = 0; i < dir.size(); ++i) {
		const ofFile& file = dir.getFile(static_cast<int>(i));
		if (!file.isFile()) {
			continue;
		}
		ofImage image;
		if (!image.load(file.getAbsolutePath())) {
			ofLogWarning("ofxGgmlStableDiffusionVideoControlFramesExample")
				<< "Failed to load control frame: " << file.getAbsolutePath();
			continue;
		}
		image.resize(width, height);
		controlImagePreviews.push_back(image);
		controlPixels.push_back(image.getPixels());
	}
	rebuildControlFrameViews();
	statusMessage = "Loaded control frames: " + ofToString(controlFrames.size());
}

//--------------------------------------------------------------
void ofApp::clearControlFrames() {
	controlImagePreviews.clear();
	controlPixels.clear();
	controlFrames.clear();
	statusMessage = "Control frames cleared";
}

//--------------------------------------------------------------
void ofApp::startGeneration() {
	if (generating) {
		return;
	}
	if (!sd.hasLoadedContext()) {
		statusMessage = sd.isBusy() ? "Model is still loading." : "Load a video model before generating.";
		return;
	}
	syncRequestFromUi();
	if (controlFrames.empty()) {
		statusMessage = "Load control frames first.";
		return;
	}

	ofxGgmlStableDiffusionVideoRequest request;
	request.controlFrames = controlFrames;
	request.prompt = prompt;
	request.negativePrompt = negativePrompt;
	request.width = width;
	request.height = height;
	request.frameCount = frameCount;
	request.fps = fps;
	request.sampleSteps = sampleSteps;
	request.cfgScale = cfgScale;
	request.guidance = guidance;
	request.eta = eta;
	request.flowShift = flowShift;
	request.moeBoundary = moeBoundary;
	request.vaceStrength = vaceStrength;
	request.seed = seed;
	request.useHighNoiseOverrides = useHighNoiseOverrides;
	if (useHighNoiseOverrides) {
		request.highNoiseSampleSteps = highNoiseSampleSteps;
		request.highNoiseCfgScale = highNoiseCfgScale;
		request.highNoiseGuidance = highNoiseGuidance;
		request.highNoiseEta = highNoiseEta;
		request.highNoiseFlowShift = highNoiseFlowShift;
	}
	request.cache.mode = selectedCacheMode();
	if (cacheModeUsesThresholdWindow(request.cache.mode)) {
		request.cache.reuse_threshold = cacheThreshold;
		request.cache.start_percent = ofClamp(cacheStartPercent, 0.0f, 0.99f);
		request.cache.end_percent = ofClamp(
			std::max(cacheEndPercent, request.cache.start_percent + 0.01f),
			0.01f,
			1.0f);
	}

	progress.store(0.0f);
	framePreview.clear();
	statusMessage = "Generating guided video...";
	sd.generateVideo(request);
}

//--------------------------------------------------------------
void ofApp::cancelGeneration() {
	ofxGgmlStableDiffusionExampleRequestCancel(sd, statusMessage);
}

//--------------------------------------------------------------
void ofApp::saveFrames() {
	const std::string directory = ofToDataPath(
		ofGetTimestampString("output/ofxGgmlStableDiffusion-control-video-%Y-%m-%d-%H-%M-%S"),
		true);
	if (sd.saveVideoFramesWithMetadata(directory, "frame", "metadata.json")) {
		statusMessage = "Saved frames to " + directory;
	} else {
		statusMessage = "Save frames failed";
	}
}

//--------------------------------------------------------------
void ofApp::saveVideo() {
	const std::string path = ofToDataPath(
		ofGetTimestampString("output/ofxGgmlStableDiffusion-control-video-%Y-%m-%d-%H-%M-%S.webm"),
		true);
	if (sd.saveVideoWebm(path)) {
		statusMessage = "Saved video to " + path;
	} else {
		statusMessage = "Save video failed";
	}
}

//--------------------------------------------------------------
void ofApp::drawFramePreview() {
	ofxGgmlStableDiffusionExampleDrawImageFit(framePreview);
}

//--------------------------------------------------------------
sd_cache_mode_t ofApp::selectedCacheMode() const {
	const sd_cache_mode_t modes[] = {
		SD_CACHE_DISABLED,
		SD_CACHE_EASYCACHE,
		SD_CACHE_UCACHE,
		SD_CACHE_DBCACHE,
		SD_CACHE_TAYLORSEER,
		SD_CACHE_CACHE_DIT,
		SD_CACHE_SPECTRUM
	};
	const int index = std::max(0, std::min(cacheModeIndex, 6));
	return modes[index];
}

//--------------------------------------------------------------
void ofApp::rebuildControlFrameViews() {
	controlFrames.clear();
	controlFrames.reserve(controlPixels.size());
	for (auto& pixels : controlPixels) {
		controlFrames.push_back(ofxGgmlStableDiffusionExampleImageView(pixels));
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
