#include "ofApp.h"

#include <algorithm>

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlStableDiffusion creative loop");
	ofSetFrameRate(60);
	ofSetLogLevel(OF_LOG_WARNING);

	prompt = "A luminous abstract landscape, cinematic, painterly";
	negativePrompt = "blurry, low quality, distorted";
	std::copy(prompt.begin(), prompt.end(), promptInput.begin());
	std::copy(negativePrompt.begin(), negativePrompt.end(), negativePromptInput.begin());
	statusMessage = "Ready";

	auto window = ofGetCurrentWindow();
	const auto setupState = gui.setup(window, nullptr, true, ImGuiConfigFlags_None, true);
	if (!(setupState & ofxImGui::SetupState::Success)) {
		imGuiOk = false;
		statusMessage = "ImGui setup failed";
		return;
	}

	configureContext();
	loop.setFrameCallback([this](const ofxGgmlStableDiffusionRealtimeVideoFrame& frame) {
		if (!frame.frame.pixels.isAllocated()) {
			return;
		}
		previewImage.setFromPixels(frame.frame.pixels);
		lastLatencyMs = frame.latencyMs;
		lastFrameIndex = frame.frameIndex;
		lastQuality = frame.quality;
		statusMessage =
			std::string("Generated ") +
			ofxGgmlStableDiffusionRealtimeVideoQualityLabel(frame.quality) +
			" frame " +
			ofToString(frame.frameIndex + 1);
	});
	loop.setLatencyCallback([this](
		float latencyMs,
		ofxGgmlStableDiffusionRealtimeVideoQuality quality) {
		lastLatencyMs = latencyMs;
		lastQuality = quality;
	});
}

//--------------------------------------------------------------
void ofApp::update() {
	if (contextLoading && !sd.isBusy()) {
		contextLoading = false;
		if (sd.hasLoadedContext()) {
			modelSummary = "Context configured. Creative loop uses preview/refine image requests.";
			statusMessage = "Model loaded";
		} else {
			const auto error = sd.getLastErrorInfo();
			modelSummary = "Place a Stable Diffusion image model in bin/data/models/ before running.";
			statusMessage = error.code == ofxGgmlStableDiffusionErrorCode::None ?
				"Model load failed" :
				"Error: " + error.message;
		}
	}
	if (loopRunning || loop.isGenerating() || loop.hasPendingRequest()) {
		loop.update();
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(22);
	drawPreview();

	if (!imGuiOk) {
		ofSetColor(255);
		ofDrawBitmapString(statusMessage, 20, 20);
		return;
	}

	gui.begin();
	ImGui::SetNextWindowSize(ImVec2(520.0f, 520.0f), ImGuiCond_Once);
	if (ImGui::Begin("Creative Loop")) {
		const auto stats = loop.getStats();
		ImGui::TextWrapped("%s", statusMessage.c_str());
		ImGui::TextWrapped("%s", modelSummary.c_str());
		ImGui::Text(
			"Frames %d | Preview %d | Refine %d | Latency %.1f ms | %s",
			stats.framesGenerated,
			stats.previewFrames,
			stats.refineFrames,
			lastLatencyMs,
			ofxGgmlStableDiffusionRealtimeVideoQualityLabel(lastQuality));
		if (stats.coalescedUpdates > 0 || stats.droppedUpdates > 0) {
			ImGui::Text(
				"Queued %d | Coalesced %d | Dropped %d",
				stats.promptUpdates,
				stats.coalescedUpdates,
				stats.droppedUpdates);
		}
		ImGui::Separator();

		if (ImGui::InputTextMultiline("Prompt", promptInput.data(), promptInput.size(), ImVec2(-1.0f, 90.0f))) {
			syncRequestFromUi();
		}
		if (ImGui::InputTextMultiline("Negative", negativePromptInput.data(), negativePromptInput.size(), ImVec2(-1.0f, 54.0f))) {
			syncRequestFromUi();
		}
		ImGui::InputInt("Width", &width, 64, 128);
		ImGui::InputInt("Height", &height, 64, 128);
		ImGui::SliderInt("Preview Steps", &previewSteps, 1, 12);
		ImGui::SliderInt("Refine Steps", &refineSteps, 1, 48);
		refineSteps = std::max(previewSteps, refineSteps);
		ImGui::SliderFloat("Loop CFG", &cfgScale, 0.1f, 12.0f);
		ImGui::SliderFloat("Preview Strength", &previewStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("Refine Strength", &refineStrength, 0.0f, 1.0f);
		ImGui::InputInt("Seed", &seed);
		ImGui::Checkbox("Previous-frame feedback", &useFeedback);
		ImGui::SameLine();
		ImGui::Checkbox("Refine on idle", &refineOnIdle);
		ImGui::Checkbox("Lock seed", &lockSeed);
		ImGui::SameLine();
		ImGui::Checkbox("Drop while busy", &dropIfBusy);

		const bool canStartOrSubmit = sd.hasLoadedContext() && !sd.isBusy();
		if (!loopRunning && !canStartOrSubmit) {
			ImGui::BeginDisabled();
		}
		if (!loopRunning) {
			if (ImGui::Button("Start Loop")) {
				startLoop();
			}
		}
		if (!loopRunning && !canStartOrSubmit) {
			ImGui::EndDisabled();
		}
		if (loopRunning) {
			if (ImGui::Button("Stop Loop")) {
				stopLoop();
			}
		} else {
			ImGui::SameLine();
			if (!canStartOrSubmit) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("Send Prompt")) {
				submitPrompt();
			}
			if (!canStartOrSubmit) {
				ImGui::EndDisabled();
			}
		}
		if (loopRunning) {
			ImGui::SameLine();
			if (ImGui::Button("Send Prompt")) {
				submitPrompt();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Feedback")) {
			clearFeedback();
		}
	}
	ImGui::End();
	gui.end();
}

//--------------------------------------------------------------
void ofApp::syncRequestFromUi() {
	prompt = ofxGgmlStableDiffusionExampleInputString(promptInput);
	negativePrompt = ofxGgmlStableDiffusionExampleInputString(negativePromptInput);
}

//--------------------------------------------------------------
void ofApp::configureContext() {
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = ofToDataPath("models/sd_v1.5.safetensors");
	settings.weightType = SD_TYPE_COUNT;
	settings.nThreads = -1;
	sd.configureContext(settings);
	contextLoading = sd.isBusy();
	if (contextLoading) {
		modelSummary = "Loading image model...";
		statusMessage = "Loading model...";
	} else {
		modelSummary = sd.getCapabilities().contextConfigured ?
			"Context configured. Creative loop uses preview/refine image requests." :
			"Place a Stable Diffusion image model in bin/data/models/ before running.";
		if (!sd.getCapabilities().contextConfigured) {
			const auto error = sd.getLastErrorInfo();
			statusMessage = error.code == ofxGgmlStableDiffusionErrorCode::None ?
				"Model not loaded" :
				"Error: " + error.message;
		}
	}
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionRealtimeVideoSettings ofApp::buildLoopSettings() const {
	ofxGgmlStableDiffusionRealtimeVideoSettings settings;
	settings.previewWidth = width;
	settings.previewHeight = height;
	settings.previewSteps = previewSteps;
	settings.refineSteps = refineSteps;
	settings.previewStrength = previewStrength;
	settings.refineStrength = refineStrength;
	settings.cfgScale = cfgScale;
	settings.sampleMethod = EULER_A_SAMPLE_METHOD;
	settings.seed = seed;
	settings.lockSeed = lockSeed;
	settings.usePreviousFrameFeedback = useFeedback;
	settings.coalescePromptUpdates = true;
	settings.dropIfBusy = dropIfBusy;
	settings.enableRefineOnIdle = refineOnIdle;
	return settings;
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionRealtimeVideoRequest ofApp::buildLoopRequest() const {
	ofxGgmlStableDiffusionRealtimeVideoRequest request;
	request.prompt = prompt;
	request.negativePrompt = negativePrompt;
	request.width = width;
	request.height = height;
	request.previewSteps = previewSteps;
	request.refineSteps = refineSteps;
	request.previewStrength = previewStrength;
	request.refineStrength = refineStrength;
	request.cfgScale = cfgScale;
	request.sampleMethod = EULER_A_SAMPLE_METHOD;
	request.seed = seed;
	request.lockSeed = lockSeed;
	return request;
}

//--------------------------------------------------------------
void ofApp::startLoop() {
	syncRequestFromUi();
	if (!sd.hasLoadedContext()) {
		statusMessage = sd.isBusy() ? "Model is still loading." : "Load a model before starting the loop.";
		return;
	}
	loopRunning = loop.start(buildLoopSettings(), sd);
	statusMessage = loopRunning ? "Creative loop started" : "Creative loop is already active";
	if (loopRunning) {
		submitPrompt();
	}
}

//--------------------------------------------------------------
void ofApp::stopLoop() {
	loop.stop();
	loopRunning = false;
	if (sd.requestCancellation()) {
		statusMessage = "Creative loop stopping...";
	} else {
		statusMessage = "Creative loop stopped";
	}
}

//--------------------------------------------------------------
void ofApp::submitPrompt() {
	syncRequestFromUi();
	if (!loopRunning) {
		startLoop();
		return;
	}
	statusMessage = loop.submit(buildLoopRequest()) ?
		"Queued latest prompt" :
		"Prompt was dropped";
}

//--------------------------------------------------------------
void ofApp::clearFeedback() {
	loop.clearFeedbackFrame();
	statusMessage = "Feedback frame cleared";
}

//--------------------------------------------------------------
void ofApp::drawPreview() {
	ofxGgmlStableDiffusionExampleDrawImageFit(previewImage);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == ' ') {
		submitPrompt();
	}
	if (key == 27) {
		stopLoop();
	}
	if (key == 'c' || key == 'C') {
		clearFeedback();
	}
}
