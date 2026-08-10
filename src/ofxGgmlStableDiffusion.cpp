#include "ofxGgmlStableDiffusion.h"
#include "core/ofxGgmlStableDiffusionCapabilityHelpers.h"
#include "core/ofxGgmlStableDiffusionLimits.h"
#include "core/ofxGgmlStableDiffusionMemoryHelpers.h"
#include "core/ofxGgmlStableDiffusionNativeAdapter.h"
#include "core/ofxGgmlStableDiffusionValidationHelpers.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <limits>
#include <mutex>
#include <thread>

namespace {

namespace fs = std::filesystem;
std::atomic<bool> g_sdLoggingEnabled{true};
std::atomic<int> g_sdMinLogLevel{SD_LOG_DEBUG};
bool isProgressLikeSdLog(const char* log) {
	if (log == nullptr) {
		return false;
	}
	if (std::strchr(log, '\r') != nullptr) {
		return true;
	}
	const char* p = log;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
		++p;
	}
	return *p == '|';
}

void sd_log_cb(enum sd_log_level_t level, const char* log, void* data) {
	(void)data;
	if (!g_sdLoggingEnabled.load()) {
		return;
	}
	if (level < g_sdMinLogLevel.load()) {
		return;
	}
	if (log == nullptr || log[0] == '\0') {
		return;
	}
	if (isProgressLikeSdLog(log)) {
		FILE* stream = (level <= SD_LOG_INFO) ? stdout : stderr;
		std::string prefixed;
		prefixed.reserve(std::strlen(log) + 16);
		bool atLineStart = true;
		for (const char* p = log; *p != '\0'; ++p) {
			if (*p == '\r' || *p == '\n') {
				prefixed.push_back(*p);
				atLineStart = true;
				continue;
			}
			if (atLineStart) {
				prefixed += "[notice ] ";
				atLineStart = false;
			}
			prefixed.push_back(*p);
		}
		fputs(prefixed.c_str(), stream);
		fflush(stream);
		return;
	}

	std::string message(log);
	while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
		message.pop_back();
	}
	if (message.empty()) {
		return;
	}

	if (!ofIsCurrentThreadTheMainThread()) {
		FILE* stream = (level <= SD_LOG_INFO) ? stdout : stderr;
		const char* prefix =
			level == SD_LOG_DEBUG ? "[debug ] " :
			level == SD_LOG_INFO ? "[notice] " :
			level == SD_LOG_WARN ? "[warning] " :
			"[error ] ";
		fputs(prefix, stream);
		fputs(message.c_str(), stream);
		fputc('\n', stream);
		fflush(stream);
		return;
	}

	switch (level) {
	case SD_LOG_DEBUG:
		ofLogVerbose("stable-diffusion") << message;
		break;
	case SD_LOG_INFO:
		ofLogNotice("stable-diffusion") << message;
		break;
	case SD_LOG_WARN:
		ofLogWarning("stable-diffusion") << message;
		break;
	case SD_LOG_ERROR:
	default:
		ofLogError("stable-diffusion") << message;
		break;
	}
}

struct ValidationResult {
	ofxGgmlStableDiffusionErrorCode code = ofxGgmlStableDiffusionErrorCode::None;
	std::string message;

	bool ok() const {
		return code == ofxGgmlStableDiffusionErrorCode::None;
	}
};

std::string formatValidationFloat(float value) {
	std::string formatted = ofToString(value, 3);
	while (!formatted.empty() && formatted.back() == '0') {
		formatted.pop_back();
	}
	if (!formatted.empty() && formatted.back() == '.') {
		formatted.pop_back();
	}
	return formatted.empty() ? "0" : formatted;
}

ValidationResult validateDimensions(int width, int height) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (width <= 0 || height <= 0) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidDimensions, "Width and height must be positive values"};
	}
	if (width > MAX_DIMENSION || height > MAX_DIMENSION) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidDimensions,
			"Width and height must not exceed " + std::to_string(MAX_DIMENSION) + " pixels"};
	}
	return {};
}

ValidationResult validateBatchCount(int batchCount) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (batchCount <= 0) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidBatchCount, "Batch count must be positive"};
	}
	if (batchCount > MAX_BATCH_COUNT) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidBatchCount,
			"Batch count exceeds maximum of " + std::to_string(MAX_BATCH_COUNT)};
	}
	return {};
}

ValidationResult validateSampleSteps(int sampleSteps) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (sampleSteps <= 0 || sampleSteps > MAX_SAMPLE_STEPS) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"Sample steps must be between 1 and " + std::to_string(MAX_SAMPLE_STEPS)};
	}
	return {};
}

ValidationResult validateCfgScale(float cfgScale) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (cfgScale < MIN_CFG_SCALE || cfgScale > MAX_CFG_SCALE) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"CFG scale must be greater than or equal to " + formatValidationFloat(MIN_CFG_SCALE) +
				" and no more than " + formatValidationFloat(MAX_CFG_SCALE)};
	}
	return {};
}

ValidationResult validateStrength(float strength) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (!isValidUnitInterval(strength)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Strength must be between 0.0 and 1.0"};
	}
	return {};
}

ValidationResult validateClipSkip(int clipSkip) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (!isValidClipSkip(clipSkip)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"Clip skip must be -1 (auto) or between 0 and " + std::to_string(MAX_CLIP_SKIP)};
	}
	return {};
}

ValidationResult validateSeed(int64_t seed) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (!isValidSeed(seed)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"Seed must be -1 for randomization or a non-negative value"};
	}
	return {};
}

ValidationResult validateControlStrength(float controlStrength) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (!isValidControlStrength(controlStrength)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"Control strength must be between 0.0 and " + std::to_string(static_cast<int>(MAX_CONTROL_STRENGTH))};
	}
	return {};
}

ValidationResult validateStyleStrength(float styleStrength) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (!isValidStyleStrength(styleStrength)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"Style strength must be between 0 and " + std::to_string(static_cast<int>(MAX_STYLE_STRENGTH))};
	}
	return {};
}

ValidationResult validateVaceStrength(float vaceStrength) {
	using namespace ofxGgmlStableDiffusionLimits;
	if (!isValidUnitInterval(vaceStrength)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter, "VACE strength must be between 0.0 and 1.0"};
	}
	return {};
}

ValidationResult validateUnitInterval(float value, const std::string& label) {
	if (value < 0.0f || value > 1.0f) {
		return {
			ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			label + " must be between 0.0 and 1.0"
		};
	}
	return {};
}

ValidationResult validateOptionalFinite(float value, const std::string& label) {
	if (!ofxSdOptionalFloatIsFiniteWhenProvided(value)) {
		return {
			ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			label + " must be finite when provided"
		};
	}
	return {};
}

bool isNativeWanVideoFamily(ofxGgmlStableDiffusionModelFamily family) {
	return family == ofxGgmlStableDiffusionModelFamily::WAN ||
		family == ofxGgmlStableDiffusionModelFamily::WANI2V ||
		family == ofxGgmlStableDiffusionModelFamily::WANTI2V ||
		family == ofxGgmlStableDiffusionModelFamily::WANFLF2V ||
		family == ofxGgmlStableDiffusionModelFamily::WANVACE;
}

ValidationResult validateControlNets(
	const std::vector<ofxGgmlStableDiffusionControlNet>& controlNets,
	int width,
	int height) {
	if (controlNets.empty()) {
		return {};
	}

	int expectedWidth = -1;
	int expectedHeight = -1;
	int expectedChannels = -1;
	for (const auto& controlNet : controlNets) {
		if (controlNet.conditionImage.data == nullptr) {
			return {ofxGgmlStableDiffusionErrorCode::InvalidParameter, "ControlNet image is missing"};
		}
		if (controlNet.strength < 0.0f || controlNet.strength > 2.0f) {
			return {ofxGgmlStableDiffusionErrorCode::InvalidParameter, "ControlNet strength must be between 0.0 and 2.0"};
		}

		const int controlWidth = static_cast<int>(controlNet.conditionImage.width);
		const int controlHeight = static_cast<int>(controlNet.conditionImage.height);
		const int controlChannels = static_cast<int>(controlNet.conditionImage.channel);

		if (expectedWidth < 0) {
			expectedWidth = controlWidth;
			expectedHeight = controlHeight;
			expectedChannels = controlChannels;
		} else if (controlWidth != expectedWidth ||
			controlHeight != expectedHeight ||
			controlChannels != expectedChannels) {
			return {ofxGgmlStableDiffusionErrorCode::InvalidDimensions, "All ControlNet images must share width, height, and channel count"};
		}

		if ((width > 0 && controlWidth != width) || (height > 0 && controlHeight != height)) {
			return {ofxGgmlStableDiffusionErrorCode::InvalidDimensions, "ControlNet image dimensions must match the request dimensions"};
		}
	}

	return {};
}

ValidationResult validateImageRequestNumbers(const ofxGgmlStableDiffusionImageRequest& request) {
	const ValidationResult dimResult = validateDimensions(request.width, request.height);
	if (!dimResult.ok()) return dimResult;

	const ValidationResult batchResult = validateBatchCount(request.batchCount);
	if (!batchResult.ok()) return batchResult;

	if (request.sampleSteps > 0) {
		const ValidationResult stepsResult = validateSampleSteps(request.sampleSteps);
		if (!stepsResult.ok()) return stepsResult;
	}

	const ValidationResult cfgFiniteResult = validateOptionalFinite(request.cfgScale, "CFG scale");
	if (!cfgFiniteResult.ok()) return cfgFiniteResult;
	if (std::isfinite(request.cfgScale)) {
		const ValidationResult cfgResult = validateCfgScale(request.cfgScale);
		if (!cfgResult.ok()) return cfgResult;
	}

	const ValidationResult flowShiftFiniteResult = validateOptionalFinite(request.flowShift, "Flow shift");
	if (!flowShiftFiniteResult.ok()) return flowShiftFiniteResult;

	const ValidationResult strengthFiniteResult = validateOptionalFinite(request.strength, "Strength");
	if (!strengthFiniteResult.ok()) return strengthFiniteResult;
	if (std::isfinite(request.strength)) {
		const ValidationResult strengthResult = validateStrength(request.strength);
		if (!strengthResult.ok()) return strengthResult;
	}

	const ValidationResult clipResult = validateClipSkip(request.clipSkip);
	if (!clipResult.ok()) return clipResult;

	const ValidationResult seedResult = validateSeed(request.seed);
	if (!seedResult.ok()) return seedResult;

	const ValidationResult controlResult = validateControlStrength(request.controlStrength);
	if (!controlResult.ok()) return controlResult;

	const ValidationResult styleResult = validateStyleStrength(request.styleStrength);
	if (!styleResult.ok()) return styleResult;

	const ValidationResult controlNetResult =
		validateControlNets(request.controlNets, request.width, request.height);
	if (!controlNetResult.ok()) return controlNetResult;

	return {};
}

ValidationResult validateVideoRequestNumbers(const ofxGgmlStableDiffusionVideoRequest& request) {
	const ValidationResult dimResult = validateDimensions(request.width, request.height);
	if (!dimResult.ok()) return dimResult;

	if (!ofxGgmlStableDiffusionLimits::isValidFrameCount(request.frameCount)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidFrameCount,
			"Frame count must be between " +
			std::to_string(ofxGgmlStableDiffusionLimits::MIN_FRAME_COUNT) + " and " +
			std::to_string(ofxGgmlStableDiffusionLimits::MAX_FRAME_COUNT)};
	}

	if (!ofxGgmlStableDiffusionLimits::isValidFps(request.fps)) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"FPS must be between " +
			std::to_string(ofxGgmlStableDiffusionLimits::MIN_FPS) + " and " +
			std::to_string(ofxGgmlStableDiffusionLimits::MAX_FPS)};
	}

	const ValidationResult clipResult = validateClipSkip(request.clipSkip);
	if (!clipResult.ok()) return clipResult;

	const ValidationResult cfgFiniteResult = validateOptionalFinite(request.cfgScale, "CFG scale");
	if (!cfgFiniteResult.ok()) return cfgFiniteResult;
	if (std::isfinite(request.cfgScale)) {
		const ValidationResult cfgResult = validateCfgScale(request.cfgScale);
		if (!cfgResult.ok()) return cfgResult;
	}

	const ValidationResult guidanceFiniteResult = validateOptionalFinite(request.guidance, "Guidance");
	if (!guidanceFiniteResult.ok()) return guidanceFiniteResult;

	const ValidationResult etaFiniteResult = validateOptionalFinite(request.eta, "Eta");
	if (!etaFiniteResult.ok()) return etaFiniteResult;

	const ValidationResult flowShiftFiniteResult = validateOptionalFinite(request.flowShift, "Flow shift");
	if (!flowShiftFiniteResult.ok()) return flowShiftFiniteResult;

	if (request.sampleSteps > 0) {
		const ValidationResult stepsResult = validateSampleSteps(request.sampleSteps);
		if (!stepsResult.ok()) return stepsResult;
	}

	const ValidationResult strengthFiniteResult = validateOptionalFinite(request.strength, "Strength");
	if (!strengthFiniteResult.ok()) return strengthFiniteResult;
	if (std::isfinite(request.strength)) {
		const ValidationResult strengthResult = validateStrength(request.strength);
		if (!strengthResult.ok()) return strengthResult;
	}

	const ValidationResult seedResult = validateSeed(request.seed);
	if (!seedResult.ok()) return seedResult;

	for (std::size_t i = 0; i < request.controlFrames.size(); ++i) {
		const auto& frame = request.controlFrames[i];
		if (frame.data == nullptr || frame.width == 0 || frame.height == 0 || frame.channel == 0) {
			return {
				ofxGgmlStableDiffusionErrorCode::InvalidParameter,
				"Control frame " + ofToString(static_cast<int>(i)) + " is not allocated"
			};
		}
		if (frame.width != static_cast<uint32_t>(request.width) ||
			frame.height != static_cast<uint32_t>(request.height)) {
			return {
				ofxGgmlStableDiffusionErrorCode::InvalidDimensions,
				"Control frame " + ofToString(static_cast<int>(i)) + " dimensions must match the request dimensions"
			};
		}
	}

	if (request.cache.mode == SD_CACHE_EASYCACHE || request.cache.mode == SD_CACHE_UCACHE) {
		if (request.cache.reuse_threshold < 0.0f) {
			return {
				ofxGgmlStableDiffusionErrorCode::InvalidParameter,
				"Video cache reuse threshold must be non-negative"
			};
		}
		if (request.cache.start_percent < 0.0f ||
			request.cache.start_percent >= 1.0f ||
			request.cache.end_percent <= 0.0f ||
			request.cache.end_percent > 1.0f ||
			request.cache.start_percent >= request.cache.end_percent) {
			return {
				ofxGgmlStableDiffusionErrorCode::InvalidParameter,
				"Video cache start/end percents must satisfy 0.0 <= start < end <= 1.0"
			};
		}
	}

	const ValidationResult moeBoundaryFiniteResult = validateOptionalFinite(request.moeBoundary, "MoE boundary");
	if (!moeBoundaryFiniteResult.ok()) return moeBoundaryFiniteResult;
	if (std::isfinite(request.moeBoundary)) {
		const ValidationResult moeBoundaryResult =
			validateUnitInterval(request.moeBoundary, "MoE boundary");
		if (!moeBoundaryResult.ok()) return moeBoundaryResult;
	}

	const ValidationResult vaceStrengthFiniteResult = validateOptionalFinite(request.vaceStrength, "VACE strength");
	if (!vaceStrengthFiniteResult.ok()) return vaceStrengthFiniteResult;
	if (std::isfinite(request.vaceStrength)) {
		const ValidationResult vaceResult = validateVaceStrength(request.vaceStrength);
		if (!vaceResult.ok()) return vaceResult;
	}

	if (request.useHighNoiseOverrides) {
		const ValidationResult highNoiseCfgFiniteResult = validateOptionalFinite(request.highNoiseCfgScale, "High-noise CFG scale");
		if (!highNoiseCfgFiniteResult.ok()) return highNoiseCfgFiniteResult;
		if (std::isfinite(request.highNoiseCfgScale)) {
			const ValidationResult highNoiseCfgResult = validateCfgScale(request.highNoiseCfgScale);
			if (!highNoiseCfgResult.ok()) return highNoiseCfgResult;
		}

		const ValidationResult highNoiseGuidanceFiniteResult = validateOptionalFinite(request.highNoiseGuidance, "High-noise guidance");
		if (!highNoiseGuidanceFiniteResult.ok()) return highNoiseGuidanceFiniteResult;

		const ValidationResult highNoiseEtaFiniteResult = validateOptionalFinite(request.highNoiseEta, "High-noise eta");
		if (!highNoiseEtaFiniteResult.ok()) return highNoiseEtaFiniteResult;

		const ValidationResult highNoiseFlowShiftFiniteResult = validateOptionalFinite(request.highNoiseFlowShift, "High-noise flow shift");
		if (!highNoiseFlowShiftFiniteResult.ok()) return highNoiseFlowShiftFiniteResult;

		if (request.highNoiseSampleSteps > 0) {
			const ValidationResult highNoiseStepsResult = validateSampleSteps(request.highNoiseSampleSteps);
			if (!highNoiseStepsResult.ok()) return highNoiseStepsResult;
		}
	}

	if (request.hasAnimation()) {
		std::string animationError;
		if (!ofxGgmlStableDiffusionValidateAnimationKeyframes(
				request.animationSettings,
				request.frameCount,
				animationError)) {
			return {ofxGgmlStableDiffusionErrorCode::InvalidParameter, animationError};
		}
	}

	return {};
}

ValidationResult validateUpscalerSettings(const ofxGgmlStableDiffusionUpscalerSettings& settings) {
	if (settings.nThreads == 0 || settings.nThreads < -1) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Upscaler thread count must be -1 (auto) or a positive value"};
	}
	if (settings.multiplier < 1 || settings.multiplier > 8) {
		return {ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Upscale multiplier must be between 1 and 8"};
	}
	return {};
}

bool mergeControlNets(
	const std::vector<ofxGgmlStableDiffusionControlNet>& controlNets,
	ofxGgmlStableDiffusionThread::OwnedImage& output,
	float& resolvedStrength,
	std::string& errorMessage) {
	if (controlNets.empty()) {
		return false;
	}

	const sd_image_t& reference = controlNets.front().conditionImage;
	const std::size_t byteCount =
		static_cast<std::size_t>(reference.width) *
		static_cast<std::size_t>(reference.height) *
		static_cast<std::size_t>(reference.channel);

	std::vector<double> accum(byteCount, 0.0);
	double totalStrength = 0.0;

	for (std::size_t idx = 0; idx < controlNets.size(); ++idx) {
		const auto& controlNet = controlNets[idx];
		if (controlNet.conditionImage.data == nullptr) {
			errorMessage = "ControlNet image is missing (index " + std::to_string(idx) + ")";
			return false;
		}
		if (controlNet.strength < 0.0f || controlNet.strength > 2.0f) {
			errorMessage = "ControlNet strength must be between 0.0 and 2.0 (index " + std::to_string(idx) + ")";
			return false;
		}

		const double weight = static_cast<double>(controlNet.strength);
		totalStrength += weight;
		const uint8_t* data = controlNet.conditionImage.data;
		for (std::size_t i = 0; i < byteCount; ++i) {
			accum[i] += weight * static_cast<double>(data[i]);
		}
	}

	if (totalStrength <= 0.0) {
		errorMessage = "Combined ControlNet strength must be greater than zero";
		return false;
	}

	output.storage.resize(byteCount);
	for (std::size_t i = 0; i < byteCount; ++i) {
		const double value = accum[i] / totalStrength;
		output.storage[i] = static_cast<uint8_t>(std::clamp(
			std::llround(value),
			0ll,
			255ll));
	}

	output.image = {
		reference.width,
		reference.height,
		reference.channel,
		output.storage.data()
	};

	resolvedStrength = static_cast<float>(std::min(
		totalStrength / static_cast<double>(controlNets.size()),
		2.0));
	return true;
}

bool isResolvableModelFile(const fs::path& path) {
	if (!fs::is_regular_file(path)) {
		return false;
	}

	std::string ext = ofToLower(path.extension().string());
	return ext == ".gguf" ||
		ext == ".safetensors" ||
		ext == ".ckpt" ||
		ext == ".bin" ||
		ext == ".pth";
}

void appendUniqueSearchRoot(std::vector<fs::path>& roots, const fs::path& root) {
	if (root.empty()) {
		return;
	}

	const fs::path normalized = root.lexically_normal();
	if (std::find(roots.begin(), roots.end(), normalized) == roots.end()) {
		roots.push_back(normalized);
	}
}

void appendSearchRootsForModelPath(std::vector<fs::path>& roots, const std::string& modelPath) {
	if (modelPath.empty()) {
		return;
	}
	if (ofxSdPathHasParentTraversal(modelPath)) {
		return;
	}

	const fs::path path(modelPath);
	const fs::path dir = path.has_parent_path() ? path.parent_path() : fs::path();
	if (dir.empty()) {
		return;
	}

	appendUniqueSearchRoot(roots, dir);
	if (dir.has_parent_path()) {
		appendUniqueSearchRoot(roots, dir.parent_path());
	}
}

std::string readGgufArchitecture(const std::string& path);

struct GgufMetadataInfo {
	std::string architecture;
	uint32_t maxTensorDimensions = 0;
	bool readable = false;
};

std::string resolveTextEncoderPathFromSubfolders(const ofxGgmlStableDiffusionContextSettings& settings) {
	std::vector<fs::path> roots;
	appendSearchRootsForModelPath(roots, settings.diffusionModelPath);
	appendSearchRootsForModelPath(roots, settings.modelPath);
	if (roots.empty()) {
		return "";
	}

	const std::vector<std::string> subfolders = {"umt5", "t5xxl", "text_encoders"};
	const std::vector<std::string> preferredNameParts = {"umt5", "t5xxl", "encoder"};

	for (const auto& root : roots) {
		for (const auto& subfolder : subfolders) {
			const fs::path candidateDir = root / subfolder;
			std::error_code ec;
			if (!fs::exists(candidateDir, ec) || ec) {
				continue;
			}
			ec.clear();
			if (!fs::is_directory(candidateDir, ec) || ec) {
				continue;
			}

			std::vector<fs::path> preferredFiles;
			std::vector<fs::path> fallbackFiles;
			fs::directory_iterator it(candidateDir, ec);
			if (ec) {
				continue;
			}
			for (fs::directory_iterator end; it != end;) {
				const auto& entry = *it;
				const fs::path candidatePath = entry.path();
				if (!isResolvableModelFile(candidatePath)) {
					it.increment(ec);
					if (ec) {
						break;
					}
					continue;
				}

				const std::string filename = ofToLower(candidatePath.filename().string());
				const bool preferred = std::any_of(
					preferredNameParts.begin(),
					preferredNameParts.end(),
					[&filename](const std::string& part) {
						return filename.find(part) != std::string::npos;
					});

				if (preferred) {
					preferredFiles.push_back(candidatePath);
				} else {
					fallbackFiles.push_back(candidatePath);
				}
				it.increment(ec);
				if (ec) {
					break;
				}
			}
			if (ec) {
				continue;
			}

			auto byFilename = [](const fs::path& a, const fs::path& b) {
				return ofToLower(a.filename().string()) < ofToLower(b.filename().string());
			};
			std::sort(preferredFiles.begin(), preferredFiles.end(), byFilename);
			std::sort(fallbackFiles.begin(), fallbackFiles.end(), byFilename);

			if (!preferredFiles.empty()) {
				return preferredFiles.front().string();
			}
			if (!fallbackFiles.empty()) {
				return fallbackFiles.front().string();
			}
		}
	}

	return "";
}

ofxGgmlStableDiffusionContextSettings resolveContextModelPaths(
	const ofxGgmlStableDiffusionContextSettings& requestedSettings) {
	ofxGgmlStableDiffusionContextSettings resolvedSettings = requestedSettings;

	if (!resolvedSettings.modelPath.empty() && resolvedSettings.diffusionModelPath.empty()) {
		const std::string architecture = ofToLower(readGgufArchitecture(resolvedSettings.modelPath));
		if (architecture == "wan") {
			resolvedSettings.diffusionModelPath = resolvedSettings.modelPath;
			resolvedSettings.modelPath.clear();
		}
	}

	if (resolvedSettings.t5xxlPath.empty()) {
		resolvedSettings.t5xxlPath = resolveTextEncoderPathFromSubfolders(resolvedSettings);
	}

	return resolvedSettings;
}

std::vector<std::string> describeMissingContextModelPaths(
	const ofxGgmlStableDiffusionContextSettings& settings) {
	std::vector<std::string> missing;
	const auto addMissingFile = [&missing](const std::string& path, const char* label) {
		if (path.empty()) {
			return;
		}
		std::error_code ec;
		const fs::path p(path);
		if (!fs::exists(p, ec) || ec || !fs::is_regular_file(p, ec) || ec) {
			missing.push_back(std::string(label) + ": " + p.string());
		}
	};
	addMissingFile(settings.modelPath, "model");
	addMissingFile(settings.diffusionModelPath, "diffusion");
	addMissingFile(settings.clipLPath, "clip_l");
	addMissingFile(settings.clipGPath, "clip_g");
	addMissingFile(settings.t5xxlPath, "text_encoder");
	addMissingFile(settings.vaePath, "vae");
	addMissingFile(settings.taesdPath, "taesd");
	addMissingFile(settings.controlNetPath, "controlnet");
	addMissingFile(settings.stackedIdEmbedDir, "photomaker");
	return missing;
}

bool hasPrimaryContextModelPath(const ofxGgmlStableDiffusionContextSettings& settings) {
	return !settings.modelPath.empty() || !settings.diffusionModelPath.empty();
}

template <typename T>
bool readBinary(std::ifstream& input, T& value) {
	input.read(reinterpret_cast<char*>(&value), sizeof(T));
	return input.good();
}

bool skipGgufBytes(std::ifstream& input, uint64_t count) {
	const auto maxOffset = static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max());
	if (count > maxOffset) {
		return false;
	}
	input.seekg(static_cast<std::streamoff>(count), std::ios::cur);
	return input.good();
}

bool readGgufString(std::ifstream& input, std::string& value) {
	uint64_t size = 0;
	if (!readBinary(input, size)) {
		return false;
	}
	constexpr uint64_t maxReasonableMetadataStringSize = 64ull * 1024ull * 1024ull;
	if (size > maxReasonableMetadataStringSize) {
		return false;
	}
	value.resize(static_cast<std::size_t>(size));
	if (size == 0) {
		return true;
	}
	input.read(&value[0], static_cast<std::streamsize>(size));
	return input.good();
}

uint64_t ggufFixedValueSize(uint32_t type) {
	switch (type) {
	case 0: return 1; // UINT8
	case 1: return 1; // INT8
	case 2: return 2; // UINT16
	case 3: return 2; // INT16
	case 4: return 4; // UINT32
	case 5: return 4; // INT32
	case 6: return 4; // FLOAT32
	case 7: return 1; // BOOL
	case 10: return 8; // UINT64
	case 11: return 8; // INT64
	case 12: return 8; // FLOAT64
	default: return 0;
	}
}

bool skipGgufValue(std::ifstream& input, uint32_t type) {
	constexpr uint32_t ggufTypeString = 8;
	constexpr uint32_t ggufTypeArray = 9;

	if (type == ggufTypeString) {
		std::string ignored;
		return readGgufString(input, ignored);
	}

	if (type == ggufTypeArray) {
		uint32_t elementType = 0;
		uint64_t elementCount = 0;
		if (!readBinary(input, elementType) || !readBinary(input, elementCount)) {
			return false;
		}

		const uint64_t fixedSize = ggufFixedValueSize(elementType);
		if (fixedSize > 0) {
			if (elementCount > std::numeric_limits<uint64_t>::max() / fixedSize) {
				return false;
			}
			return skipGgufBytes(input, elementCount * fixedSize);
		}

		for (uint64_t i = 0; i < elementCount; ++i) {
			if (!skipGgufValue(input, elementType)) {
				return false;
			}
		}
		return true;
	}

	const uint64_t fixedSize = ggufFixedValueSize(type);
	return fixedSize > 0 && skipGgufBytes(input, fixedSize);
}

GgufMetadataInfo readGgufMetadataInfo(const std::string& path) {
	GgufMetadataInfo info;
	if (ofToLower(fs::path(path).extension().string()) != ".gguf") {
		return info;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input.is_open()) {
		return info;
	}

	constexpr uint32_t ggufMagic = 0x46554747;
	constexpr uint32_t ggufTypeString = 8;
	uint32_t magic = 0;
	uint32_t version = 0;
	uint64_t tensorCount = 0;
	uint64_t kvCount = 0;
	if (!readBinary(input, magic) || magic != ggufMagic ||
		!readBinary(input, version) ||
		!readBinary(input, tensorCount) ||
		!readBinary(input, kvCount)) {
		return info;
	}

	for (uint64_t i = 0; i < kvCount; ++i) {
		std::string key;
		uint32_t type = 0;
		if (!readGgufString(input, key) || !readBinary(input, type)) {
			return info;
		}

		if (key == "general.architecture" && type == ggufTypeString) {
			if (!readGgufString(input, info.architecture)) {
				return info;
			}
			continue;
		}

		if (!skipGgufValue(input, type)) {
			return info;
		}
	}

	for (uint64_t i = 0; i < tensorCount; ++i) {
		std::string name;
		uint32_t nDims = 0;
		uint32_t type = 0;
		uint64_t offset = 0;
		if (!readGgufString(input, name) || !readBinary(input, nDims)) {
			return info;
		}
		info.maxTensorDimensions = std::max(info.maxTensorDimensions, nDims);
		if (nDims > 64) {
			return info;
		}
		if (!skipGgufBytes(input, static_cast<uint64_t>(nDims) * sizeof(uint64_t)) ||
			!readBinary(input, type) ||
			!readBinary(input, offset)) {
			return info;
		}
	}

	info.readable = true;
	return info;
}

std::string readGgufArchitecture(const std::string& path) {
	return readGgufMetadataInfo(path).architecture;
}

bool isUnsupportedTextModelArchitecture(const std::string& architecture) {
	const std::string value = ofToLower(architecture);
	if (value.empty()) {
		return false;
	}
	if (value.find("qwen_image") != std::string::npos) {
		return false;
	}

	const std::vector<std::string> textArchitectures = {
		"baichuan",
		"bert",
		"bloom",
		"chatglm",
		"deepseek",
		"deepseek2",
		"falcon",
		"gemma",
		"gpt",
		"glm",
		"llama",
		"mistral",
		"mixtral",
		"phi",
		"qwen",
		"qwen2",
		"qwen3",
		"qwen35",
		"starcoder"
	};
	return std::find(textArchitectures.begin(), textArchitectures.end(), value) != textArchitectures.end();
}

ValidationResult validatePrimaryModelCompatibility(
	const ofxGgmlStableDiffusionContextSettings& settings) {
	const auto validatePath = [](const std::string& path, const char* label) -> ValidationResult {
		if (path.empty()) {
			return {};
		}

		const GgufMetadataInfo metadata = readGgufMetadataInfo(path);
		const std::string architecture = metadata.architecture;
		if (!isUnsupportedTextModelArchitecture(architecture)) {
			return {};
		}

		return {
			ofxGgmlStableDiffusionErrorCode::ModelLoadFailed,
			std::string("Unsupported GGUF architecture '") + architecture +
				"' for " + label + ": " + path +
				". Select a stable-diffusion.cpp image model instead of a language model."
		};
	};

	ValidationResult result = validatePath(settings.modelPath, "main model");
	if (!result.ok()) {
		return result;
	}
	result = validatePath(settings.diffusionModelPath, "diffusion model");
	if (!result.ok()) {
		return result;
	}

	const std::string diffusionArchitecture = readGgufArchitecture(settings.diffusionModelPath);
	if (ofToLower(diffusionArchitecture) == "wan") {
		if (settings.t5xxlPath.empty()) {
			return {
				ofxGgmlStableDiffusionErrorCode::InvalidParameter,
				"WAN diffusion models require a UMT5 / T5XXL text encoder path before loading context."
			};
		}
		if (settings.vaePath.empty()) {
			return {
				ofxGgmlStableDiffusionErrorCode::InvalidParameter,
				"WAN diffusion models require a WAN VAE path before loading context."
			};
		}
	}

	return {};
}

bool contextSettingsEquivalent(
	const ofxGgmlStableDiffusionContextSettings& lhs,
	const ofxGgmlStableDiffusionContextSettings& rhs) {
	return lhs == rhs;
}

} // namespace

ofxGgmlStableDiffusion::ofxGgmlStableDiffusion() {
	sd_set_log_callback(sd_log_cb, nullptr);
	thread.userData = this;
	cachedResolvedSchedulersBySampleMethod.assign(static_cast<std::size_t>(SAMPLE_METHOD_COUNT), SCHEDULER_COUNT);
}

ofxGgmlStableDiffusion::~ofxGgmlStableDiffusion() {
	if (thread.isThreadRunning()) {
		thread.waitForThread(true);
	}
	thread.clearContexts();
}

void ofxGgmlStableDiffusion::configureContext(const ofxGgmlStableDiffusionContextSettings& settings) {
	if (settings.nThreads == 0 || settings.nThreads < -1) {
		activeTask = ofxGgmlStableDiffusionTask::LoadModel;
		setLastError(ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Thread count must be -1 (auto) or a positive value");
		return;
	}

	const ofxGgmlStableDiffusionContextSettings resolvedSettings = resolveContextModelPaths(settings);
	if (!hasPrimaryContextModelPath(resolvedSettings)) {
		activeTask = ofxGgmlStableDiffusionTask::LoadModel;
		setLastError(
			ofxGgmlStableDiffusionErrorCode::ModelNotFound,
			"No primary model path configured. Select a main model or diffusion model before loading context.");
		return;
	}

	const std::vector<std::string> missingPaths = describeMissingContextModelPaths(resolvedSettings);
	if (!missingPaths.empty()) {
		std::string message = "Missing model files: ";
		for (std::size_t i = 0; i < missingPaths.size(); ++i) {
			message += missingPaths[i];
			if (i + 1 < missingPaths.size()) {
				message += "; ";
			}
		}
		activeTask = ofxGgmlStableDiffusionTask::LoadModel;
		setLastError(ofxGgmlStableDiffusionErrorCode::ModelNotFound, message);
		return;
	}

	const ValidationResult compatibilityResult = validatePrimaryModelCompatibility(resolvedSettings);
	if (!compatibilityResult.ok()) {
		activeTask = ofxGgmlStableDiffusionTask::LoadModel;
		setLastError(compatibilityResult.code, compatibilityResult.message);
		return;
	}

	if (!beginBackgroundTask(ofxGgmlStableDiffusionTask::LoadModel)) {
		return;
	}

	applyContextSettings(resolvedSettings);
	ofxGgmlStableDiffusionThread::ContextTaskData taskData;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		taskData.contextSettings = captureContextSettingsNoLock();
		taskData.upscalerSettings = captureUpscalerSettingsNoLock();
	}
	thread.prepareContextTask(taskData);
	thread.startThread();
}

void ofxGgmlStableDiffusion::generate(const ofxGgmlStableDiffusionImageRequest& request) {
	const ofxGgmlStableDiffusionTask task = ofxGgmlStableDiffusionTaskForImageMode(request.mode);
	if (!validateImageRequestAndSetError(request, task)) {
		return;
	}
	if (!beginBackgroundTask(task)) {
		return;
	}
	if (!applyImageRequest(request)) {
		return;
	}
	thread.startThread();
}

void ofxGgmlStableDiffusion::generateVideo(const ofxGgmlStableDiffusionVideoRequest& request) {
	if (!validateVideoRequestAndSetError(request)) {
		return;
	}
	const ofxGgmlStableDiffusionCapabilities capabilities = getCapabilities();
	if (!capabilities.contextConfigured) {
		setLastError(ofxGgmlStableDiffusionErrorCode::ModelNotFound, "Video generation requires a loaded model");
		return;
	}
	if (!capabilities.imageToVideo) {
		setLastError(ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Current model does not support image-to-video generation");
		return;
	}
	if (request.endImage.data != nullptr && !capabilities.videoEndFrame) {
		setLastError(ofxGgmlStableDiffusionErrorCode::InvalidParameter, "This model does not support providing an end frame");
		return;
	}
	if (request.hasAnimation() && !capabilities.videoAnimation) {
		setLastError(ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Animated video generation is not supported by the current model");
		return;
	}
	if (!beginBackgroundTask(ofxGgmlStableDiffusionTask::ImageToVideo)) {
		return;
	}
	if (!applyVideoRequest(request)) {
		finishBackgroundTask();
		return;
	}
	thread.startThread();
}

void ofxGgmlStableDiffusion::setUpscalerSettings(const ofxGgmlStableDiffusionUpscalerSettings& settings) {
	const ValidationResult validation = validateUpscalerSettings(settings);
	if (!validation.ok()) {
		activeTask = ofxGgmlStableDiffusionTask::Upscale;
		setLastError(validation.code, validation.message);
		return;
	}

	std::lock_guard<std::mutex> lock(stateMutex);
	cachedUpscalerSettings = settings;
	esrganPath = settings.modelPath;
	esrganMultiplier = settings.multiplier;
	isESRGAN = settings.enabled;
}

ofxGgmlStableDiffusionContextSettings ofxGgmlStableDiffusion::getContextSettings() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return captureContextSettingsNoLock();
}

ofxGgmlStableDiffusionUpscalerSettings ofxGgmlStableDiffusion::getUpscalerSettings() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return captureUpscalerSettingsNoLock();
}

ofxGgmlStableDiffusionCapabilities ofxGgmlStableDiffusion::getCapabilities() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return ofxGgmlStableDiffusionCapabilityHelpers::resolveCapabilities(
		captureContextSettingsNoLock(),
		captureUpscalerSettingsNoLock());
}

ofxGgmlStableDiffusionResult ofxGgmlStableDiffusion::getLastResult() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult;
}

std::vector<ofxGgmlStableDiffusionImageFrame> ofxGgmlStableDiffusion::getImages() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult.images;
}

ofxGgmlStableDiffusionVideoClip ofxGgmlStableDiffusion::getVideoClip() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult.video;
}

bool ofxGgmlStableDiffusion::hasImageResult() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult.hasImages();
}

bool ofxGgmlStableDiffusion::hasVideoResult() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult.hasVideo();
}

bool ofxGgmlStableDiffusion::hasLoadedContext() const {
	return thread.hasLoadedContext();
}

int ofxGgmlStableDiffusion::getOutputCount() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (lastResult.hasVideo()) {
		return static_cast<int>(lastResult.video.frames.size());
	}
	return static_cast<int>(lastResult.images.size());
}

std::string ofxGgmlStableDiffusion::getLastError() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastError;
}

ofxGgmlStableDiffusionErrorCode ofxGgmlStableDiffusion::getLastErrorCode() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastErrorInfo.code;
}

ofxGgmlStableDiffusionError ofxGgmlStableDiffusion::getLastErrorInfo() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastErrorInfo;
}

std::string ofxGgmlStableDiffusion::getLastResolvedVideoRequestSummary() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResolvedVideoRequestSummary;
}

std::string ofxGgmlStableDiffusion::getLastResolvedVideoCliCommand() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResolvedVideoCliCommand;
}

sample_method_t ofxGgmlStableDiffusion::getResolvedSampleMethod(sample_method_t requested) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (requested != SAMPLE_METHOD_COUNT) {
		return requested;
	}
	return cachedResolvedDefaultSampleMethod;
}

scheduler_t ofxGgmlStableDiffusion::getResolvedScheduler(
	sample_method_t requestedSampleMethod,
	scheduler_t requestedSchedule) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (requestedSchedule != SCHEDULER_COUNT) {
		return requestedSchedule;
	}
	if (requestedSampleMethod == SAMPLE_METHOD_COUNT) {
		return cachedResolvedDefaultScheduler;
	}
	const int sampleMethodIndex = static_cast<int>(requestedSampleMethod);
	if (sampleMethodIndex < 0 ||
		sampleMethodIndex >= static_cast<int>(cachedResolvedSchedulersBySampleMethod.size())) {
		return SCHEDULER_COUNT;
	}
	return cachedResolvedSchedulersBySampleMethod[static_cast<std::size_t>(sampleMethodIndex)];
}

std::string ofxGgmlStableDiffusion::getResolvedSampleMethodName(sample_method_t requested) const {
	const sample_method_t resolved = getResolvedSampleMethod(requested);
	if (resolved == SAMPLE_METHOD_COUNT) {
		return "MODEL_DEFAULT";
	}
	return sd_sample_method_name(resolved);
}

std::string ofxGgmlStableDiffusion::getResolvedSchedulerName(
	sample_method_t requestedSampleMethod,
	scheduler_t requestedSchedule) const {
	const scheduler_t resolved = getResolvedScheduler(requestedSampleMethod, requestedSchedule);
	if (resolved == SCHEDULER_COUNT) {
		return "MODEL_DEFAULT";
	}
	return sd_scheduler_name(resolved);
}

std::vector<ofxGgmlStableDiffusionError> ofxGgmlStableDiffusion::getErrorHistory() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return std::vector<ofxGgmlStableDiffusionError>(errorHistory.begin(), errorHistory.end());
}

void ofxGgmlStableDiffusion::clearErrorHistory() {
	std::lock_guard<std::mutex> lock(stateMutex);
	errorHistory.clear();
}

int ofxGgmlStableDiffusion::getVideoFrameIndexForTime(float seconds) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult.video.frameIndexForTime(seconds);
}

const ofPixels* ofxGgmlStableDiffusion::getImagePixels(int index) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (index < 0 || index >= static_cast<int>(lastResult.images.size())) {
		return nullptr;
	}
	return &lastResult.images[static_cast<std::size_t>(index)].pixels;
}

bool ofxGgmlStableDiffusion::copyImagePixels(int index, ofPixels& pixels) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (index < 0 || index >= static_cast<int>(lastResult.images.size())) {
		return false;
	}
	const auto& storedPixels = lastResult.images[static_cast<std::size_t>(index)].pixels;
	if (!storedPixels.isAllocated()) {
		return false;
	}
	pixels = storedPixels;
	return true;
}

bool ofxGgmlStableDiffusion::getImageFrameMetadata(
	int index,
	ofxGgmlStableDiffusionImageScore& score,
	bool& isSelected) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (index < 0 || index >= static_cast<int>(lastResult.images.size())) {
		return false;
	}
	const auto& frame = lastResult.images[static_cast<std::size_t>(index)];
	score = frame.score;
	isSelected = frame.isSelected;
	return true;
}

const ofPixels* ofxGgmlStableDiffusion::getVideoFramePixels(int index) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (index < 0 || index >= static_cast<int>(lastResult.video.frames.size())) {
		return nullptr;
	}
	return &lastResult.video.frames[static_cast<std::size_t>(index)].pixels;
}

bool ofxGgmlStableDiffusion::copyVideoFramePixels(int index, ofPixels& pixels) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (index < 0 || index >= static_cast<int>(lastResult.video.frames.size())) {
		return false;
	}
	const auto& storedPixels = lastResult.video.frames[static_cast<std::size_t>(index)].pixels;
	if (!storedPixels.isAllocated()) {
		return false;
	}
	pixels = storedPixels;
	return true;
}

bool ofxGgmlStableDiffusion::getVideoFrameMetadata(
	int index,
	int64_t& seed,
	ofxGgmlStableDiffusionGenerationParameters& generation) const {
	std::lock_guard<std::mutex> lock(stateMutex);
	if (index < 0 || index >= static_cast<int>(lastResult.video.frames.size())) {
		return false;
	}
	const auto& frame = lastResult.video.frames[static_cast<std::size_t>(index)];
	seed = frame.seed;
	generation = frame.generation;
	return true;
}

bool ofxGgmlStableDiffusion::saveVideoFrames(const std::string& directory, const std::string& prefix) const {
	return getVideoClip().saveFrameSequence(directory, prefix);
}

bool ofxGgmlStableDiffusion::saveVideoMetadata(const std::string& path) const {
	return getVideoClip().saveMetadataJson(path);
}

bool ofxGgmlStableDiffusion::saveVideoFramesWithMetadata(
	const std::string& directory,
	const std::string& prefix,
	const std::string& metadataFilename) const {
	return getVideoClip().saveFrameSequenceWithMetadata(directory, prefix, metadataFilename);
}

bool ofxGgmlStableDiffusion::saveVideoWebm(const std::string& path, int quality) const {
	return getVideoClip().saveWebm(path, quality);
}

ofxGgmlStableDiffusionLongVideoRunResult ofxGgmlStableDiffusion::renderLongVideo(
	const ofxGgmlStableDiffusionLongVideoManifest& manifest,
	const std::string& framePrefix,
	const std::string& metadataFilename,
	int pollIntervalMs) {
	ofxGgmlStableDiffusionLongVideoRunResult runResult;

	const auto validation = ofxGgmlStableDiffusionLongVideoWorkflow::validate(manifest);
	if (!validation.ok) {
		runResult.success = false;
		runResult.error = validation.errors.empty()
			? "Long-video manifest validation failed."
			: validation.errors.front();
		return runResult;
	}

	const ofxGgmlStableDiffusionCapabilities capabilities = getCapabilities();
	if (!capabilities.contextConfigured) {
		runResult.success = false;
		runResult.error = "Long-video rendering requires a loaded model.";
		return runResult;
	}
	if (!capabilities.imageToVideo) {
		runResult.success = false;
		runResult.error = "Current model does not support image-to-video generation.";
		return runResult;
	}

	const int sleepMs = pollIntervalMs <= 0 ? 10 : pollIntervalMs;

		ofPixels previousLastFrame;
	bool hasPreviousLastFrame = false;

	runResult.chunks.reserve(manifest.chunks.size());
	for (std::size_t i = 0; i < manifest.chunks.size(); ++i) {
		const auto& chunk = manifest.chunks[i];
		ofxGgmlStableDiffusionLongVideoChunkResult chunkResult;
		chunkResult.chunkId = chunk.id.empty() ? ("chunk-" + std::to_string(i + 1)) : chunk.id;

		ofxGgmlStableDiffusionVideoRequest request =
			ofxGgmlStableDiffusionLongVideoWorkflow::buildChunkRequest(manifest, chunk);

		ofxGgmlStableDiffusionThread::OwnedImage initImage;
		if (chunk.usePreviousLastFrame && hasPreviousLastFrame) {
			const sd_image_t initView{
				static_cast<uint32_t>(previousLastFrame.getWidth()),
				static_cast<uint32_t>(previousLastFrame.getHeight()),
				static_cast<uint32_t>(previousLastFrame.getNumChannels()),
				previousLastFrame.getData()};
			initImage.assign(initView);
			request.initImage = initImage.image;
		}

		generateVideo(request);
		while (isGenerating()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
		}

		const ofxGgmlStableDiffusionResult result = getLastResult();
		if (!result.success || !result.hasVideo()) {
			chunkResult.success = false;
			chunkResult.error = getLastError();
			runResult.chunks.push_back(chunkResult);
			runResult.success = false;
			runResult.error = chunkResult.error.empty() ? "Chunk generation failed." : chunkResult.error;
			return runResult;
		}

		const std::string clipDirectory =
			ofxGgmlStableDiffusionLongVideoWorkflow::buildChunkOutputDirectory(manifest, chunk);
		if (!saveVideoFramesWithMetadata(clipDirectory, framePrefix, metadataFilename)) {
			chunkResult.success = false;
			chunkResult.error = "Failed to save chunk frame sequence.";
			runResult.chunks.push_back(chunkResult);
			runResult.success = false;
			runResult.error = chunkResult.error;
			return runResult;
		}

		chunkResult.success = true;
		chunkResult.clipDirectory = clipDirectory;
		chunkResult.metadataPath = ofxGgmlStableDiffusionLongVideoWorkflow::joinPath(clipDirectory, metadataFilename);
		chunkResult.actualSeed = result.actualSeedUsed;
		chunkResult.renderedFrameCount = static_cast<int>(result.video.frames.size());
		runResult.chunks.push_back(chunkResult);

		const auto clip = getVideoClip();
		if (!clip.frames.empty() && clip.frames.back().pixels.isAllocated()) {
			previousLastFrame = clip.frames.back().pixels;
			hasPreviousLastFrame = true;
		} else {
			hasPreviousLastFrame = false;
		}
	}

	runResult.success = true;
	runResult.playlistManifestJson =
		ofxGgmlStableDiffusionLongVideoWorkflow::buildPlaylistManifestJson(manifest, runResult.chunks);
	return runResult;
}

void ofxGgmlStableDiffusion::setVideoGenerationMode(ofxGgmlStableDiffusionVideoMode mode) {
	std::lock_guard<std::mutex> lock(stateMutex);
	videoMode = mode;
}

ofxGgmlStableDiffusionVideoMode ofxGgmlStableDiffusion::getVideoGenerationMode() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return videoMode;
}

void ofxGgmlStableDiffusion::setImageGenerationMode(ofxGgmlStableDiffusionImageMode mode) {
	std::lock_guard<std::mutex> lock(stateMutex);
	imageMode = mode;
	isTextToImage.store(mode == ofxGgmlStableDiffusionImageMode::TextToImage, std::memory_order_relaxed);
	isImageToVideo.store(false, std::memory_order_relaxed);
}

ofxGgmlStableDiffusionImageMode ofxGgmlStableDiffusion::getImageGenerationMode() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return imageMode;
}

void ofxGgmlStableDiffusion::setImageSelectionMode(ofxGgmlStableDiffusionImageSelectionMode mode) {
	std::lock_guard<std::mutex> lock(stateMutex);
	imageSelectionMode = mode;
}

ofxGgmlStableDiffusionImageSelectionMode ofxGgmlStableDiffusion::getImageSelectionMode() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return imageSelectionMode;
}

void ofxGgmlStableDiffusion::setImageRankCallback(ofxSdImageRankCallback cb) {
	std::lock_guard<std::mutex> lock(stateMutex);
	imageRankCallback = cb;
}

int ofxGgmlStableDiffusion::getSelectedImageIndex() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult.selectedImageIndex;
}

void ofxGgmlStableDiffusion::loadImage(const ofPixels& pixels) {
	std::lock_guard<std::mutex> lock(stateMutex);
	// Create a temporary sd_image_t with the pixel data, then copy via assign()
	sd_image_t tempImage{
		static_cast<uint32_t>(pixels.getWidth()),
		static_cast<uint32_t>(pixels.getHeight()),
		static_cast<uint32_t>(pixels.getNumChannels()),
		const_cast<unsigned char*>(pixels.getData())
	};
	loadedInputImage.assign(tempImage);
	inputImage = loadedInputImage.image;
}

void ofxGgmlStableDiffusion::setLoras(const std::vector<ofxGgmlStableDiffusionLora>& loras_) {
	std::lock_guard<std::mutex> lock(stateMutex);
	loras = loras_;
}

std::vector<ofxGgmlStableDiffusionLora> ofxGgmlStableDiffusion::getLoras() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return loras;
}

std::vector<std::pair<std::string, std::string>> ofxGgmlStableDiffusion::listLoras() const {
	std::vector<std::pair<std::string, std::string>> results;
	std::string targetDir;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		targetDir = loraModelDir;
	}
	if (targetDir.empty()) {
		return results;
	}

	ofDirectory dir(targetDir);
	if (!dir.exists()) {
		return results;
	}
	dir.allowExt("safetensors");
	dir.allowExt("ckpt");
	dir.allowExt("pt");
	dir.allowExt("bin");
	dir.listDir();

	for (std::size_t i = 0; i < dir.size(); ++i) {
		const ofFile& file = dir.getFile(static_cast<int>(i));
		if (file.isFile()) {
			results.emplace_back(file.getBaseName(), file.getAbsolutePath());
		}
	}
	return results;
}

std::vector<ofxGgmlStableDiffusionModelInfo> ofxGgmlStableDiffusion::scanModels(const std::string& directory) {
	return modelManager.scanModelsInDirectory(directory);
}

ofxGgmlStableDiffusionModelInfo ofxGgmlStableDiffusion::getModelInfo(const std::string& modelPath) {
	return modelManager.extractModelInfo(modelPath);
}

std::vector<ofxGgmlStableDiffusionModelInfo> ofxGgmlStableDiffusion::getCachedModels() const {
	return modelManager.getCachedModels();
}

bool ofxGgmlStableDiffusion::preloadModel(const std::string& modelPath, std::string& errorMessage) {
	ofxGgmlStableDiffusionModelInfo info = modelManager.extractModelInfo(modelPath);
	return modelManager.preloadModel(info, errorMessage);
}

void ofxGgmlStableDiffusion::clearModelCache() {
	modelManager.clearCache();
}

void ofxGgmlStableDiffusion::setModelCacheSize(uint64_t maxBytes) {
	modelManager.setMaxCacheSize(maxBytes);
}

void ofxGgmlStableDiffusion::setMaxCachedModels(int count) {
	modelManager.setMaxCachedModels(count);
}

void ofxGgmlStableDiffusion::setProfilingEnabled(bool enabled) {
	performanceProfiler.setEnabled(enabled);
}

bool ofxGgmlStableDiffusion::isProfilingEnabled() const {
	return performanceProfiler.isEnabled();
}

ofxGgmlStableDiffusionPerformanceStats ofxGgmlStableDiffusion::getPerformanceStats() const {
	return performanceProfiler.getStats();
}

ofxGgmlStableDiffusionProfileEntry ofxGgmlStableDiffusion::getPerformanceEntry(const std::string& name) const {
	return performanceProfiler.getEntry(name);
}

void ofxGgmlStableDiffusion::resetProfiling() {
	performanceProfiler.reset();
}

void ofxGgmlStableDiffusion::printPerformanceSummary() const {
	performanceProfiler.printSummary();
}

std::vector<std::string> ofxGgmlStableDiffusion::getPerformanceBottlenecks(float thresholdPercent) const {
	return performanceProfiler.getBottlenecks(thresholdPercent);
}

std::string ofxGgmlStableDiffusion::exportPerformanceJSON() const {
	return performanceProfiler.toJSON();
}

std::string ofxGgmlStableDiffusion::exportPerformanceCSV() const {
	return performanceProfiler.toCSV();
}

void ofxGgmlStableDiffusion::addControlNet(const ofxGgmlStableDiffusionControlNet& controlNet) {
	std::lock_guard<std::mutex> lock(stateMutex);
	controlNets.push_back(controlNet);
}

void ofxGgmlStableDiffusion::clearControlNets() {
	std::lock_guard<std::mutex> lock(stateMutex);
	controlNets.clear();
}

std::vector<ofxGgmlStableDiffusionControlNet> ofxGgmlStableDiffusion::getControlNets() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return controlNets;
}

void ofxGgmlStableDiffusion::reloadEmbeddings(const std::string& embedDir) {
	ofxGgmlStableDiffusionContextSettings settings = getContextSettings();
	if (!embedDir.empty()) {
		settings.embedDir = embedDir;
	}
	if (settings.modelPath.empty() && settings.diffusionModelPath.empty()) {
		std::lock_guard<std::mutex> lock(stateMutex);
		embedDirCStr = settings.embedDir;
		return;
	}
	newSdCtx(settings);
}

std::vector<std::pair<std::string, std::string>> ofxGgmlStableDiffusion::listEmbeddings() const {
	std::vector<std::pair<std::string, std::string>> results;
	std::string targetDir;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		targetDir = embedDirCStr;
	}
	if (targetDir.empty()) {
		return results;
	}

	ofDirectory dir(targetDir);
	if (!dir.exists()) {
		return results;
	}
	dir.allowExt("pt");
	dir.allowExt("ckpt");
	dir.allowExt("safetensors");
	dir.allowExt("bin");
	dir.allowExt("gguf");
	dir.listDir();

	for (std::size_t i = 0; i < dir.size(); ++i) {
		const ofFile& file = dir.getFile(static_cast<int>(i));
		if (file.isFile()) {
			results.emplace_back(file.getBaseName(), file.getAbsolutePath());
		}
	}
	return results;
}

bool ofxGgmlStableDiffusion::isDiffused() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return diffused;
}

void ofxGgmlStableDiffusion::setDiffused(bool diffused_) {
	std::lock_guard<std::mutex> lock(stateMutex);
	diffused = diffused_;
}

sd_image_t* ofxGgmlStableDiffusion::returnImages() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return outputImages;
}

const char* ofxGgmlStableDiffusion::typeName(enum sd_type_t type) {
	return sd_type_name(type);
}

int32_t ofxGgmlStableDiffusion::getNumPhysicalCores() {
	return sd_get_num_physical_cores();
}

const char* ofxGgmlStableDiffusion::getSystemInfo() {
	return sd_get_system_info();
}

void ofxGgmlStableDiffusion::setProgressCallback(ofxSdProgressCallback cb) {
	std::lock_guard<std::mutex> lock(stateMutex);
	progressCallback = cb;
}

void ofxGgmlStableDiffusion::setNativeLoggingEnabled(bool enabled) {
	g_sdLoggingEnabled.store(enabled);
}

bool ofxGgmlStableDiffusion::isNativeLoggingEnabled() const {
	return g_sdLoggingEnabled.load();
}

void ofxGgmlStableDiffusion::setNativeLogLevel(sd_log_level_t level) {
	g_sdMinLogLevel.store(static_cast<int>(level));
}

sd_log_level_t ofxGgmlStableDiffusion::getNativeLogLevel() const {
	return static_cast<sd_log_level_t>(g_sdMinLogLevel.load());
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusion::newSdCtx(const ofxGgmlStableDiffusionContextSettings& settings) {
	configureContext(settings);
}

void ofxGgmlStableDiffusion::freeSdCtx() {
	if (thread.isThreadRunning()) {
		thread.waitForThread(true);
	}
	thread.clearContexts();
	std::lock_guard<std::mutex> lock(stateMutex);
	clearResolvedDefaultCachesNoLock();
}

void ofxGgmlStableDiffusion::txt2img(const std::string& prompt_,
	const std::string& negativePrompt_,
	int clipSkip_,
	float cfgScale_,
	int width_,
	int height_,
	sample_method_t sampleMethod_,
	int sampleSteps_,
	int64_t seed_,
	int batchCount_,
	sd_image_t* controlCond_,
	float controlStrength_,
	float styleStrength_,
	const std::string& inputIdImagesPath_) {
	if (thread.isThreadRunning()) {
		setLastError(ofxGgmlStableDiffusionErrorCode::ThreadBusy, "A task is already running");
		return;
	}

	const ValidationResult dimResult = validateDimensions(width_, height_);
	if (!dimResult.ok()) {
		imageMode = ofxGgmlStableDiffusionImageMode::TextToImage;
		activeTask = ofxGgmlStableDiffusionTask::TextToImage;
		setLastError(dimResult.code, dimResult.message);
		return;
	}
	const ValidationResult batchResult = validateBatchCount(batchCount_);
	if (!batchResult.ok()) {
		imageMode = ofxGgmlStableDiffusionImageMode::TextToImage;
		activeTask = ofxGgmlStableDiffusionTask::TextToImage;
		setLastError(batchResult.code, batchResult.message);
		return;
	}

	ofxGgmlStableDiffusionImageRequest request;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		request.selectionMode = imageSelectionMode;
		request.loras = loras;
	}
	request.mode = ofxGgmlStableDiffusionImageMode::TextToImage;
	request.prompt = prompt_;
	request.negativePrompt = negativePrompt_;
	request.clipSkip = clipSkip_;
	request.cfgScale = cfgScale_;
	request.width = width_;
	request.height = height_;
	request.sampleMethod = sampleMethod_;
	request.sampleSteps = sampleSteps_;
	request.seed = seed_;
	request.batchCount = batchCount_;
	request.controlCond = controlCond_;
	request.controlStrength = controlStrength_;
	request.styleStrength = styleStrength_;
	request.inputIdImagesPath = inputIdImagesPath_;
	generate(request);
}

void ofxGgmlStableDiffusion::img2img(sd_image_t initImage_,
	const std::string& prompt_,
	const std::string& negativePrompt_,
	int clipSkip_,
	float cfgScale_,
	int width_,
	int height_,
	enum sample_method_t sampleMethod_,
	int sampleSteps_,
	float strength_,
	int64_t seed_,
	int batchCount_,
	sd_image_t* controlCond_,
	float controlStrength_,
	float styleStrength_,
	const std::string& inputIdImagesPath_) {
	if (thread.isThreadRunning()) {
		setLastError(ofxGgmlStableDiffusionErrorCode::ThreadBusy, "A task is already running");
		return;
	}

	if (initImage_.data == nullptr) {
		imageMode = ofxGgmlStableDiffusionImageMode::ImageToImage;
		activeTask = ofxGgmlStableDiffusionTask::ImageToImage;
		setLastError(ofxGgmlStableDiffusionErrorCode::MissingInputImage, "Image-to-image requires an input image");
		return;
	}

	const ValidationResult dimResult = validateDimensions(width_, height_);
	if (!dimResult.ok()) {
		imageMode = ofxGgmlStableDiffusionImageMode::ImageToImage;
		activeTask = ofxGgmlStableDiffusionTask::ImageToImage;
		setLastError(dimResult.code, dimResult.message);
		return;
	}
	const ValidationResult batchResult = validateBatchCount(batchCount_);
	if (!batchResult.ok()) {
		imageMode = ofxGgmlStableDiffusionImageMode::ImageToImage;
		activeTask = ofxGgmlStableDiffusionTask::ImageToImage;
		setLastError(batchResult.code, batchResult.message);
		return;
	}

	ofxGgmlStableDiffusionImageRequest request;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		request.selectionMode = imageSelectionMode;
		request.loras = loras;
	}
	request.mode = ofxGgmlStableDiffusionImageMode::ImageToImage;
	request.initImage = initImage_;
	request.prompt = prompt_;
	request.negativePrompt = negativePrompt_;
	request.clipSkip = clipSkip_;
	request.cfgScale = cfgScale_;
	request.width = width_;
	request.height = height_;
	request.sampleMethod = sampleMethod_;
	request.sampleSteps = sampleSteps_;
	request.strength = strength_;
	request.seed = seed_;
	request.batchCount = batchCount_;
	request.controlCond = controlCond_;
	request.controlStrength = controlStrength_;
	request.styleStrength = styleStrength_;
	request.inputIdImagesPath = inputIdImagesPath_;
	generate(request);
}

void ofxGgmlStableDiffusion::img2vid(sd_image_t initImage_,
	int width_,
	int height_,
	int videoFrames_,
	int fps_,
	float cfgScale_,
	enum sample_method_t sampleMethod_,
	int sampleSteps_,
	float strength_,
	int64_t seed_) {
	if (thread.isThreadRunning()) {
		setLastError(ofxGgmlStableDiffusionErrorCode::ThreadBusy, "A task is already running");
		return;
	}

	if (initImage_.data == nullptr) {
		activeTask = ofxGgmlStableDiffusionTask::ImageToVideo;
		setLastError(ofxGgmlStableDiffusionErrorCode::MissingInputImage, "Image-to-video requires an input image");
		return;
	}

	const ValidationResult dimResult = validateDimensions(width_, height_);
	if (!dimResult.ok()) {
		activeTask = ofxGgmlStableDiffusionTask::ImageToVideo;
		setLastError(dimResult.code, dimResult.message);
		return;
	}
	if (videoFrames_ <= 0) {
		activeTask = ofxGgmlStableDiffusionTask::ImageToVideo;
		setLastError(ofxGgmlStableDiffusionErrorCode::InvalidFrameCount, "Frame count must be positive");
		return;
	}

	ofxGgmlStableDiffusionVideoRequest request;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		request.prompt = prompt;
		request.negativePrompt = negativePrompt;
		request.clipSkip = clipSkip;
		request.vaceStrength = vaceStrength;
		request.mode = videoMode;
		request.loras = loras;
	}
	request.initImage = initImage_;
	request.width = width_;
	request.height = height_;
	request.frameCount = videoFrames_;
	request.fps = fps_;
	request.cfgScale = cfgScale_;
	request.sampleMethod = sampleMethod_;
	request.sampleSteps = sampleSteps_;
	request.strength = strength_;
	request.seed = seed_;
	generateVideo(request);
}

void ofxGgmlStableDiffusion::newUpscalerCtx(const std::string& esrganPath_,
	int nThreads_,
	enum sd_type_t wType_) {
	int requestedMultiplier = 4;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		requestedMultiplier = esrganMultiplier;
	}
	const ofxGgmlStableDiffusionUpscalerSettings requested{
		esrganPath_,
		nThreads_,
		wType_,
		requestedMultiplier,
		true
	};

	const ValidationResult validation = validateUpscalerSettings(requested);
	activeTask = ofxGgmlStableDiffusionTask::Upscale;
	if (!validation.ok()) {
		setLastError(validation.code, validation.message);
		return;
	}

	if (thread.isThreadRunning()) {
		setLastError(ofxGgmlStableDiffusionErrorCode::ThreadBusy, "Cannot rebuild the upscaler while another task is running");
		return;
	}

	setUpscalerSettings(requested);
	if (thread.upscalerCtx) {
		free_upscaler_ctx(thread.upscalerCtx);
		thread.upscalerCtx = nullptr;
	}

	thread.upscalerCtx = new_upscaler_ctx(esrganPath_.c_str(), false, nThreads_, 0, nullptr, nullptr);
	if (!thread.upscalerCtx) {
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			isESRGAN = false;
		}
		setLastError(ofxGgmlStableDiffusionErrorCode::UpscaleFailed, "Failed to create upscaler context");
		return;
	}

	{
		std::lock_guard<std::mutex> lock(stateMutex);
		isESRGAN = true;
	}
}

void ofxGgmlStableDiffusion::freeUpscalerCtx() {
	if (thread.isThreadRunning()) {
		thread.waitForThread(true);
	}
	if (thread.upscalerCtx) {
		free_upscaler_ctx(thread.upscalerCtx);
		thread.upscalerCtx = nullptr;
	}
	std::lock_guard<std::mutex> lock(stateMutex);
	isESRGAN = false;
}

sd_image_t ofxGgmlStableDiffusion::upscaleImage(sd_image_t inputImage_, uint32_t upscaleFactor) {
	if (thread.isThreadRunning()) {
		setLastError(ofxGgmlStableDiffusionErrorCode::ThreadBusy, "Cannot upscale while another task is running");
		return {0, 0, 0, nullptr};
	}
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		activeTask = ofxGgmlStableDiffusionTask::Upscale;
	}
	if (upscaleFactor == 0) {
		setLastError(ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Upscale factor must be at least 1");
		return {0, 0, 0, nullptr};
	}
	if (!thread.upscalerCtx) {
		setLastError(ofxGgmlStableDiffusionErrorCode::UpscaleFailed, "Upscaler context is not initialized");
		return {0, 0, 0, nullptr};
	}
	sd_image_t* output = nullptr;
	int outputCount = 0;
	if (!upscale(thread.upscalerCtx, inputImage_, upscaleFactor, &output, &outputCount) ||
		!output || outputCount < 1) {
		setLastError(ofxGgmlStableDiffusionErrorCode::UpscaleFailed, "Upscaling returned no image");
		return {0, 0, 0, nullptr};
	}
	sd_image_t result = output[0];
	free(output);
	return result;
}

bool ofxGgmlStableDiffusion::convert(const char* inputPath_, const char* vaePath_, const char* outputPath_, sd_type_t outputType_) {
	return ::convert(inputPath_, vaePath_, outputPath_, outputType_, nullptr, false);
}

uint8_t* ofxGgmlStableDiffusion::preprocessCanny(uint8_t* img,
	int width_,
	int height_,
	float highThreshold,
	float lowThreshold,
	float weak,
	float strong,
	bool inverse,
	int channels) {
	sd_image_t image{
		static_cast<uint32_t>(width_),
		static_cast<uint32_t>(height_),
		static_cast<uint32_t>(channels > 0 ? channels : 1),
		img
	};
	return ::preprocess_canny(image, highThreshold, lowThreshold, weak, strong, inverse) ? img : nullptr;
}

bool ofxGgmlStableDiffusion::isGenerating() const {
	return thread.isThreadRunning();
}

bool ofxGgmlStableDiffusion::isBusy() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return thread.isThreadRunning() || isModelLoading.load(std::memory_order_acquire);
}

bool ofxGgmlStableDiffusion::requestCancellation() {
	if (!isGenerating()) {
		return false;
	}
	thread.requestCancellation();
	return true;
}

bool ofxGgmlStableDiffusion::isCancellationRequested() const {
	return thread.isCancellationRequested();
}

bool ofxGgmlStableDiffusion::wasCancelled() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastOperationCancelled;
}

bool ofxGgmlStableDiffusion::matchesContextSettings(
	const ofxGgmlStableDiffusionContextSettings& settings) const {
	const ofxGgmlStableDiffusionContextSettings resolvedSettings =
		resolveContextModelPaths(settings);
	std::lock_guard<std::mutex> lock(stateMutex);
	return contextSettingsEquivalent(
		captureContextSettingsNoLock(),
		resolvedSettings);
}

int64_t ofxGgmlStableDiffusion::getLastUsedSeed() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return lastResult.actualSeedUsed;
}

std::vector<int64_t> ofxGgmlStableDiffusion::getSeedHistory() const {
	std::lock_guard<std::mutex> lock(stateMutex);
	return std::vector<int64_t>(seedHistory.begin(), seedHistory.end());
}

void ofxGgmlStableDiffusion::clearSeedHistory() {
	std::lock_guard<std::mutex> lock(stateMutex);
	seedHistory.clear();
}

int64_t ofxGgmlStableDiffusion::hashStringToSeed(const std::string& text) {
	return ofxGgmlStableDiffusionHashStringToSeed(text);
}

bool ofxGgmlStableDiffusion::beginBackgroundTask(ofxGgmlStableDiffusionTask task) {
	if (thread.isThreadRunning()) {
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			activeTask = task;
		}
		setLastError(ofxGgmlStableDiffusionErrorCode::ThreadBusy, "Another task is still running");
		return false;
	}

	taskStartMicros = ofGetElapsedTimeMicros();
	isModelLoading.store(task == ofxGgmlStableDiffusionTask::LoadModel, std::memory_order_release);
	isTextToImage.store(task == ofxGgmlStableDiffusionTask::TextToImage, std::memory_order_relaxed);
	isImageToVideo.store(task == ofxGgmlStableDiffusionTask::ImageToVideo, std::memory_order_relaxed);
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		activeTask = task;
		lastOperationCancelled = false;
	}
	clearLastError();
	clearOutputState();
	thread.userData = this;
	thread.resetCancellation();  // Reset cancellation flag for new task
	return true;
}

void ofxGgmlStableDiffusion::finishBackgroundTask(bool cancelled, const std::string& cancelMessage) {
	if (cancelled) {
		setLastError(
			ofxGgmlStableDiffusionErrorCode::Cancelled,
			cancelMessage.empty() ? "Operation cancelled" : cancelMessage);
	}
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		lastOperationCancelled = cancelled;
		activeTask = ofxGgmlStableDiffusionTask::None;
	}
	isModelLoading.store(false, std::memory_order_release);
	isTextToImage.store(false, std::memory_order_relaxed);
	isImageToVideo.store(false, std::memory_order_relaxed);
}

void ofxGgmlStableDiffusion::clearResolvedDefaultCachesNoLock() {
	cachedResolvedDefaultSampleMethod = SAMPLE_METHOD_COUNT;
	cachedResolvedDefaultScheduler = SCHEDULER_COUNT;
	if (cachedResolvedSchedulersBySampleMethod.size() != static_cast<std::size_t>(SAMPLE_METHOD_COUNT)) {
		cachedResolvedSchedulersBySampleMethod.assign(static_cast<std::size_t>(SAMPLE_METHOD_COUNT), SCHEDULER_COUNT);
	} else {
		std::fill(cachedResolvedSchedulersBySampleMethod.begin(), cachedResolvedSchedulersBySampleMethod.end(), SCHEDULER_COUNT);
	}
}

void ofxGgmlStableDiffusion::refreshResolvedDefaultCachesNoLock(sd_ctx_t* ctx) {
	clearResolvedDefaultCachesNoLock();
	if (ctx == nullptr) {
		return;
	}
	cachedResolvedDefaultSampleMethod =
		ofxGgmlStableDiffusionNativeAdapter::resolveSampleMethod(ctx, SAMPLE_METHOD_COUNT);
	cachedResolvedDefaultScheduler =
		ofxGgmlStableDiffusionNativeAdapter::resolveScheduler(
			ctx,
			cachedResolvedDefaultSampleMethod,
			SCHEDULER_COUNT);
	for (int i = 0; i < static_cast<int>(SAMPLE_METHOD_COUNT); ++i) {
		const auto sampleMethod = static_cast<sample_method_t>(i);
		cachedResolvedSchedulersBySampleMethod[static_cast<std::size_t>(i)] =
			ofxGgmlStableDiffusionNativeAdapter::resolveScheduler(
				ctx,
				sampleMethod,
				SCHEDULER_COUNT);
	}
}

ofxGgmlStableDiffusionContextSettings ofxGgmlStableDiffusion::captureContextSettingsNoLock() const {
	ofxGgmlStableDiffusionContextSettings settings;
	settings.modelPath = modelPath;
	settings.diffusionModelPath = diffusionModelPath;
	settings.clipLPath = clipLPath;
	settings.clipGPath = clipGPath;
	settings.t5xxlPath = t5xxlPath;
	settings.vaePath = vaePath;
	settings.taesdPath = taesdPath;
	settings.controlNetPath = controlNetPathCStr;
	settings.loraModelDir = loraModelDir;
	settings.embedDir = embedDirCStr;
	settings.stackedIdEmbedDir = stackedIdEmbedDirCStr;
	settings.vaeDecodeOnly = vaeDecodeOnly;
	settings.vaeTiling = vaeTiling;
	settings.freeParamsImmediately = freeParamsImmediately;
	settings.nThreads = nThreads;
	settings.weightType = wType;
	settings.rngType = rngType;
	settings.schedule = schedule;
	settings.prediction = prediction;
	settings.loraApplyMode = loraApplyMode;
	settings.keepClipOnCpu = keepClipOnCpu;
	settings.keepControlNetCpu = keepControlNetCpu;
	settings.keepVaeOnCpu = keepVaeOnCpu;
	settings.offloadParamsToCpu = offloadParamsToCpu;
	settings.flashAttn = flashAttn;
	settings.diffusionFlashAttn = diffusionFlashAttn;
	settings.enableMmap = enableMmap;
	settings.backend = backend;
	settings.paramsBackend = paramsBackend;
	return settings;
}

ofxGgmlStableDiffusionUpscalerSettings ofxGgmlStableDiffusion::captureUpscalerSettingsNoLock() const {
	return {esrganPath, cachedUpscalerSettings.nThreads, cachedUpscalerSettings.weightType, esrganMultiplier, isESRGAN};
}

void ofxGgmlStableDiffusion::applyContextSettings(const ofxGgmlStableDiffusionContextSettings& settings) {
	const ofxGgmlStableDiffusionContextSettings resolvedSettings = resolveContextModelPaths(settings);
	std::lock_guard<std::mutex> lock(stateMutex);
	clearResolvedDefaultCachesNoLock();
	modelPath = resolvedSettings.modelPath;
	diffusionModelPath = resolvedSettings.diffusionModelPath;
	clipLPath = resolvedSettings.clipLPath;
	clipGPath = resolvedSettings.clipGPath;
	t5xxlPath = resolvedSettings.t5xxlPath;
	modelName = ofFilePath::getFileName(modelPath.empty() ? diffusionModelPath : modelPath);
	vaePath = resolvedSettings.vaePath;
	taesdPath = resolvedSettings.taesdPath;
	controlNetPathCStr = resolvedSettings.controlNetPath;
	loraModelDir = resolvedSettings.loraModelDir;
	embedDirCStr = resolvedSettings.embedDir;
	stackedIdEmbedDirCStr = resolvedSettings.stackedIdEmbedDir;
	vaeDecodeOnly = resolvedSettings.vaeDecodeOnly;
	vaeTiling = resolvedSettings.vaeTiling;
	freeParamsImmediately = resolvedSettings.freeParamsImmediately;
	nThreads = resolvedSettings.nThreads;
	wType = resolvedSettings.weightType;
	rngType = resolvedSettings.rngType;
	schedule = resolvedSettings.schedule;
	prediction = resolvedSettings.prediction;
	loraApplyMode = resolvedSettings.loraApplyMode;
	keepClipOnCpu = resolvedSettings.keepClipOnCpu;
	keepControlNetCpu = resolvedSettings.keepControlNetCpu;
	keepVaeOnCpu = resolvedSettings.keepVaeOnCpu;
	offloadParamsToCpu = resolvedSettings.offloadParamsToCpu;
	flashAttn = resolvedSettings.flashAttn;
	diffusionFlashAttn = resolvedSettings.diffusionFlashAttn;
	enableMmap = resolvedSettings.enableMmap;
	backend = resolvedSettings.backend;
	paramsBackend = resolvedSettings.paramsBackend;
}

bool ofxGgmlStableDiffusion::applyImageRequest(const ofxGgmlStableDiffusionImageRequest& request) {
	ofxGgmlStableDiffusionThread::ImageTaskData taskData;
	bool mergeFailed = false;
	std::string mergeError;
	float mergedStrength = request.controlStrength;

	try {
		std::lock_guard<std::mutex> lock(stateMutex);
		taskData.task = activeTask;
		taskData.contextSettings = captureContextSettingsNoLock();
		taskData.upscalerSettings = captureUpscalerSettingsNoLock();
		taskData.request = request;
		if (request.initImage.data != nullptr) {
			loadedInputImage.assign(request.initImage);
			inputImage = loadedInputImage.image;
		} else if (!ofxGgmlStableDiffusionImageModeUsesInputImage(request.mode)) {
			inputImage = {0, 0, 0, nullptr};
			loadedInputImage.clear();
		}

		if (ofxGgmlStableDiffusionImageModeUsesInputImage(request.mode) && inputImage.data != nullptr) {
			taskData.initImage.assign(inputImage);
		}
		taskData.maskImage.assign(request.maskImage);

		bool hasControl = false;
		if (!request.controlNets.empty()) {
			if (!mergeControlNets(request.controlNets, taskData.controlImage, mergedStrength, mergeError)) {
				mergeFailed = true;
			} else {
				taskData.request.controlCond = &taskData.controlImage.image;
				taskData.request.controlStrength = mergedStrength;
				hasControl = true;
			}
		}

		if (!hasControl && request.controlCond != nullptr) {
			taskData.controlImage.assign(*request.controlCond);
			taskData.request.controlCond = &taskData.controlImage.image;
		}

		taskData.syncViews();
		taskData.progressCallback = progressCallback;
		taskData.imageRankCallback = imageRankCallback;

		imageMode = request.mode;
		imageSelectionMode = request.selectionMode;
		maskImage = request.maskImage;
		endImage = {0, 0, 0, nullptr};
		prompt = request.prompt;
		negativePrompt = request.negativePrompt;
		clipSkip = request.clipSkip;
		cfgScale = request.cfgScale;
		width = request.width;
		height = request.height;
		sampleMethodEnum = request.sampleMethod;
		sampleSteps = request.sampleSteps;
		strength = request.strength;
		seed = request.seed;
		batchCount = request.batchCount;
		controlCond = nullptr;
		controlStrength = taskData.request.controlStrength;
		styleStrength = request.styleStrength;
		inputIdImagesPath = request.inputIdImagesPath;
		loras = request.loras;
	} catch (const std::exception& e) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			std::string("Exception while preparing image request: ") + e.what());
		return false;
	} catch (...) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			"Unknown exception while preparing image request");
		return false;
	}

	if (mergeFailed) {
		const std::string message = mergeError.empty() ?
			"Failed to merge ControlNet inputs" :
			mergeError;
		setLastError(ofxGgmlStableDiffusionErrorCode::InvalidParameter, message);
		return false;
	}

	try {
		thread.prepareImageTask(taskData);
	} catch (const std::exception& e) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			std::string("Exception while starting image task: ") + e.what());
		return false;
	} catch (...) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			"Unknown exception while starting image task");
		return false;
	}

	return true;
}

bool ofxGgmlStableDiffusion::applyVideoRequest(const ofxGgmlStableDiffusionVideoRequest& request) {
	ofxGgmlStableDiffusionThread::VideoTaskData taskData;

	try {
		std::lock_guard<std::mutex> lock(stateMutex);
		taskData.task = activeTask;
		taskData.contextSettings = captureContextSettingsNoLock();
		taskData.upscalerSettings = captureUpscalerSettingsNoLock();
		taskData.request = request;
		loadedInputImage.assign(request.initImage);
		inputImage = loadedInputImage.image;
		taskData.initImage.assign(inputImage);
		taskData.endImage.assign(request.endImage);
		taskData.controlFrames.clear();
		taskData.controlFrames.reserve(request.controlFrames.size());
		for (const auto& frame : request.controlFrames) {
			ofxGgmlStableDiffusionThread::OwnedImage ownedFrame;
			if (ownedFrame.assign(frame)) {
				taskData.controlFrames.push_back(std::move(ownedFrame));
			}
		}
		taskData.syncViews();
		taskData.progressCallback = progressCallback;

		endImage = request.endImage;
		prompt = request.prompt;
		negativePrompt = request.negativePrompt;
		clipSkip = request.clipSkip;
		width = request.width;
		height = request.height;
		videoFrames = request.frameCount;
		fps = request.fps;
		cfgScale = request.cfgScale;
		sampleMethodEnum = request.sampleMethod;
		sampleSteps = request.sampleSteps;
		strength = request.strength;
		seed = request.seed;
		vaceStrength = request.vaceStrength;
		videoMode = request.mode;
		loras = request.loras;
	} catch (const std::exception& e) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			std::string("Exception while preparing video request: ") + e.what());
		return false;
	} catch (...) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			"Unknown exception while preparing video request");
		return false;
	}

	try {
		thread.prepareVideoTask(taskData);
	} catch (const std::exception& e) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			std::string("Exception while starting video task: ") + e.what());
		return false;
	} catch (...) {
		setLastError(ofxGgmlStableDiffusionErrorCode::Unknown,
			"Unknown exception while starting video task");
		return false;
	}
	return true;
}

bool ofxGgmlStableDiffusion::validateImageRequestAndSetError(const ofxGgmlStableDiffusionImageRequest& request, ofxGgmlStableDiffusionTask task) {
	const ValidationResult validation = validateImageRequestNumbers(request);
	imageMode = request.mode;
	activeTask = task;
	if (!validation.ok()) {
		setLastErrorPreservingResult(validation.code, validation.message);
		return false;
	}

	sd_image_t candidateInputImage = request.initImage;
	if (candidateInputImage.data == nullptr) {
		std::lock_guard<std::mutex> lock(stateMutex);
		candidateInputImage = inputImage;
	}
	if (ofxGgmlStableDiffusionImageModeUsesInputImage(request.mode) && candidateInputImage.data == nullptr) {
		setLastErrorPreservingResult(ofxGgmlStableDiffusionErrorCode::MissingInputImage, "Selected image mode requires an input image");
		return false;
	}

	if (request.mode == ofxGgmlStableDiffusionImageMode::Inpainting && request.maskImage.data == nullptr) {
		setLastErrorPreservingResult(ofxGgmlStableDiffusionErrorCode::InvalidParameter, "Inpainting requires a mask image");
		return false;
	}

	// Validate mask dimensions match init image if both are provided
	if (request.mode == ofxGgmlStableDiffusionImageMode::Inpainting &&
		request.maskImage.data != nullptr &&
		candidateInputImage.data != nullptr) {
		if (request.maskImage.width != candidateInputImage.width ||
			request.maskImage.height != candidateInputImage.height) {
			setLastErrorPreservingResult(ofxGgmlStableDiffusionErrorCode::InvalidDimensions,
				"Inpainting mask dimensions must match input image dimensions");
			return false;
		}
	}

	return true;
}

bool ofxGgmlStableDiffusion::validateVideoRequestAndSetError(const ofxGgmlStableDiffusionVideoRequest& request) {
	const ValidationResult validation = validateVideoRequestNumbers(request);
	activeTask = ofxGgmlStableDiffusionTask::ImageToVideo;
	if (!validation.ok()) {
		setLastErrorPreservingResult(validation.code, validation.message);
		return false;
	}
	const ofxGgmlStableDiffusionCapabilities capabilities = getCapabilities();
	if (capabilities.videoRequiresInputImage && request.initImage.data == nullptr) {
		setLastErrorPreservingResult(ofxGgmlStableDiffusionErrorCode::MissingInputImage, "Video generation requires an input image");
		return false;
	}
	if (request.hasAnimation() && isNativeWanVideoFamily(capabilities.modelFamily)) {
		setLastErrorPreservingResult(
			ofxGgmlStableDiffusionErrorCode::InvalidParameter,
			"Wrapper image-sequence animation is not supported for native Wan video models. Use native video diffusion settings instead.");
		return false;
	}
	return true;
}

void ofxGgmlStableDiffusion::clearOutputState() {
	std::lock_guard<std::mutex> lock(stateMutex);
	outputImages = nullptr;
	outputImageViews.clear();
	lastResult = {};
	diffused = false;
	lastResolvedVideoRequestSummary.clear();
	lastResolvedVideoCliCommand.clear();
}

void ofxGgmlStableDiffusion::setLastError(const std::string& errorMessage, ofxGgmlStableDiffusionErrorCode code) {
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		lastError = errorMessage;
		lastErrorInfo.code = code;
		lastErrorInfo.message = errorMessage;
		lastErrorInfo.suggestion = ofxGgmlStableDiffusionErrorCodeSuggestion(code);
		lastErrorInfo.timestampMicros = ofGetElapsedTimeMicros();
		errorHistory.push_back(lastErrorInfo);
		if (errorHistory.size() > maxErrorHistorySize) {
			errorHistory.pop_front();
		}

		lastResult = {};
		lastResult.success = false;
		lastResult.task = activeTask;
		lastResult.imageMode = imageMode;
		lastResult.selectionMode = imageSelectionMode;
		lastResult.error = errorMessage;
		outputImageViews.clear();
		outputImages = nullptr;
		diffused = false;
	}
}

void ofxGgmlStableDiffusion::setLastError(ofxGgmlStableDiffusionErrorCode code, const std::string& errorMessage) {
	setLastError(errorMessage, code);
}

void ofxGgmlStableDiffusion::setLastErrorPreservingResult(
	const std::string& errorMessage,
	ofxGgmlStableDiffusionErrorCode code) {
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		lastError = errorMessage;
		lastErrorInfo.code = code;
		lastErrorInfo.message = errorMessage;
		lastErrorInfo.suggestion = ofxGgmlStableDiffusionErrorCodeSuggestion(code);
		lastErrorInfo.timestampMicros = ofGetElapsedTimeMicros();
		errorHistory.push_back(lastErrorInfo);
		if (errorHistory.size() > maxErrorHistorySize) {
			errorHistory.pop_front();
		}
	}

	if (!errorMessage.empty()) {
		ofLogError("ofxGgmlStableDiffusion") << errorMessage;
	}
}

void ofxGgmlStableDiffusion::setLastErrorPreservingResult(
	ofxGgmlStableDiffusionErrorCode code,
	const std::string& errorMessage) {
	setLastErrorPreservingResult(errorMessage, code);
}

void ofxGgmlStableDiffusion::setLastResolvedVideoRequestSummary(const std::string& summary) {
	std::lock_guard<std::mutex> lock(stateMutex);
	lastResolvedVideoRequestSummary = summary;
}

void ofxGgmlStableDiffusion::setLastResolvedVideoCliCommand(const std::string& command) {
	std::lock_guard<std::mutex> lock(stateMutex);
	lastResolvedVideoCliCommand = command;
}

void ofxGgmlStableDiffusion::clearLastError() {
	std::lock_guard<std::mutex> lock(stateMutex);
	lastError.clear();
	lastErrorInfo = ofxGgmlStableDiffusionError();
	lastResult.error.clear();
}

ofPixels ofxGgmlStableDiffusion::makePixelsCopy(const sd_image_t& image) const {
	ofPixels pixels;
	if (!image.data || image.width == 0 || image.height == 0) {
		return pixels;
	}

	ofImageType type = OF_IMAGE_COLOR;
	switch (image.channel) {
	case 1: type = OF_IMAGE_GRAYSCALE; break;
	case 4: type = OF_IMAGE_COLOR_ALPHA; break;
	case 3:
	default:
		type = OF_IMAGE_COLOR;
		break;
	}

	pixels.setFromPixels(
		image.data,
		static_cast<int>(image.width),
		static_cast<int>(image.height),
		type);
	return pixels;
}

void ofxGgmlStableDiffusion::captureImageResults(
	sd_image_t* images,
	int count,
	int64_t seedValue,
	float elapsedMs,
	ofxGgmlStableDiffusionTask task,
	const ofxGgmlStableDiffusionImageRequest& request,
	const ofxSdImageRankCallback& rankCallback) {
	ofxGgmlStableDiffusionResult result;
	result.success = true;
	result.task = task;
	result.imageMode = request.mode;
	result.selectionMode = request.selectionMode;
	result.elapsedMs = elapsedMs;
	result.actualSeedUsed = seedValue;
	result.images.reserve(std::max(0, count));

	for (int i = 0; i < count; ++i) {
		ofxGgmlStableDiffusionImageFrame frame;
		frame.index = i;
		frame.sourceIndex = i;
		frame.seed = seedValue;
		frame.generation.prompt = request.prompt;
		frame.generation.negativePrompt = request.negativePrompt;
		frame.generation.cfgScale = request.cfgScale;
		frame.generation.strength = request.strength;
		frame.pixels = makePixelsCopy(images[i]);
		result.images.push_back(std::move(frame));
	}

	applyImageRanking(result.images, result, request, rankCallback);

	{
		std::lock_guard<std::mutex> lock(stateMutex);
		seedHistory.push_back(seedValue);
		if (seedHistory.size() > maxSeedHistorySize) {
			seedHistory.pop_front();
		}
		lastResult = std::move(result);
		outputImageViews = buildOutputImageViews(lastResult);
		outputImages = outputImageViews.empty() ? nullptr : outputImageViews.data();
		diffused = true;
	}

	ofxSdReleaseImageArray(images, count);
}

void ofxGgmlStableDiffusion::captureVideoResults(
	sd_image_t* images,
	int count,
	int64_t seedValue,
	const std::vector<int64_t>& frameSeeds,
	const std::vector<ofxGgmlStableDiffusionGenerationParameters>& frameGeneration,
	float elapsedMs,
	ofxGgmlStableDiffusionTask task,
	const ofxGgmlStableDiffusionVideoRequest& request) {
	ofxGgmlStableDiffusionResult result;
	result.success = true;
	result.task = task;
	result.elapsedMs = elapsedMs;
	result.actualSeedUsed = seedValue;
	result.video.fps = request.fps;
	result.video.sourceFrameCount = count;
	result.video.mode = request.mode;

	std::vector<ofxGgmlStableDiffusionImageFrame> sourceFrames;
	sourceFrames.reserve(std::max(0, count));
	for (int i = 0; i < count; ++i) {
		ofxGgmlStableDiffusionImageFrame frame;
		frame.index = i;
		frame.sourceIndex = i;
		frame.seed =
			static_cast<std::size_t>(i) < frameSeeds.size() ?
				frameSeeds[static_cast<std::size_t>(i)] :
				seedValue;
		frame.generation =
			static_cast<std::size_t>(i) < frameGeneration.size() ?
				frameGeneration[static_cast<std::size_t>(i)] :
				ofxGgmlStableDiffusionGenerationParameters{
					request.prompt,
					request.negativePrompt,
					request.cfgScale,
					request.strength
				};
		frame.pixels = makePixelsCopy(images[i]);
		sourceFrames.push_back(std::move(frame));
	}

	result.video.frames = ofxGgmlStableDiffusionBuildVideoFrames(sourceFrames, request.mode);

	{
		std::lock_guard<std::mutex> lock(stateMutex);
		seedHistory.push_back(seedValue);
		if (seedHistory.size() > maxSeedHistorySize) {
			seedHistory.pop_front();
		}
		lastResult = std::move(result);
		outputImageViews = buildOutputImageViews(lastResult);
		outputImages = outputImageViews.empty() ? nullptr : outputImageViews.data();
		diffused = true;
	}

	ofxSdReleaseImageArray(images, count);
}

void ofxGgmlStableDiffusion::applyImageRanking(
	std::vector<ofxGgmlStableDiffusionImageFrame>& frames,
	ofxGgmlStableDiffusionResult& result,
	const ofxGgmlStableDiffusionImageRequest& request,
	const ofxSdImageRankCallback& rankCallback) {
	result.rankingApplied = false;
	result.selectedImageIndex = frames.empty() ? -1 : 0;
	if (frames.empty()) {
		return;
	}

	for (auto& frame : frames) {
		frame.isSelected = false;
	}

	if (!rankCallback) {
		frames.front().isSelected = true;
		return;
	}

	std::vector<ofxGgmlStableDiffusionImageScore> scores;
	try {
		scores = rankCallback(request, frames);
	} catch (const std::exception& e) {
		ofLogWarning("ofxGgmlStableDiffusion") << "Image rank callback threw: " << e.what();
	} catch (...) {
		ofLogWarning("ofxGgmlStableDiffusion") << "Image rank callback threw an unknown exception";
	}
	if (scores.size() != frames.size()) {
		frames.front().isSelected = true;
		return;
	}

	for (std::size_t i = 0; i < frames.size(); ++i) {
		frames[i].score = scores[i];
	}

	const std::vector<std::size_t> order = ofxGgmlStableDiffusionBuildRankedImageOrder(scores);
	if (order.empty()) {
		frames.front().isSelected = true;
		return;
	}

	result.rankingApplied = true;
	const int bestSourceIndex = static_cast<int>(order.front());
	if (request.selectionMode == ofxGgmlStableDiffusionImageSelectionMode::Rerank ||
		request.selectionMode == ofxGgmlStableDiffusionImageSelectionMode::BestOnly) {
		std::vector<ofxGgmlStableDiffusionImageFrame> ranked;
		ranked.reserve(frames.size());
		for (const std::size_t index : order) {
			ranked.push_back(frames[index]);
		}
		frames = std::move(ranked);
		if (request.selectionMode == ofxGgmlStableDiffusionImageSelectionMode::BestOnly && !frames.empty()) {
			frames.resize(1);
		}
	}

	result.selectedImageIndex = -1;
	for (std::size_t i = 0; i < frames.size(); ++i) {
		frames[i].index = static_cast<int>(i);
		if (frames[i].sourceIndex == bestSourceIndex && result.selectedImageIndex < 0) {
			frames[i].isSelected = true;
			result.selectedImageIndex = static_cast<int>(i);
		}
	}

	if (result.selectedImageIndex < 0 && !frames.empty()) {
		frames.front().isSelected = true;
		result.selectedImageIndex = 0;
	}
}

std::vector<sd_image_t> ofxGgmlStableDiffusion::buildOutputImageViews(const ofxGgmlStableDiffusionResult& result) const {
	std::vector<sd_image_t> views;
	const auto appendViews = [&views](const std::vector<ofxGgmlStableDiffusionImageFrame>& frames) {
		views.reserve(frames.size());
		for (const auto& frame : frames) {
			if (!frame.isAllocated()) {
				continue;
			}
			// NOTE: const_cast required for sd_image_t compatibility; this creates read-only views
			views.push_back({
				static_cast<uint32_t>(frame.pixels.getWidth()),
				static_cast<uint32_t>(frame.pixels.getHeight()),
				static_cast<uint32_t>(frame.pixels.getNumChannels()),
				const_cast<unsigned char*>(frame.pixels.getData())
			});
		}
	};

	if (result.hasVideo()) {
		appendViews(result.video.frames);
	} else {
		appendViews(result.images);
	}

	return views;
}
