#include "ofApp.h"

#include "core/ofxGgmlStableDiffusionParameterTuningHelpers.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

const std::string kModelPathSettingsFile = "settings/video-generation-model-paths.json";

const char* kPromptPresetNames[] = {
	"Neon night market",
	"Botanical glasshouse",
	"Clockwork coast",
	"Input-image drift",
	"First-last-frame morph"
};

const char* kExportFormatNames[] = {
	"Animated WebP",
	"WebM (experimental)",
	"AVI (fallback)"
};

const char* kExportFormatExtensions[] = {
	"webp",
	"webm",
	"avi"
};

bool isContextSmokeEnabled() {
	std::string value = ofGetEnv("OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE");
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value == "1" || value == "true" || value == "yes";
}

bool isAutoLoadEnabled() {
	std::string value = ofGetEnv("OFXGGML_STABLE_DIFFUSION_AUTO_LOAD");
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

bool hasEnvOverride(std::initializer_list<std::string> names) {
	for (const auto& name : names) {
		if (!ofGetEnv(name).empty()) {
			return true;
		}
	}
	return false;
}

std::string jsonStringValue(const std::string& json, const std::string& key) {
	const std::string quotedKey = "\"" + key + "\"";
	const auto keyPosition = json.find(quotedKey);
	if (keyPosition == std::string::npos) {
		return "";
	}
	const auto colonPosition = json.find(':', keyPosition + quotedKey.size());
	if (colonPosition == std::string::npos) {
		return "";
	}
	const auto quotePosition = json.find('"', colonPosition + 1);
	if (quotePosition == std::string::npos) {
		return "";
	}

	std::string value;
	bool escaping = false;
	for (std::size_t i = quotePosition + 1; i < json.size(); ++i) {
		const char c = json[i];
		if (escaping) {
			switch (c) {
			case '"':
			case '\\':
			case '/':
				value.push_back(c);
				break;
			case 'n':
				value.push_back('\n');
				break;
			case 'r':
				value.push_back('\r');
				break;
			case 't':
				value.push_back('\t');
				break;
			default:
				value.push_back(c);
				break;
			}
			escaping = false;
			continue;
		}
		if (c == '\\') {
			escaping = true;
			continue;
		}
		if (c == '"') {
			return value;
		}
		value.push_back(c);
	}
	return "";
}

bool savedFileLooksValid(const std::string& path, std::uint64_t& sizeBytes) {
	sizeBytes = 0;
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if (!input) {
		return false;
	}
	const auto size = input.tellg();
	if (size <= 0) {
		return false;
	}
	sizeBytes = static_cast<std::uint64_t>(size);
	return true;
}

std::string formatByteCount(std::uint64_t bytes) {
	if (bytes >= 1024ull * 1024ull) {
		return ofToString(static_cast<float>(bytes) / (1024.0f * 1024.0f), 2) + " MB";
	}
	if (bytes >= 1024ull) {
		return ofToString(static_cast<float>(bytes) / 1024.0f, 1) + " KB";
	}
	return ofToString(bytes) + " B";
}

std::string enclosingDirectory(const std::string& path) {
	const auto separator = path.find_last_of("\\/");
	if (separator == std::string::npos) {
		return "";
	}
	return path.substr(0, separator);
}

void createDirectoriesForFile(const std::string& path) {
	const std::string directory = enclosingDirectory(path);
	if (directory.empty()) {
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(directory, error);
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

	prompt =
		"A rain-slick neon night market seen through a slow handheld dolly shot, "
		"paper lanterns swaying in the wind, steam rolling across the street, "
		"a glass koi lantern drifting past camera, cinematic reflections, natural motion";
	promptB =
		"The same neon night market moments later, lantern light blooming through mist, "
		"the camera glides closer as the glass koi lantern dissolves into sparks";
	negativePrompt = "blurry, low quality, distorted, jitter, warped hands, flicker, duplicate objects";
	modelPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL", "OFXGGML_STABLE_DIFFUSION_MODEL"},
		{"models/Wan2.1-T2V-1.3B-Q8_0.gguf", "models/video/wan2.1-t2v-1.3b.gguf"});
	t5xxlPath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER", "OFXGGML_STABLE_DIFFUSION_T5XXL"},
		{"models/umt5-xxl-encoder-Q8_0.gguf", "models/text/umt5-xxl-encoder-Q8_0.gguf"});
	vaePath = ofxGgmlStableDiffusionExampleEnvOrReadablePath(
		{"OFXGGML_STABLE_DIFFUSION_VAE"},
		{"models/wan_2.1_vae.safetensors", "models/vae/wan_2.1_vae.safetensors"});
	loadModelPathSettings();
	applyModelDefaults(false);
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

	if (isAutoLoadEnabled()) {
		configureContext();
	} else {
		statusMessage = "Ready";
	}
}

//--------------------------------------------------------------
void ofApp::update() {
	updateExportJob();
	updatePlayback();

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
			previewPlaying = false;
			lastPreviewFrameMillis = ofGetElapsedTimeMillis();
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
		const float durationSeconds =
			fps > 0 ? static_cast<float>(frameCount) / static_cast<float>(fps) : 0.0f;
		ImGui::Text(
			"Settings: %dx%d, %d frames @ %d fps (%.2fs), %d steps",
			width,
			height,
			frameCount,
			fps,
			durationSeconds,
			sampleSteps);
		if (generating) {
			ImGui::ProgressBar(progress.load(), ImVec2(-1.0f, 0.0f));
		}
		ImGui::Separator();

		const bool busy = exportInProgress || sd.isBusy();
		if (busy) {
			ImGui::BeginDisabled();
		}
		if (ImGui::InputText("Diffusion model", modelPathInput.data(), modelPathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##diffusion")) {
			browseModelPath(modelPath, modelPathInput, true);
		}
		if (ImGui::InputText("UMT5 / T5XXL", t5xxlPathInput.data(), t5xxlPathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##t5xxl")) {
			browseModelPath(t5xxlPath, t5xxlPathInput, true);
		}
		if (ImGui::InputText("VAE", vaePathInput.data(), vaePathInput.size())) {
			syncRequestFromUi();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##vae")) {
			browseModelPath(vaePath, vaePathInput, true);
		}
		if (ImGui::Button("Configure Context")) {
			configureContext();
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply Model Defaults")) {
			syncRequestFromUi();
			applyModelDefaults(true);
			statusMessage = "Applied model defaults";
		}
		if (busy) {
			ImGui::EndDisabled();
		}
		ImGui::Separator();

		if (ImGui::Combo(
				"Prompt Preset",
				&promptPresetIndex,
				kPromptPresetNames,
				static_cast<int>(std::size(kPromptPresetNames)))) {
			applyPromptPreset(promptPresetIndex);
		}
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
		if (ImGui::Button("Browse Image...")) {
			if (browseImagePath("Select video start image", imagePath, imagePathInput)) {
				useInputImage = true;
				loadInputImage();
			}
		}
		ImGui::SameLine();
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
		if (ImGui::Button("Browse End Frame...")) {
			if (browseImagePath("Select video end frame", endFramePath, endFramePathInput)) {
				useEndFrame = true;
				loadEndFrame();
			}
		}
		ImGui::SameLine();
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

		const bool hasVideo = !exportInProgress && sd.hasVideoResult();
		if (!hasVideo) {
			ImGui::BeginDisabled();
		}
		if (hasVideo) {
			const int maxFrame = std::max(0, sd.getOutputCount() - 1);
			if (ImGui::Button(previewPlaying ? "Pause" : "Play")) {
				previewPlaying = !previewPlaying;
				lastPreviewFrameMillis = ofGetElapsedTimeMillis();
			}
			ImGui::SameLine();
			if (ImGui::SliderInt("Preview Frame", &currentFrame, 0, maxFrame)) {
				previewPlaying = false;
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
		ImGui::Combo(
			"##export-format",
			&exportFormatIndex,
			kExportFormatNames,
			static_cast<int>(std::size(kExportFormatNames)));
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
void ofApp::loadModelPathSettings() {
	const std::string path = ofToDataPath(kModelPathSettingsFile, true);
	if (!ofFile::doesFileExist(path)) {
		return;
	}

	try {
		std::ifstream input(path);
		const std::string json(
			(std::istreambuf_iterator<char>(input)),
			std::istreambuf_iterator<char>());
		if (!hasEnvOverride({"OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL", "OFXGGML_STABLE_DIFFUSION_MODEL"})) {
			const std::string saved = jsonStringValue(json, "diffusionModelPath");
			if (!saved.empty()) {
				modelPath = saved;
			}
		}
		if (!hasEnvOverride({"OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER", "OFXGGML_STABLE_DIFFUSION_T5XXL"})) {
			const std::string saved = jsonStringValue(json, "t5xxlPath");
			if (!saved.empty()) {
				t5xxlPath = saved;
			}
		}
		if (!hasEnvOverride({"OFXGGML_STABLE_DIFFUSION_VAE"})) {
			const std::string saved = jsonStringValue(json, "vaePath");
			if (!saved.empty()) {
				vaePath = saved;
			}
		}
	} catch (const std::exception& e) {
		ofLogWarning("ofxGgmlStableDiffusionVideoGenerationExample")
			<< "Could not load saved model paths: " << e.what();
	}
}

//--------------------------------------------------------------
void ofApp::saveModelPathSettings() const {
	const std::string path = ofToDataPath(kModelPathSettingsFile, true);
	createDirectoriesForFile(path);

	ofJson json;
	json["diffusionModelPath"] = modelPath;
	json["t5xxlPath"] = t5xxlPath;
	json["vaePath"] = vaePath;
	json["updatedAt"] = ofGetTimestampString("%Y-%m-%dT%H:%M:%S");

	if (!ofSaveJson(path, json)) {
		ofLogWarning("ofxGgmlStableDiffusionVideoGenerationExample")
			<< "Could not save model paths to " << path;
	}
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionContextSettings ofApp::makeCurrentContextSettings() const {
	ofxGgmlStableDiffusionContextSettings settings;
	settings.diffusionModelPath = modelPath;
	settings.t5xxlPath = t5xxlPath;
	settings.vaePath = vaePath;
	return settings;
}

//--------------------------------------------------------------
void ofApp::applyModelDefaults(bool updatePrompt) {
	const auto settings = makeCurrentContextSettings();
	const auto profile =
		ofxGgmlStableDiffusionParameterTuningHelpers::resolveVideoProfile(settings);

	width = profile.defaultWidth;
	height = profile.defaultHeight;
	frameCount = profile.defaultFrameCount;
	fps = profile.defaultFps;
	sampleSteps = profile.defaultSampleSteps;
	cfgScale = profile.defaultCfgScale;
	strength = profile.defaultStrength;
	vaceStrength = profile.defaultVaceStrength;
	previewFps = std::max(1, fps);

	const auto descriptor = ofxGgmlStableDiffusionExampleLower(
		modelPath + " " + t5xxlPath + " " + vaePath);
	if (descriptor.find("wan") != std::string::npos) {
		guidance = 5.0f;
		flowShift = 5.0f;
		moeBoundary = 0.875f;
	}

	modelSummary = profile.summary;
	if (updatePrompt) {
		if (descriptor.find("i2v") != std::string::npos) {
			applyPromptPreset(3);
		} else if (descriptor.find("flf2v") != std::string::npos) {
			applyPromptPreset(4);
		} else {
			applyPromptPreset(0);
		}
	}
}

//--------------------------------------------------------------
void ofApp::applyPromptPreset(int presetIndex) {
	promptPresetIndex = std::max(0, std::min(
		presetIndex,
		static_cast<int>(std::size(kPromptPresetNames)) - 1));
	switch (promptPresetIndex) {
	case 1:
		prompt =
			"A cinematic macro tracking shot through a midnight botanical glasshouse, "
			"bioluminescent orchids slowly opening, condensation sliding down glass, "
			"tiny maintenance drones weaving through mist, elegant shallow depth of field";
		promptB =
			"The glasshouse warms with sunrise color, orchids glow brighter, mist thins, "
			"the camera floats past leaves covered in sparkling droplets";
		break;
	case 2:
		prompt =
			"A lonely clockwork lighthouse on a stormy copper coastline, gears turning under glass, "
			"waves striking black rocks in slow motion, brass birds circling the beacon, cinematic camera orbit";
		promptB =
			"The lighthouse beacon catches the storm clouds and turns them gold, "
			"waves pull back as the brass birds sweep past camera";
		break;
	case 3:
		prompt =
			"Animate the input image with subtle parallax and believable camera drift, "
			"soft wind moving fabric and hair, cinematic light changes, coherent subject identity";
		promptB =
			"The same scene settles into a calmer final beat, camera eases forward, "
			"ambient light warms, motion remains natural and stable";
		break;
	case 4:
		prompt =
			"A surreal museum corridor transforms between the first and last frame, "
			"paintings breathing softly, floor reflections stretching, slow cinematic push-in";
		promptB =
			"The corridor reaches the final frame as reflections calm and the paintings glow, "
			"motion smooth, no abrupt cuts";
		break;
	case 0:
	default:
		prompt =
			"A rain-slick neon night market seen through a slow handheld dolly shot, "
			"paper lanterns swaying in the wind, steam rolling across the street, "
			"a glass koi lantern drifting past camera, cinematic reflections, natural motion";
		promptB =
			"The same neon night market moments later, lantern light blooming through mist, "
			"the camera glides closer as the glass koi lantern dissolves into sparks";
		break;
	}
	negativePrompt = "blurry, low quality, distorted, jitter, warped hands, flicker, duplicate objects";
	ofxGgmlStableDiffusionExampleCopyToInput(prompt, promptInput);
	ofxGgmlStableDiffusionExampleCopyToInput(promptB, promptBInput);
	ofxGgmlStableDiffusionExampleCopyToInput(negativePrompt, negativePromptInput);
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
	saveModelPathSettings();
	ofxGgmlStableDiffusionContextSettings settings = makeCurrentContextSettings();
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
void ofApp::browseModelPath(std::string& path, std::array<char, 512>& input, bool persistModelPaths) {
	ofFileDialogResult result = ofSystemLoadDialog("Select model file");
	if (!result.bSuccess) {
		return;
	}
	path = result.getPath();
	ofxGgmlStableDiffusionExampleCopyToInput(path, input);
	if (persistModelPaths) {
		syncRequestFromUi();
		applyModelDefaults(false);
		saveModelPathSettings();
	}
	statusMessage = "Selected model path";
}

//--------------------------------------------------------------
bool ofApp::browseImagePath(
	const std::string& title,
	std::string& path,
	std::array<char, 512>& input) {
	ofFileDialogResult result = ofSystemLoadDialog(title, false, path);
	if (!result.bSuccess) {
		return false;
	}
	path = result.getPath();
	ofxGgmlStableDiffusionExampleCopyToInput(path, input);
	return true;
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
	saveModelPathSettings();
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
	previewFps = std::max(1, request.fps);
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
	if (exportInProgress) {
		statusMessage = "Export already in progress";
		return;
	}

	const std::string directory = ofToDataPath(
		ofGetTimestampString("output/ofxGgmlStableDiffusion-video-%Y-%m-%d-%H-%M-%S"),
		true);
	const auto clip = sd.getVideoClip();
	if (clip.empty()) {
		statusMessage = "No video to save";
		return;
	}
	statusMessage = "Saving frames...";
	exportInProgress = true;
	exportFuture = std::async(std::launch::async, [clip, directory]() {
		const bool ok = clip.saveFrameSequenceWithMetadata(directory, "frame", "metadata.json");
		return ExportJobResult{
			ok,
			ok ? "Saved frames to " + directory : "Save frames failed"
		};
	});
}

//--------------------------------------------------------------
void ofApp::saveVideo() {
	if (exportInProgress) {
		statusMessage = "Export already in progress";
		return;
	}

	exportFormatIndex = std::max(0, std::min(
		exportFormatIndex,
		static_cast<int>(std::size(kExportFormatExtensions)) - 1));
	const std::string extension = kExportFormatExtensions[exportFormatIndex];
	const std::string path = ofToDataPath(
		ofGetTimestampString("output/ofxGgmlStableDiffusion-video-%Y-%m-%d-%H-%M-%S." + extension),
		true);
	const auto clip = sd.getVideoClip();
	if (clip.empty()) {
		statusMessage = "No video to save";
		return;
	}
	statusMessage = "Saving video...";
	exportInProgress = true;
	exportFuture = std::async(std::launch::async, [clip, path, extension]() {
		const bool encoded = clip.saveWebm(path);
		std::uint64_t sizeBytes = 0;
		const bool ok = encoded && savedFileLooksValid(path, sizeBytes);
		return ExportJobResult{
			ok,
			ok ?
				"Saved " + extension + " video (" + formatByteCount(sizeBytes) + ") to " + path :
				"Save " + extension + " video failed"
		};
	});
}

//--------------------------------------------------------------
void ofApp::updatePlayback() {
	if (!previewPlaying || exportInProgress || !sd.hasVideoResult()) {
		return;
	}

	const int outputCount = sd.getOutputCount();
	if (outputCount <= 0) {
		previewPlaying = false;
		return;
	}

	const uint64_t now = ofGetElapsedTimeMillis();
	const int playbackFps = std::max(1, previewFps);
	const uint64_t frameIntervalMillis =
		std::max<uint64_t>(1, 1000 / static_cast<uint64_t>(playbackFps));
	if (lastPreviewFrameMillis != 0 && now - lastPreviewFrameMillis < frameIntervalMillis) {
		return;
	}

	lastPreviewFrameMillis = now;
	currentFrame = (currentFrame + 1) % outputCount;
	ofPixels pixels;
	if (sd.copyVideoFramePixels(currentFrame, pixels) && pixels.isAllocated()) {
		framePreview.setFromPixels(pixels);
	}
}

//--------------------------------------------------------------
void ofApp::updateExportJob() {
	if (!exportInProgress || !exportFuture.valid()) {
		return;
	}

	if (exportFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
		return;
	}

	const ExportJobResult result = exportFuture.get();
	exportInProgress = false;
	statusMessage = result.message;
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
