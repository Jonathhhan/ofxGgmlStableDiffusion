#include "ofApp.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

bool isContextSmokeEnabled() {
	std::string value = ofGetEnv("OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE");
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value == "1" || value == "true" || value == "yes";
}

uint64_t readContextSmokeTimeoutMillis() {
	const std::string value =
		ofGetEnv("OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_TIMEOUT_MS");
	if (value.empty()) {
		return 900000;
	}
	try {
		return std::max<uint64_t>(1000, std::stoull(value));
	} catch (const std::exception&) {
		return 900000;
	}
}

void writeContextSmokeStatus(const std::string& message) {
	const std::string path =
		ofGetEnv("OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_STATUS");
	if (path.empty()) {
		return;
	}
	std::ofstream output(path, std::ios::app);
	output << message << '\n';
}

} // namespace

//--------------------------------------------------------------
void ofApp::setup() {
	writeContextSmokeStatus("setup:start");
	ofSetWindowTitle("ofxGgmlStableDiffusion video generation");
	ofSetFrameRate(60);
	contextSmoke = isContextSmokeEnabled();
	contextSmokeStartMillis = ofGetElapsedTimeMillis();
	contextSmokeTimeoutMillis = readContextSmokeTimeoutMillis();
	ofSetLogLevel(contextSmoke ? OF_LOG_NOTICE : OF_LOG_WARNING);

	prompt = "A cinematic ocean cliff at sunrise, slow camera drift";
	promptB = "A cinematic ocean cliff at sunset, glowing clouds";
	negativePrompt = "blurry, low quality, distorted";
	modelPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL", "OFXGGML_STABLE_DIFFUSION_MODEL"},
		{"models/Wan2.1-T2V-1.3B-Q8_0.gguf", "models/video/wan2.1-t2v-1.3b.gguf"});
	t5xxlPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER", "OFXGGML_STABLE_DIFFUSION_T5XXL"},
		{"models/umt5-xxl-encoder-Q8_0.gguf", "models/text/umt5-xxl-encoder-Q8_0.gguf"});
	vaePath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_VAE"},
		{"models/wan_2.1_vae.safetensors", "models/vae/wan_2.1_vae.safetensors"});
	ofxGgmlStableDiffusionExampleCopyToInput(prompt, promptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(promptB, promptBInput);
	ofxGgmlStableDiffusionExampleCopyToInput(negativePrompt, negativePromptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(modelPath, modelPathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(t5xxlPath, t5xxlPathInput);
	ofxGgmlStableDiffusionExampleCopyToInput(vaePath, vaePathInput);
	statusMessage = "Ready";

	sd.setProgressCallback([this](int step, int steps, float time) {
		const float value = steps > 0 ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
		progress.store(value);
	});

	if (contextSmoke) {
		imGuiOk = false;
		writeContextSmokeStatus("setup:context-smoke-configure");
		configureContext();
		writeContextSmokeStatus("setup:context-smoke-configured");
		return;
	}

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	configureContext();
}

//--------------------------------------------------------------
void ofApp::update() {
	const bool wasGenerating = generating;
	generating = sd.isGenerating();

	if (contextLoading && !sd.isBusy()) {
		contextLoading = false;
		const auto capabilities = sd.getCapabilities();
		if (capabilities.imageToVideo) {
			modelSummary = "Model advertises image-to-video support.";
			statusMessage = "Model loaded";
		} else {
			const auto error = sd.getLastErrorInfo();
			modelSummary = "Load a WAN/video model before generating.";
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
			statusMessage = "Video ready: " + ofToString(outputCount) + " frames";
			return;
		}

		const auto error = sd.getLastErrorInfo();
		if (error.code != ofxGgmlStableDiffusionErrorCode::None) {
			statusMessage = "Error: " + error.message;
		}
	}

	updateContextSmoke();
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(24);
	drawFramePreview();

	if (!imGuiOk) {
		ofSetColor(255);
		ofDrawBitmapString(statusMessage, 20, 20);
		return;
	}

	gui.begin();
	ImGui::SetNextWindowSize(ImVec2(620.0f, 720.0f), ImGuiCond_Once);
	if (ImGui::Begin("Video Generation")) {
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
		if (ImGui::Button("Configure Context")) {
			configureContext();
		}
		if (busy) {
			ImGui::EndDisabled();
		}
		ImGui::Separator();

		if (ImGui::InputTextMultiline("Prompt", promptInput.data(), promptInput.size(), ImVec2(-1.0f, 84.0f))) {
			syncRequestFromUi();
		}
		ImGui::Checkbox("Image-sequence prompt morph", &enablePromptInterpolation);
		if (enablePromptInterpolation) {
			if (ImGui::InputTextMultiline("End Prompt", promptBInput.data(), promptBInput.size(), ImVec2(-1.0f, 64.0f))) {
				syncRequestFromUi();
			}
		}
		if (ImGui::InputTextMultiline("Negative", negativePromptInput.data(), negativePromptInput.size(), ImVec2(-1.0f, 54.0f))) {
			syncRequestFromUi();
		}
		ImGui::Checkbox("Use input image", &useInputImage);
		if (ImGui::InputText("Image path", imagePathInput.data(), imagePathInput.size())) {
			syncRequestFromUi();
		}
		if (ImGui::Button("Load Image")) {
			loadInputImage();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Image")) {
			clearInputImage();
		}
		ImGui::Checkbox("Use end frame", &useEndFrame);
		if (ImGui::InputText("End frame path", endFramePathInput.data(), endFramePathInput.size())) {
			syncRequestFromUi();
		}
		if (ImGui::Button("Load End Frame")) {
			loadEndFrame();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear End Frame")) {
			clearEndFrame();
		}

		ImGui::InputInt("Width", &width, 64, 128);
		ImGui::InputInt("Height", &height, 64, 128);
		ImGui::SliderInt("Frames", &frameCount, 1, 300);
		ImGui::SliderInt("FPS", &fps, 1, 60);
		ImGui::SliderInt("Steps", &sampleSteps, 1, 60);
		ImGui::SliderFloat("CFG", &cfgScale, 1.0f, 15.0f);
		ImGui::SliderFloat("Guidance", &guidance, 1.0f, 15.0f);
		ImGui::SliderFloat("Strength", &strength, 0.0f, 1.0f);
		ImGui::SliderFloat("Eta", &eta, 0.0f, 1.0f);
		ImGui::SliderFloat("Flow Shift", &flowShift, 0.0f, 12.0f);
		ImGui::SliderFloat("MoE Boundary", &moeBoundary, 0.0f, 1.0f);
		ImGui::SliderFloat("VACE Strength", &vaceStrength, 0.0f, 1.0f);
		ImGui::InputInt("Seed", &seed);
		ImGui::Checkbox("Image-sequence seed sweep", &useSeedSequence);
		if (useSeedSequence) {
			ImGui::InputInt("Seed Increment", &seedIncrement);
		}

		const bool canGenerate = sd.hasLoadedContext() && !generating && !busy;
		if (generating) {
			ImGui::BeginDisabled();
		}
		if (!canGenerate) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Generate Video")) {
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
	promptB = ofxGgmlStableDiffusionExampleInputString(promptBInput);
	negativePrompt = ofxGgmlStableDiffusionExampleInputString(negativePromptInput);
	modelPath = ofxGgmlStableDiffusionExampleInputString(modelPathInput);
	t5xxlPath = ofxGgmlStableDiffusionExampleInputString(t5xxlPathInput);
	vaePath = ofxGgmlStableDiffusionExampleInputString(vaePathInput);
	imagePath = ofxGgmlStableDiffusionExampleInputString(imagePathInput);
	endFramePath = ofxGgmlStableDiffusionExampleInputString(endFramePathInput);
}

//--------------------------------------------------------------
void ofApp::configureContext() {
	writeContextSmokeStatus("configure:start");
	if (sd.isBusy()) {
		statusMessage = "Stable Diffusion is busy";
		writeContextSmokeStatus("configure:busy");
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
	writeContextSmokeStatus("configure:submitted");
	const auto capabilities = sd.getCapabilities();
	contextLoading = sd.isBusy();
	if (contextLoading) {
		modelSummary = "Loading video model...";
		statusMessage = "Loading model...";
	} else {
		modelSummary = capabilities.imageToVideo ?
			"Model advertises image-to-video support." :
			"Load a WAN/video model before generating.";
		if (!capabilities.imageToVideo) {
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
		statusMessage = sd.isBusy() ? "Model is still loading." : "Load a video model before generating.";
		return;
	}
	syncRequestFromUi();
	if (useInputImage && inputImage.data == nullptr) {
		statusMessage = "Load an input image or disable Use input image.";
		return;
	}
	if (useEndFrame && endFrame.data == nullptr) {
		statusMessage = "Load an end frame or disable Use end frame.";
		return;
	}

	ofxGgmlStableDiffusionVideoRequest request;
	request.initImage = useInputImage ? inputImage : sd_image_t{0, 0, 0, nullptr};
	request.endImage = useEndFrame ? endFrame : sd_image_t{0, 0, 0, nullptr};
	request.prompt = prompt;
	request.negativePrompt = negativePrompt;
	request.width = width;
	request.height = height;
	request.frameCount = frameCount;
	request.fps = fps;
	request.sampleSteps = sampleSteps;
	request.cfgScale = cfgScale;
	request.guidance = guidance;
	request.strength = strength;
	request.eta = eta;
	request.flowShift = flowShift;
	request.moeBoundary = moeBoundary;
	request.vaceStrength = vaceStrength;
	request.seed = seed;
	if (enablePromptInterpolation && !promptB.empty() && frameCount > 1) {
		request.animationSettings.enablePromptInterpolation = true;
		request.animationSettings.promptInterpolationMode =
			ofxGgmlStableDiffusionInterpolationMode::Smooth;
		request.animationSettings.promptKeyframes = {
			{0, prompt},
			{frameCount - 1, promptB}
		};
	}
	if (useSeedSequence) {
		request.animationSettings.useSeedSequence = true;
		request.animationSettings.seedIncrement = seedIncrement;
	}

	progress.store(0.0f);
	framePreview.clear();
	statusMessage = "Generating video...";
	sd.generateVideo(request);
}

//--------------------------------------------------------------
void ofApp::cancelGeneration() {
	ofxGgmlStableDiffusionExampleRequestCancel(sd, statusMessage);
}

//--------------------------------------------------------------
void ofApp::loadInputImage() {
	syncRequestFromUi();
	if (!ofxGgmlStableDiffusionExampleLoadImageView(
		imagePath,
		width,
		height,
		inputImagePreview,
		inputPixels,
		inputImage)) {
		statusMessage = "Input image load failed";
		return;
	}
	useInputImage = true;
	statusMessage = "Input image loaded";
}

//--------------------------------------------------------------
void ofApp::clearInputImage() {
	inputImagePreview.clear();
	inputPixels.clear();
	inputImage = {0, 0, 0, nullptr};
	useInputImage = false;
	statusMessage = "Input image cleared";
}

//--------------------------------------------------------------
void ofApp::loadEndFrame() {
	syncRequestFromUi();
	if (!ofxGgmlStableDiffusionExampleLoadImageView(
		endFramePath,
		width,
		height,
		endFramePreview,
		endFramePixels,
		endFrame)) {
		statusMessage = "End frame load failed";
		return;
	}
	useEndFrame = true;
	statusMessage = "End frame loaded";
}

//--------------------------------------------------------------
void ofApp::clearEndFrame() {
	endFramePreview.clear();
	endFramePixels.clear();
	endFrame = {0, 0, 0, nullptr};
	useEndFrame = false;
	statusMessage = "End frame cleared";
}

//--------------------------------------------------------------
void ofApp::saveFrames() {
	const std::string directory = ofToDataPath(
		ofGetTimestampString("output/ofxGgmlStableDiffusion-video-%Y-%m-%d-%H-%M-%S"),
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
		ofGetTimestampString("output/ofxGgmlStableDiffusion-video-%Y-%m-%d-%H-%M-%S.webm"),
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
void ofApp::keyPressed(int key) {
	if (key == ' ') {
		startGeneration();
	}
	if (key == 27 || key == 'c' || key == 'C') {
		cancelGeneration();
	}
}

//--------------------------------------------------------------
void ofApp::updateContextSmoke() {
	if (!contextSmoke) {
		return;
	}

	if (sd.isBusy() || contextLoading) {
		const uint64_t elapsed = ofGetElapsedTimeMillis() - contextSmokeStartMillis;
		if (elapsed > contextSmokeTimeoutMillis) {
			ofxGgmlStableDiffusionExampleRequestCancel(
				sd,
				statusMessage,
				"Context smoke timed out; cancelling load...");
			finishContextSmoke(2, "WAN context smoke timed out");
		}
		return;
	}

	if (sd.hasLoadedContext()) {
		finishContextSmoke(0, "WAN context smoke loaded successfully");
		return;
	}

	const auto error = sd.getLastErrorInfo();
	const std::string message = error.code == ofxGgmlStableDiffusionErrorCode::None ?
		"WAN context smoke failed without an addon error" :
		"WAN context smoke failed: " + error.message;
	finishContextSmoke(1, message);
}

//--------------------------------------------------------------
void ofApp::finishContextSmoke(int exitCode, const std::string& message) {
	contextSmoke = false;
	writeContextSmokeStatus("finish:" + ofToString(exitCode) + ":" + message);
#ifdef _WIN32
	TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exitCode));
#endif
	if (exitCode == 0) {
		ofLogNotice("ofxGgmlStableDiffusionVideoGenerationExample")
			<< message << " (" << modelPath << ")";
	} else {
		ofLogError("ofxGgmlStableDiffusionVideoGenerationExample")
			<< message;
	}
	ofExit(exitCode);
}
