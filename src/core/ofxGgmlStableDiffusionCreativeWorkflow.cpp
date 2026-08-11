#include "ofxGgmlStableDiffusionCreativeWorkflow.h"

#include "../ofxGgmlStableDiffusion.h"

#include <filesystem>

namespace {

bool pathIsSafeToRestore(const std::string& path) {
	if (path.empty()) {
		return true;
	}
	const std::filesystem::path fsPath(path);
	for (const auto& component : fsPath) {
		if (component == "..") {
			return false;
		}
	}
	return std::filesystem::exists(fsPath);
}

bool contextSettingsAreSafeToRestore(
	const ofxGgmlStableDiffusionContextSettings& settings) {
	return pathIsSafeToRestore(settings.modelPath) &&
		pathIsSafeToRestore(settings.diffusionModelPath) &&
		pathIsSafeToRestore(settings.clipLPath) &&
		pathIsSafeToRestore(settings.clipGPath) &&
		pathIsSafeToRestore(settings.t5xxlPath) &&
		pathIsSafeToRestore(settings.vaePath) &&
		pathIsSafeToRestore(settings.taesdPath) &&
		pathIsSafeToRestore(settings.controlNetPath) &&
		pathIsSafeToRestore(settings.loraModelDir) &&
		pathIsSafeToRestore(settings.embedDir) &&
		pathIsSafeToRestore(settings.stackedIdEmbedDir);
}

ofJson serializeContextSettings(const ofxGgmlStableDiffusionContextSettings& settings) {
	return {
		{"modelPath", settings.modelPath},
		{"diffusionModelPath", settings.diffusionModelPath},
		{"clipLPath", settings.clipLPath},
		{"clipGPath", settings.clipGPath},
		{"t5xxlPath", settings.t5xxlPath},
		{"vaePath", settings.vaePath},
		{"taesdPath", settings.taesdPath},
		{"controlNetPath", settings.controlNetPath},
		{"loraModelDir", settings.loraModelDir},
		{"embedDir", settings.embedDir},
		{"stackedIdEmbedDir", settings.stackedIdEmbedDir},
		{"vaeDecodeOnly", settings.vaeDecodeOnly},
		{"vaeTiling", settings.vaeTiling},
		{"freeParamsImmediately", settings.freeParamsImmediately},
		{"nThreads", settings.nThreads},
		{"weightType", static_cast<int>(settings.weightType)},
		{"rngType", static_cast<int>(settings.rngType)},
		{"schedule", static_cast<int>(settings.schedule)},
		{"prediction", static_cast<int>(settings.prediction)},
		{"loraApplyMode", static_cast<int>(settings.loraApplyMode)},
		{"keepClipOnCpu", settings.keepClipOnCpu},
		{"keepControlNetCpu", settings.keepControlNetCpu},
		{"keepVaeOnCpu", settings.keepVaeOnCpu},
		{"offloadParamsToCpu", settings.offloadParamsToCpu},
		{"flashAttn", settings.flashAttn},
		{"diffusionFlashAttn", settings.diffusionFlashAttn},
		{"enableMmap", settings.enableMmap},
		{"maxVram", settings.maxVram},
		{"streamLayers", settings.streamLayers},
		{"eagerLoad", settings.eagerLoad},
		{"backend", settings.backend},
		{"paramsBackend", settings.paramsBackend},
		{"splitMode", settings.splitMode},
		{"autoFit", settings.autoFit}
	};
}

ofJson serializeRealtimeRequest(const ofxGgmlStableDiffusionRealtimeRequest& request) {
	return {
		{"prompt", request.prompt},
		{"negativePrompt", request.negativePrompt},
		{"cfgScale", request.cfgScale},
		{"strength", request.strength},
		{"seed", request.seed},
		{"width", request.width},
		{"height", request.height},
		{"sampleSteps", request.sampleSteps},
		{"sampleMethod", static_cast<int>(request.sampleMethod)}
	};
}

ofJson serializeRealtimeVideoRequest(const ofxGgmlStableDiffusionRealtimeVideoRequest& request) {
	return {
		{"prompt", request.prompt},
		{"negativePrompt", request.negativePrompt},
		{"width", request.width},
		{"height", request.height},
		{"previewSteps", request.previewSteps},
		{"refineSteps", request.refineSteps},
		{"cfgScale", request.cfgScale},
		{"previewStrength", request.previewStrength},
		{"refineStrength", request.refineStrength},
		{"sampleMethod", static_cast<int>(request.sampleMethod)},
		{"seed", request.seed},
		{"lockSeed", request.lockSeed}
	};
}

ofJson serializeImageRequest(const ofxGgmlStableDiffusionImageRequest& request) {
	return {
		{"mode", static_cast<int>(request.mode)},
		{"selectionMode", static_cast<int>(request.selectionMode)},
		{"prompt", request.prompt},
		{"negativePrompt", request.negativePrompt},
		{"clipSkip", request.clipSkip},
		{"cfgScale", request.cfgScale},
		{"width", request.width},
		{"height", request.height},
		{"sampleMethod", static_cast<int>(request.sampleMethod)},
		{"schedule", static_cast<int>(request.schedule)},
		{"sampleSteps", request.sampleSteps},
		{"strength", request.strength},
		{"seed", request.seed},
		{"batchCount", request.batchCount}
	};
}

ofJson serializeVideoRequest(const ofxGgmlStableDiffusionVideoRequest& request) {
	return {
		{"prompt", request.prompt},
		{"negativePrompt", request.negativePrompt},
		{"clipSkip", request.clipSkip},
		{"width", request.width},
		{"height", request.height},
		{"frameCount", request.frameCount},
		{"fps", request.fps},
		{"cfgScale", request.cfgScale},
		{"guidance", request.guidance},
		{"sampleMethod", static_cast<int>(request.sampleMethod)},
		{"schedule", static_cast<int>(request.schedule)},
		{"sampleSteps", request.sampleSteps},
		{"eta", request.eta},
		{"flowShift", request.flowShift},
		{"strength", request.strength},
		{"seed", request.seed},
		{"vaceStrength", request.vaceStrength},
		{"mode", static_cast<int>(request.mode)}
	};
}

ofxGgmlStableDiffusionContextSettings parseContextSettings(const ofJson& json) {
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = json.value("modelPath", "");
	settings.diffusionModelPath = json.value("diffusionModelPath", "");
	settings.clipLPath = json.value("clipLPath", "");
	settings.clipGPath = json.value("clipGPath", "");
	settings.t5xxlPath = json.value("t5xxlPath", "");
	settings.vaePath = json.value("vaePath", "");
	settings.taesdPath = json.value("taesdPath", "");
	settings.controlNetPath = json.value("controlNetPath", "");
	settings.loraModelDir = json.value("loraModelDir", "");
	settings.embedDir = json.value("embedDir", "");
	settings.stackedIdEmbedDir = json.value("stackedIdEmbedDir", "");
	settings.vaeDecodeOnly = json.value("vaeDecodeOnly", false);
	settings.vaeTiling = json.value("vaeTiling", false);
	settings.freeParamsImmediately = json.value("freeParamsImmediately", true);
	settings.nThreads = json.value("nThreads", -1);
	settings.weightType = static_cast<sd_type_t>(json.value("weightType", static_cast<int>(settings.weightType)));
	settings.rngType = static_cast<rng_type_t>(json.value("rngType", static_cast<int>(settings.rngType)));
	settings.schedule = static_cast<scheduler_t>(json.value("schedule", static_cast<int>(settings.schedule)));
	settings.prediction = static_cast<prediction_t>(json.value("prediction", static_cast<int>(settings.prediction)));
	settings.loraApplyMode = static_cast<lora_apply_mode_t>(json.value("loraApplyMode", static_cast<int>(settings.loraApplyMode)));
	settings.keepClipOnCpu = json.value("keepClipOnCpu", false);
	settings.keepControlNetCpu = json.value("keepControlNetCpu", false);
	settings.keepVaeOnCpu = json.value("keepVaeOnCpu", false);
	settings.offloadParamsToCpu = json.value("offloadParamsToCpu", false);
	settings.flashAttn = json.value("flashAttn", false);
	settings.diffusionFlashAttn = json.value("diffusionFlashAttn", false);
	settings.enableMmap = json.value("enableMmap", true);
	settings.maxVram = json.value("maxVram", "");
	settings.streamLayers = json.value("streamLayers", false);
	settings.eagerLoad = json.value("eagerLoad", false);
	settings.backend = json.value("backend", "");
	settings.paramsBackend = json.value("paramsBackend", "");
	settings.splitMode = json.value("splitMode", "");
	settings.autoFit = json.value("autoFit", false);
	return settings;
}

ofxGgmlStableDiffusionRealtimeRequest parseRealtimeRequest(const ofJson& json) {
	ofxGgmlStableDiffusionRealtimeRequest request;
	request.prompt = json.value("prompt", "");
	request.negativePrompt = json.value("negativePrompt", "");
	request.cfgScale = json.value("cfgScale", request.cfgScale);
	request.strength = json.value("strength", request.strength);
	request.seed = json.value("seed", request.seed);
	request.width = json.value("width", request.width);
	request.height = json.value("height", request.height);
	request.sampleSteps = json.value("sampleSteps", request.sampleSteps);
	request.sampleMethod = static_cast<sample_method_t>(
		json.value("sampleMethod", static_cast<int>(request.sampleMethod)));
	return request;
}

ofxGgmlStableDiffusionRealtimeVideoRequest parseRealtimeVideoRequest(const ofJson& json) {
	ofxGgmlStableDiffusionRealtimeVideoRequest request;
	request.prompt = json.value("prompt", "");
	request.negativePrompt = json.value("negativePrompt", "");
	request.width = json.value("width", request.width);
	request.height = json.value("height", request.height);
	request.previewSteps = json.value("previewSteps", request.previewSteps);
	request.refineSteps = json.value("refineSteps", request.refineSteps);
	request.cfgScale = json.value("cfgScale", request.cfgScale);
	request.previewStrength = json.value("previewStrength", request.previewStrength);
	request.refineStrength = json.value("refineStrength", request.refineStrength);
	request.sampleMethod = static_cast<sample_method_t>(
		json.value("sampleMethod", static_cast<int>(request.sampleMethod)));
	request.seed = json.value("seed", request.seed);
	request.lockSeed = json.value("lockSeed", request.lockSeed);
	return request;
}

ofxGgmlStableDiffusionImageRequest parseImageRequest(const ofJson& json) {
	ofxGgmlStableDiffusionImageRequest request;
	request.mode = static_cast<ofxGgmlStableDiffusionImageMode>(
		json.value("mode", static_cast<int>(request.mode)));
	request.selectionMode = static_cast<ofxGgmlStableDiffusionImageSelectionMode>(
		json.value("selectionMode", static_cast<int>(request.selectionMode)));
	request.prompt = json.value("prompt", "");
	request.negativePrompt = json.value("negativePrompt", "");
	request.clipSkip = json.value("clipSkip", request.clipSkip);
	request.cfgScale = json.value("cfgScale", request.cfgScale);
	request.width = json.value("width", request.width);
	request.height = json.value("height", request.height);
	request.sampleMethod = static_cast<sample_method_t>(
		json.value("sampleMethod", static_cast<int>(request.sampleMethod)));
	request.schedule = static_cast<scheduler_t>(
		json.value("schedule", static_cast<int>(request.schedule)));
	request.sampleSteps = json.value("sampleSteps", request.sampleSteps);
	request.strength = json.value("strength", request.strength);
	request.seed = json.value("seed", request.seed);
	request.batchCount = json.value("batchCount", request.batchCount);
	return request;
}

ofxGgmlStableDiffusionVideoRequest parseVideoRequest(const ofJson& json) {
	ofxGgmlStableDiffusionVideoRequest request;
	request.prompt = json.value("prompt", "");
	request.negativePrompt = json.value("negativePrompt", "");
	request.clipSkip = json.value("clipSkip", request.clipSkip);
	request.width = json.value("width", request.width);
	request.height = json.value("height", request.height);
	request.frameCount = json.value("frameCount", request.frameCount);
	request.fps = json.value("fps", request.fps);
	request.cfgScale = json.value("cfgScale", request.cfgScale);
	request.guidance = json.value("guidance", request.guidance);
	request.sampleMethod = static_cast<sample_method_t>(
		json.value("sampleMethod", static_cast<int>(request.sampleMethod)));
	request.schedule = static_cast<scheduler_t>(
		json.value("schedule", static_cast<int>(request.schedule)));
	request.sampleSteps = json.value("sampleSteps", request.sampleSteps);
	request.eta = json.value("eta", request.eta);
	request.flowShift = json.value("flowShift", request.flowShift);
	request.strength = json.value("strength", request.strength);
	request.seed = json.value("seed", request.seed);
	request.vaceStrength = json.value("vaceStrength", request.vaceStrength);
	request.mode = static_cast<ofxGgmlStableDiffusionVideoMode>(
		json.value("mode", static_cast<int>(request.mode)));
	return request;
}

} // namespace

ofxGgmlStableDiffusionCreativeWorkflow::ofxGgmlStableDiffusionCreativeWorkflow() {
}

void ofxGgmlStableDiffusionCreativeWorkflow::setGenerator(ofxGgmlStableDiffusion* generator) {
	std::lock_guard<std::mutex> lock(mutex_);
	generator_ = generator;
	imagePreview_.setGenerator(generator);
	videoPreview_.setGenerator(generator);
}

ofxGgmlStableDiffusion* ofxGgmlStableDiffusionCreativeWorkflow::getGenerator() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return generator_;
}

bool ofxGgmlStableDiffusionCreativeWorkflow::start(
	const ofxGgmlStableDiffusionCreativeWorkflowSettings& settings,
	ofxGgmlStableDiffusion& generator) {
	setGenerator(&generator);
	settings_ = settings;
	active_ = true;
	queueGenerationInFlight_ = false;
	activeQueueRequestId_ = -1;
	const bool imageOk = imagePreview_.start(settings.imagePreviewSettings, generator);
	const bool videoOk = videoPreview_.start(settings.videoPreviewSettings, generator);
	return imageOk || videoOk;
}

void ofxGgmlStableDiffusionCreativeWorkflow::stop() {
	std::lock_guard<std::mutex> lock(mutex_);
	active_ = false;
	queueGenerationInFlight_ = false;
	activeQueueRequestId_ = -1;
	imagePreview_.stop();
	videoPreview_.stop();
}

bool ofxGgmlStableDiffusionCreativeWorkflow::isActive() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return active_;
}

bool ofxGgmlStableDiffusionCreativeWorkflow::submitImagePreview(
	const ofxGgmlStableDiffusionRealtimeRequest& request) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		lastImagePreviewRequest_ = request;
	}
	return imagePreview_.submit(request);
}

bool ofxGgmlStableDiffusionCreativeWorkflow::submitVideoPreview(
	const ofxGgmlStableDiffusionRealtimeVideoRequest& request) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		lastVideoPreviewRequest_ = request;
	}
	return videoPreview_.submit(request);
}

int ofxGgmlStableDiffusionCreativeWorkflow::queueImageRender(
	const ofxGgmlStableDiffusionImageRequest& request,
	ofxGgmlStableDiffusionPriority priority,
	const std::string& tag) {
	return queue_.addImageRequest(
		request,
		priority,
		tag.empty() ? settings_.queueTag : tag);
}

int ofxGgmlStableDiffusionCreativeWorkflow::queueImageRenderFromPreview() {
	ofxGgmlStableDiffusion* generator = nullptr;
	ofxGgmlStableDiffusionRealtimeRequest previewRequest;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		generator = generator_;
		previewRequest = lastImagePreviewRequest_;
	}
	if (generator == nullptr) {
		return -1;
	}
	const auto capabilities = generator->getCapabilities();

	const auto request = ofxGgmlStableDiffusionBuildCreativeRenderRequest(
		previewRequest,
		generator->getContextSettings(),
		settings_,
		&capabilities);
	return queueImageRender(request, settings_.queuedRenderPriority, settings_.queueTag);
}

int ofxGgmlStableDiffusionCreativeWorkflow::queueVideoRender(
	const ofxGgmlStableDiffusionVideoRequest& request,
	ofxGgmlStableDiffusionPriority priority,
	const std::string& tag) {
	return queue_.addVideoRequest(
		request,
		priority,
		tag.empty() ? settings_.queueTag : tag);
}

void ofxGgmlStableDiffusionCreativeWorkflow::update() {
	if (!isActive()) {
		return;
	}
	imagePreview_.update();
	videoPreview_.update();
	processQueue();
}

ofxGgmlStableDiffusionQueue& ofxGgmlStableDiffusionCreativeWorkflow::getQueue() {
	return queue_;
}

const ofxGgmlStableDiffusionQueue& ofxGgmlStableDiffusionCreativeWorkflow::getQueue() const {
	return queue_;
}

ofxGgmlStableDiffusionRealtimeSession&
ofxGgmlStableDiffusionCreativeWorkflow::getImagePreviewSession() {
	return imagePreview_;
}

const ofxGgmlStableDiffusionRealtimeSession&
ofxGgmlStableDiffusionCreativeWorkflow::getImagePreviewSession() const {
	return imagePreview_;
}

ofxGgmlStableDiffusionRealtimeVideoSession&
ofxGgmlStableDiffusionCreativeWorkflow::getVideoPreviewSession() {
	return videoPreview_;
}

const ofxGgmlStableDiffusionRealtimeVideoSession&
ofxGgmlStableDiffusionCreativeWorkflow::getVideoPreviewSession() const {
	return videoPreview_;
}

ofxGgmlStableDiffusionCreativeWorkflowSnapshot
ofxGgmlStableDiffusionCreativeWorkflow::getSnapshot() const {
	std::lock_guard<std::mutex> lock(mutex_);
	ofxGgmlStableDiffusionCreativeWorkflowSnapshot snapshot;
	if (generator_ != nullptr) {
		snapshot.contextSettings = generator_->getContextSettings();
	}
	snapshot.lastImagePreviewRequest = lastImagePreviewRequest_;
	snapshot.lastVideoPreviewRequest = lastVideoPreviewRequest_;
	snapshot.imagePreviewActive = imagePreview_.isActive();
	snapshot.videoPreviewActive = videoPreview_.isActive();
	const auto queuedRequests = queue_.getRequestsByState(ofxGgmlStableDiffusionQueueState::Queued);
	for (const auto& request : queuedRequests) {
		if (request->isVideoGeneration()) {
			snapshot.queuedVideoRequests++;
		} else if (request->isImageGeneration()) {
			snapshot.queuedImageRequests++;
		}
	}
	return snapshot;
}

bool ofxGgmlStableDiffusionCreativeWorkflow::saveSession(const std::string& path) const {
	std::lock_guard<std::mutex> lock(mutex_);
	ofJson json;
	json["context"] = generator_ != nullptr
		? serializeContextSettings(generator_->getContextSettings())
		: serializeContextSettings(ofxGgmlStableDiffusionContextSettings());
	json["lastImagePreviewRequest"] = serializeRealtimeRequest(lastImagePreviewRequest_);
	json["lastVideoPreviewRequest"] = serializeRealtimeVideoRequest(lastVideoPreviewRequest_);
	json["imagePreviewActive"] = imagePreview_.isActive();
	json["videoPreviewActive"] = videoPreview_.isActive();
	json["queueTag"] = settings_.queueTag;
	json["queuedRenderPriority"] = static_cast<int>(settings_.queuedRenderPriority);
	json["renderSampleSteps"] = settings_.renderSampleSteps;
	json["renderCfgScale"] = settings_.renderCfgScale;
	json["autoApplyModelDefaults"] = settings_.autoApplyModelDefaults;
	json["pausePreviewWhileQueueDrains"] = settings_.pausePreviewWhileQueueDrains;

	json["queuedImageRequests"] = ofJson::array();
	for (const auto& request : queue_.getRequestsByState(ofxGgmlStableDiffusionQueueState::Queued)) {
		if (!request->isImageGeneration()) {
			continue;
		}
		ofJson entry;
		entry["tag"] = request->tag;
		entry["priority"] = static_cast<int>(request->priority);
		entry["request"] = serializeImageRequest(request->imageRequest);
		json["queuedImageRequests"].push_back(entry);
	}

	json["queuedVideoRequests"] = ofJson::array();
	for (const auto& request : queue_.getRequestsByState(ofxGgmlStableDiffusionQueueState::Queued)) {
		if (!request->isVideoGeneration()) {
			continue;
		}
		ofJson entry;
		entry["tag"] = request->tag;
		entry["priority"] = static_cast<int>(request->priority);
		entry["request"] = serializeVideoRequest(request->videoRequest);
		json["queuedVideoRequests"].push_back(entry);
	}

	return ofSavePrettyJson(path, json);
}

bool ofxGgmlStableDiffusionCreativeWorkflow::loadSession(const std::string& path) {
	const ofJson json = ofLoadJson(path);
	if (json.is_null() || json.empty()) {
		return false;
	}

	stop();
	queue_.cancelAll();
	queue_.clearHistory();

	settings_.queueTag = json.value("queueTag", settings_.queueTag);
	settings_.queuedRenderPriority = static_cast<ofxGgmlStableDiffusionPriority>(
		json.value("queuedRenderPriority", static_cast<int>(settings_.queuedRenderPriority)));
	settings_.renderSampleSteps = json.value("renderSampleSteps", settings_.renderSampleSteps);
	settings_.renderCfgScale = json.value("renderCfgScale", settings_.renderCfgScale);
	settings_.autoApplyModelDefaults = json.value(
		"autoApplyModelDefaults",
		settings_.autoApplyModelDefaults);
	settings_.pausePreviewWhileQueueDrains = json.value(
		"pausePreviewWhileQueueDrains",
		settings_.pausePreviewWhileQueueDrains);

	if (json.contains("lastImagePreviewRequest")) {
		lastImagePreviewRequest_ = parseRealtimeRequest(json["lastImagePreviewRequest"]);
	}
	if (json.contains("lastVideoPreviewRequest")) {
		lastVideoPreviewRequest_ = parseRealtimeVideoRequest(json["lastVideoPreviewRequest"]);
	}

	if (generator_ != nullptr && json.contains("context")) {
		const auto restoredContext = parseContextSettings(json["context"]);
		if (contextSettingsAreSafeToRestore(restoredContext)) {
			generator_->configureContext(restoredContext);
		} else {
			ofLogWarning("ofxGgmlStableDiffusionCreativeWorkflow")
				<< "Skipped restoring model context because one or more saved paths "
				<< "were missing or unsafe";
		}
	}

	if (json.contains("queuedImageRequests")) {
		for (const auto& entry : json["queuedImageRequests"]) {
			queue_.addImageRequest(
				parseImageRequest(entry["request"]),
				static_cast<ofxGgmlStableDiffusionPriority>(
					entry.value("priority", static_cast<int>(settings_.queuedRenderPriority))),
				entry.value("tag", settings_.queueTag));
		}
	}

	if (json.contains("queuedVideoRequests")) {
		for (const auto& entry : json["queuedVideoRequests"]) {
			queue_.addVideoRequest(
				parseVideoRequest(entry["request"]),
				static_cast<ofxGgmlStableDiffusionPriority>(
					entry.value("priority", static_cast<int>(settings_.queuedRenderPriority))),
				entry.value("tag", settings_.queueTag));
		}
	}

	active_ = true;
	return true;
}

void ofxGgmlStableDiffusionCreativeWorkflow::processQueue() {
	ofxGgmlStableDiffusion* generator = nullptr;
	bool active = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		active = active_;
		generator = generator_;
	}
	if (!active || generator == nullptr) {
		return;
	}

	if (queueGenerationInFlight_) {
		if (generator->isGenerating()) {
			return;
		}

		const ofxGgmlStableDiffusionResult result = generator->getLastResult();
		if (result.success) {
			queue_.markRequestCompleted(activeQueueRequestId_, result);
		} else {
			queue_.markRequestFailed(
				activeQueueRequestId_,
				result.error.empty() ? "Queued generation failed" : result.error);
		}
		queueGenerationInFlight_ = false;
		activeQueueRequestId_ = -1;
	}

	if (generator->isGenerating()) {
		return;
	}
	if (settings_.pausePreviewWhileQueueDrains &&
		(imagePreview_.isGenerating() || videoPreview_.isGenerating())) {
		return;
	}

	auto nextRequest = queue_.getNextRequest();
	if (nextRequest == nullptr) {
		return;
	}

	activeQueueRequestId_ = nextRequest->requestId;
	queue_.markRequestProcessing(activeQueueRequestId_);
	if (nextRequest->isVideoGeneration()) {
		generator->generateVideo(nextRequest->videoRequest);
		queueGenerationInFlight_ = true;
		return;
	}
	if (nextRequest->isImageGeneration()) {
		generator->generate(nextRequest->imageRequest);
		queueGenerationInFlight_ = true;
		return;
	}

	queue_.markRequestFailed(activeQueueRequestId_, "Unsupported queued task");
	activeQueueRequestId_ = -1;
}
