#include "ofxGgmlStableDiffusionThread.h"
#include "ofxGgmlStableDiffusion.h"
#include "core/ofxGgmlStableDiffusionNativeAdapter.h"
#include "core/ofxGgmlStableDiffusionMemoryHelpers.h"
#include "video/ofxGgmlStableDiffusionVideoHelpers.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <mutex>
#include <random>
#include <ctime>
#include <vector>
#include <sstream>

namespace {

namespace fs = std::filesystem;

std::mutex& generationCallbackMutex() {
	static std::mutex mutex;
	return mutex;
}

class ProgressCallbackGuard {
public:
	ProgressCallbackGuard(std::mutex& mutex, sd_progress_cb_t cb, void* data)
		: lock(mutex) {
		sd_set_progress_callback(cb, data);
	}

	~ProgressCallbackGuard() {
		sd_set_progress_callback(nullptr, nullptr);
	}

	ProgressCallbackGuard(const ProgressCallbackGuard&) = delete;
	ProgressCallbackGuard& operator=(const ProgressCallbackGuard&) = delete;

private:
	std::unique_lock<std::mutex> lock;
};

void threadProgressCallback(int step, int steps, float time, void* data) {
	auto* thread = static_cast<ofxGgmlStableDiffusionThread*>(data);
	if (thread && thread->task == ofxGgmlStableDiffusionTask::ImageToVideo) {
		if (thread->videoTaskData.progressCallback) {
			try {
				if (thread->videoTaskData.animationProgressEnabled) {
					const int stepsPerFrame = std::max(1, steps);
					const int totalSteps = std::max(1, thread->videoTaskData.animationFrameCount) * stepsPerFrame;
					const int compositeStep =
						(thread->videoTaskData.animationFrameIndex * stepsPerFrame) +
						std::max(0, std::min(step, stepsPerFrame));
					thread->videoTaskData.progressCallback(compositeStep, totalSteps, time);
				} else {
					thread->videoTaskData.progressCallback(step, steps, time);
				}
			} catch (const std::exception& e) {
				ofLogWarning("ofxGgmlStableDiffusion") << "Progress callback threw: " << e.what();
			} catch (...) {
				ofLogWarning("ofxGgmlStableDiffusion") << "Progress callback threw an unknown exception";
			}
		}
		return;
	}

	if (thread && thread->imageTaskData.progressCallback) {
		try {
			thread->imageTaskData.progressCallback(step, steps, time);
		} catch (const std::exception& e) {
			ofLogWarning("ofxGgmlStableDiffusion") << "Progress callback threw: " << e.what();
		} catch (...) {
			ofLogWarning("ofxGgmlStableDiffusion") << "Progress callback threw an unknown exception";
		}
	}
}

bool areBlendable(const sd_image_t& left, const sd_image_t& right) {
	return left.data != nullptr &&
		right.data != nullptr &&
		left.width == right.width &&
		left.height == right.height &&
		left.channel == right.channel &&
		left.channel > 0;
}

void assignBlendedImage(
	const sd_image_t& startImage,
	const sd_image_t& endImage,
	float t,
	ofxGgmlStableDiffusionThread::OwnedImage& output) {
	output.clear();
	if (!areBlendable(startImage, endImage)) {
		return;
	}

	const std::size_t byteCount =
		static_cast<std::size_t>(startImage.width) *
		static_cast<std::size_t>(startImage.height) *
		static_cast<std::size_t>(startImage.channel);
	output.storage.resize(byteCount);
	for (std::size_t i = 0; i < byteCount; ++i) {
		const float startValue = static_cast<float>(startImage.data[i]);
		const float endValue = static_cast<float>(endImage.data[i]);
		output.storage[i] = static_cast<uint8_t>(std::round(startValue + ((endValue - startValue) * t)));
	}
	output.image = {startImage.width, startImage.height, startImage.channel, output.storage.data()};
}

const sd_image_t& resolveAnimatedBaseImage(
	const ofxGgmlStableDiffusionThread::VideoTaskData& taskData,
	int frameIndex,
	ofxGgmlStableDiffusionThread::OwnedImage& blendedImage) {
	if (!taskData.endImage.isAllocated() || taskData.request.frameCount <= 1) {
		return taskData.initImage.image;
	}

	const float t = static_cast<float>(frameIndex) /
		static_cast<float>(std::max(1, taskData.request.frameCount - 1));
	if (areBlendable(taskData.initImage.image, taskData.endImage.image)) {
		assignBlendedImage(taskData.initImage.image, taskData.endImage.image, t, blendedImage);
		if (blendedImage.isAllocated()) {
			return blendedImage.image;
		}
	}

	return frameIndex >= (taskData.request.frameCount - 1) ?
		taskData.endImage.image :
		taskData.initImage.image;
}

std::vector<int64_t> buildAnimatedVideoSeeds(const ofxGgmlStableDiffusionThread::VideoTaskData& taskData) {
	std::vector<int64_t> seeds;
	seeds.reserve(std::max(0, taskData.request.frameCount));
	for (int frameIndex = 0; frameIndex < taskData.request.frameCount; ++frameIndex) {
		seeds.push_back(ofxGgmlStableDiffusionGetFrameSeed(taskData.request, frameIndex));
	}
	return seeds;
}

sd_image_t* makeOwnedImageArray(const std::vector<ofxGgmlStableDiffusionThread::OwnedImage>& frames) {
	if (frames.empty()) {
		return nullptr;
	}

	auto* output = static_cast<sd_image_t*>(std::malloc(sizeof(sd_image_t) * frames.size()));
	if (!output) {
		return nullptr;
	}

	for (std::size_t i = 0; i < frames.size(); ++i) {
		output[i] = {0, 0, 0, nullptr};
		const auto& frame = frames[i];
		if (!frame.isAllocated()) {
			continue;
		}

		const std::size_t byteCount =
			static_cast<std::size_t>(frame.image.width) *
			static_cast<std::size_t>(frame.image.height) *
			static_cast<std::size_t>(frame.image.channel);
		auto* pixels = static_cast<uint8_t*>(std::malloc(byteCount));
		if (!pixels) {
			ofxSdReleaseImageArray(output, static_cast<int>(i));
			return nullptr;
		}
		std::memcpy(pixels, frame.image.data, byteCount);
		output[i] = {frame.image.width, frame.image.height, frame.image.channel, pixels};
	}

	return output;
}

int64_t resolveNativeGenerationSeed(int64_t seed) {
	if (seed >= 0) {
		return seed;
	}

	static std::atomic<uint64_t> autoSeedCounter{0};
	uint64_t entropy = static_cast<uint64_t>(
		std::chrono::high_resolution_clock::now().time_since_epoch().count());
	entropy ^= autoSeedCounter.fetch_add(1, std::memory_order_relaxed) +
		0x9e3779b97f4a7c15ull;
	try {
		std::random_device rd;
		entropy ^= static_cast<uint64_t>(rd()) << 32;
		entropy ^= static_cast<uint64_t>(rd());
	} catch (...) {
	}

	std::mt19937_64 rng(entropy);
	std::uniform_int_distribution<int64_t> dist(
		0,
		(std::numeric_limits<int64_t>::max)());
	return dist(rng);
}

} // namespace

ofxGgmlStableDiffusionThread::~ofxGgmlStableDiffusionThread() {
	if (isThreadRunning()) {
		waitForThread(true);
	}
	{
		std::lock_guard<std::mutex> callbackLock(generationCallbackMutex());
		sd_set_progress_callback(nullptr, nullptr);
	}
	clearContexts();
}

void ofxGgmlStableDiffusionThread::clearContexts() {
	isSdCtxLoaded.store(false, std::memory_order_release);
	if (sdCtx) {
		free_sd_ctx(sdCtx);
		sdCtx = nullptr;
	}
	if (upscalerCtx) {
		free_upscaler_ctx(upscalerCtx);
		upscalerCtx = nullptr;
	}
	isUpscalerCtxLoaded = false;
	generationContextNeedsRefresh = false;
	lastContextFingerprint.clear();
	generationsSinceRebuild = 0;
}

bool ofxGgmlStableDiffusionThread::hasLoadedContext() const {
	return isSdCtxLoaded.load(std::memory_order_acquire);
}

std::string ofxGgmlStableDiffusionThread::computeContextFingerprint(const ofxGgmlStableDiffusionContextSettings& settings) {
	// Create a fingerprint from settings that affect the native context.
	// Only include settings that require context rebuild when changed.
	// Use '\0' as the field separator – it cannot appear in file paths, preventing
	// false collisions that could occur with printable separators like '|'.
	std::string fp;
	fp.reserve(512);
	const auto sep = [&fp]() { fp += '\0'; };
	fp += settings.modelPath;          sep();
	fp += settings.diffusionModelPath; sep();
	fp += settings.clipLPath;          sep();
	fp += settings.clipGPath;          sep();
	fp += settings.t5xxlPath;          sep();
	fp += settings.vaePath;            sep();
	fp += settings.taesdPath;          sep();
	fp += settings.controlNetPath;     sep();
	fp += settings.loraModelDir;       sep();
	fp += settings.embedDir;           sep();
	fp += settings.stackedIdEmbedDir;  sep();
	fp += settings.backend;            sep();
	fp += settings.paramsBackend;      sep();
	fp += static_cast<char>(settings.vaeDecodeOnly);
	fp += static_cast<char>(settings.vaeTiling);
	fp += static_cast<char>(settings.freeParamsImmediately);
	fp += static_cast<char>(settings.nThreads & 0xFF);
	fp += static_cast<char>((settings.nThreads >> 8) & 0xFF);
	fp += static_cast<char>((settings.nThreads >> 16) & 0xFF);
	fp += static_cast<char>((settings.nThreads >> 24) & 0xFF);
	fp += static_cast<char>(settings.weightType);
	fp += static_cast<char>(settings.rngType);
	fp += static_cast<char>(settings.schedule);
	fp += static_cast<char>(settings.prediction);
	fp += static_cast<char>(settings.loraApplyMode);
	fp += static_cast<char>(settings.keepClipOnCpu);
	fp += static_cast<char>(settings.keepControlNetCpu);
	fp += static_cast<char>(settings.keepVaeOnCpu);
	fp += static_cast<char>(settings.offloadParamsToCpu);
	fp += static_cast<char>(settings.flashAttn);
	fp += static_cast<char>(settings.diffusionFlashAttn);
	fp += static_cast<char>(settings.enableMmap);
	return fp;
}

void ofxGgmlStableDiffusionThread::prepareContextTask(const ContextTaskData& data) {
	task = ofxGgmlStableDiffusionTask::LoadModel;
	contextTaskData = data;
}

void ofxGgmlStableDiffusionThread::prepareImageTask(const ImageTaskData& data) {
	task = data.task;
	imageTaskData = data;
	imageTaskData.syncViews();
}

void ofxGgmlStableDiffusionThread::prepareVideoTask(const VideoTaskData& data) {
	task = data.task;
	videoTaskData = data;
	videoTaskData.syncViews();
}

void ofxGgmlStableDiffusionThread::threadedFunction() {
	ofxGgmlStableDiffusion* sd = static_cast<ofxGgmlStableDiffusion*>(userData);
	if (!sd) {
		return;
	}

	const auto finishTask =
		[this, &sd](bool cancelled = false, const std::string& cancelMessage = std::string()) {
			sd->finishBackgroundTask(cancelled, cancelMessage);
			task = ofxGgmlStableDiffusionTask::None;
		};

	const auto cancelRequested =
		[this, &finishTask](const std::string& message) {
			if (!isCancellationRequested()) {
				return false;
			}
			finishTask(true, message);
			return true;
		};

	if (task == ofxGgmlStableDiffusionTask::LoadModel || sd->isModelLoading.load(std::memory_order_acquire)) {
		if (cancelRequested("Model loading cancelled before the native context was created")) {
			return;
		}

		if (sdCtx) {
			isSdCtxLoaded.store(false, std::memory_order_release);
			free_sd_ctx(sdCtx);
			sdCtx = nullptr;
		}

		std::vector<std::string> embeddingNames;
		std::vector<std::string> embeddingPaths;
		std::vector<sd_embedding_t> embeddings;
		const auto& contextSettings = contextTaskData.contextSettings;
		const auto describeMissingPaths = [&contextSettings]() {
			std::vector<std::string> missing;
			const auto addIfMissing = [&missing](const std::string& path, const char* label) {
				if (path.empty()) {
					return;
				}
				const fs::path p(path);
				if (!fs::exists(p)) {
					missing.push_back(std::string(label) + ": " + p.string());
				}
			};
			addIfMissing(contextSettings.modelPath, "model");
			addIfMissing(contextSettings.diffusionModelPath, "diffusion");
			addIfMissing(contextSettings.clipLPath, "clip_l");
			addIfMissing(contextSettings.clipGPath, "clip_g");
			addIfMissing(contextSettings.t5xxlPath, "text_encoder");
			addIfMissing(contextSettings.vaePath, "vae");
			addIfMissing(contextSettings.controlNetPath, "controlnet");
			return missing;
		};
		bool contextErrorReported = false;
		sd_ctx_params_t ctxParams =
			ofxGgmlStableDiffusionNativeAdapter::buildContextParams(
				contextTaskData,
				embeddingNames,
				embeddingPaths,
				embeddings);
		sdCtx = new_sd_ctx(&ctxParams);
		if (isCancellationRequested()) {
			if (sdCtx) {
				free_sd_ctx(sdCtx);
				sdCtx = nullptr;
			}
			isSdCtxLoaded.store(false, std::memory_order_release);
			finishTask(true, "Model loading cancelled");
			return;
		}
		if (!sdCtx) {
			const auto missing = describeMissingPaths();
			if (!missing.empty()) {
				std::string message = "Missing model files: ";
				for (std::size_t i = 0; i < missing.size(); ++i) {
					message += missing[i];
					if (i + 1 < missing.size()) {
						message += "; ";
					}
				}
				sd->setLastError(ofxGgmlStableDiffusionErrorCode::ModelNotFound, message);
				contextErrorReported = true;
			} else {
				const std::string primary =
					!contextTaskData.contextSettings.modelPath.empty() ?
						contextTaskData.contextSettings.modelPath :
						contextTaskData.contextSettings.diffusionModelPath;
				sd->setLastError(
					ofxGgmlStableDiffusionErrorCode::ModelLoadFailed,
					primary.empty() ?
						"Failed to create stable-diffusion context" :
						"Failed to create stable-diffusion context for " + primary);
				contextErrorReported = true;
			}
		}

		if (upscalerCtx) {
			free_upscaler_ctx(upscalerCtx);
			upscalerCtx = nullptr;
			isUpscalerCtxLoaded = false;
		}
		if (contextTaskData.upscalerSettings.enabled && !contextTaskData.upscalerSettings.modelPath.empty()) {
			upscalerCtx = new_upscaler_ctx(
				contextTaskData.upscalerSettings.modelPath.c_str(),
				false,
				contextTaskData.upscalerSettings.nThreads,
				0,
				nullptr,
				nullptr);
			isUpscalerCtxLoaded = (upscalerCtx != nullptr);
		}
		isSdCtxLoaded.store(sdCtx != nullptr, std::memory_order_release);
		generationContextNeedsRefresh = false;
		lastContextFingerprint = isSdCtxLoaded.load(std::memory_order_acquire) ?
			computeContextFingerprint(contextTaskData.contextSettings) :
			std::string();
		generationsSinceRebuild = 0;
		{
			std::lock_guard<std::mutex> lock(sd->stateMutex);
			sd->refreshResolvedDefaultCachesNoLock(sdCtx);
		}
		if (contextTaskData.upscalerSettings.enabled &&
			!contextTaskData.upscalerSettings.modelPath.empty() &&
			!isUpscalerCtxLoaded) {
			{
				std::lock_guard<std::mutex> lock(sd->stateMutex);
				sd->isESRGAN = false;
				sd->esrganPath.clear();
			}
			contextTaskData.upscalerSettings.enabled = false;
			contextTaskData.upscalerSettings.modelPath.clear();
			sd->setLastError(ofxGgmlStableDiffusionErrorCode::UpscaleFailed, "Failed to create upscaler context");
		}
		if (!isSdCtxLoaded.load(std::memory_order_acquire) && !contextErrorReported) {
			sd->setLastError("Failed to create stable-diffusion context");
		}
		finishTask();
		return;
	}

	const auto rebuildSdContextForGeneration =
		[this, &sd, &finishTask](const ofxGgmlStableDiffusionContextSettings& currentContextSettings) -> bool {
			if (sdCtx) {
				isSdCtxLoaded.store(false, std::memory_order_release);
				free_sd_ctx(sdCtx);
				sdCtx = nullptr;
			}

			ofxGgmlStableDiffusionThread::ContextTaskData reloadTask;
			reloadTask.contextSettings = currentContextSettings;
			std::vector<std::string> embeddingNames;
			std::vector<std::string> embeddingPaths;
			std::vector<sd_embedding_t> embeddings;
			sd_ctx_params_t ctxParams =
				ofxGgmlStableDiffusionNativeAdapter::buildContextParams(
					reloadTask,
					embeddingNames,
					embeddingPaths,
					embeddings);
			sdCtx = new_sd_ctx(&ctxParams);
			isSdCtxLoaded.store(sdCtx != nullptr, std::memory_order_release);
			{
				std::lock_guard<std::mutex> lock(sd->stateMutex);
				sd->refreshResolvedDefaultCachesNoLock(sdCtx);
			}
			if (!isSdCtxLoaded.load(std::memory_order_acquire)) {
				sd->setLastError("Failed to recreate stable-diffusion context for generation");
				finishTask();
				return false;
			}
			return true;
		};

	if (cancelRequested("Generation cancelled before the request started")) {
		return;
	}

	if (!sdCtx) {
		sd->setLastError("Stable Diffusion context is not loaded");
		finishTask();
		return;
	}

	const bool isVideoTask = (task == ofxGgmlStableDiffusionTask::ImageToVideo);
	const ofxGgmlStableDiffusionContextSettings& generationContextSettings =
		isVideoTask ? videoTaskData.contextSettings : imageTaskData.contextSettings;

	// Smart context reuse: only rebuild when necessary
	std::string currentFingerprint = computeContextFingerprint(generationContextSettings);
	bool needsRebuild = generationContextNeedsRefresh
		|| (currentFingerprint != lastContextFingerprint)
		|| (generationContextSettings.freeParamsImmediately &&
			generationsSinceRebuild >= MAX_REUSE_COUNT)
		|| !sdCtx;

	if (needsRebuild) {
		if (!rebuildSdContextForGeneration(generationContextSettings)) {
			return;
		}
		lastContextFingerprint = currentFingerprint;
		generationsSinceRebuild = 0;
		generationContextNeedsRefresh = false;
	} else {
		generationsSinceRebuild++;
	}

	const bool upscalerAvailable = isUpscalerCtxLoaded && upscalerCtx;

	if (task == ofxGgmlStableDiffusionTask::ImageToVideo) {
		if (videoTaskData.upscalerSettings.enabled && !upscalerAvailable) {
			videoTaskData.upscalerSettings.enabled = false;
			sd->setLastError(
				ofxGgmlStableDiffusionErrorCode::UpscaleFailed,
				"Upscaler context is not available for video generation");
			finishTask();
			return;
		}

		if (videoTaskData.request.hasAnimation()) {
			const int frameCount = std::max(0, videoTaskData.request.frameCount);
			std::vector<OwnedImage> generatedFrames;
			generatedFrames.reserve(static_cast<std::size_t>(frameCount));
			std::vector<int64_t> frameSeeds = buildAnimatedVideoSeeds(videoTaskData);
			std::vector<ofxGgmlStableDiffusionGenerationParameters> frameGeneration;
			frameGeneration.reserve(static_cast<std::size_t>(frameCount));
			std::string generationError;

			videoTaskData.animationProgressEnabled = (videoTaskData.progressCallback != nullptr);
			videoTaskData.animationFrameIndex = 0;
			videoTaskData.animationFrameCount = frameCount;
			videoTaskData.animationSampleSteps = videoTaskData.request.sampleSteps;

			for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
					if (isCancellationRequested()) {
						generationError = "Animated video generation cancelled";
						break;
					}

					videoTaskData.animationFrameIndex = frameIndex;

					OwnedImage blendedInitImage;
					const sd_image_t& frameInitImage =
						resolveAnimatedBaseImage(videoTaskData, frameIndex, blendedInitImage);
					const bool hasFrameInitImage = (frameInitImage.data != nullptr);

					ImageTaskData frameTask;
					frameTask.task =
						hasFrameInitImage ?
							ofxGgmlStableDiffusionTask::ImageToImage :
							ofxGgmlStableDiffusionTask::TextToImage;
					frameTask.contextSettings = videoTaskData.contextSettings;
					frameTask.upscalerSettings = videoTaskData.upscalerSettings;
					frameTask.request.mode =
						hasFrameInitImage ?
							ofxGgmlStableDiffusionImageMode::ImageToImage :
							ofxGgmlStableDiffusionImageMode::TextToImage;
					frameTask.request.initImage =
						hasFrameInitImage ? frameInitImage : sd_image_t{0, 0, 0, nullptr};
					frameTask.request.prompt =
						ofxGgmlStableDiffusionGetFramePrompt(videoTaskData.request, frameIndex);
					frameTask.request.negativePrompt =
						ofxGgmlStableDiffusionGetFrameNegativePrompt(videoTaskData.request, frameIndex);
					frameTask.request.clipSkip = videoTaskData.request.clipSkip;
					frameTask.request.cfgScale =
						ofxGgmlStableDiffusionGetFrameCfgScale(videoTaskData.request, frameIndex);
					frameTask.request.width = videoTaskData.request.width;
					frameTask.request.height = videoTaskData.request.height;
					frameTask.request.sampleMethod = videoTaskData.request.sampleMethod;
					frameTask.request.sampleSteps = videoTaskData.request.sampleSteps;
					frameTask.request.strength =
						ofxGgmlStableDiffusionGetFrameStrength(videoTaskData.request, frameIndex);
					frameTask.request.seed =
						static_cast<std::size_t>(frameIndex) < frameSeeds.size() ?
							frameSeeds[static_cast<std::size_t>(frameIndex)] :
							videoTaskData.request.seed;
					frameTask.request.batchCount = 1;
					frameTask.request.loras = videoTaskData.request.loras;
					frameGeneration.push_back({
						frameTask.request.prompt,
						frameTask.request.negativePrompt,
						frameTask.request.cfgScale,
						frameTask.request.strength
					});
					if (hasFrameInitImage) {
						frameTask.initImage.assign(frameInitImage);
					}
					frameTask.syncViews();

					const std::string effectivePrompt =
						ofxGgmlStableDiffusionNativeAdapter::buildEffectivePrompt(frameTask.request);
					std::vector<ofPixels> pmPixels;
					std::vector<sd_image_t> pmImageViews;
					sd_img_gen_params_t frameParams = ofxGgmlStableDiffusionNativeAdapter::buildImageParams(
						frameTask,
						sdCtx,
						effectivePrompt,
						loraBuffer,
						pmPixels,
						pmImageViews);

					sd_image_t* frameOutput = nullptr;
					{
						ProgressCallbackGuard progressGuard(
							generationCallbackMutex(),
							videoTaskData.progressCallback ? threadProgressCallback : nullptr,
							videoTaskData.progressCallback ? this : nullptr);
						int frameOutputCount = 0;
						if (!generate_image(sdCtx, &frameParams, &frameOutput, &frameOutputCount) || frameOutputCount < 1) {
							frameOutput = nullptr;
						}
					}
					if (!frameOutput || !frameOutput[0].data) {
						ofxSdReleaseImageArray(frameOutput, 1);
						generationError = "Animated video generation returned no frame output for frame " +
							std::to_string(frameIndex);
						ofLogError("ofxGgmlStableDiffusion")
							<< "Frame " << frameIndex << " generation failed: " << generationError;
						break;
					}

					if (videoTaskData.upscalerSettings.enabled) {
						if (!upscalerCtx) {
							ofxSdReleaseImageArray(frameOutput, 1);
							sd->setLastError(
								ofxGgmlStableDiffusionErrorCode::UpscaleFailed,
								"Upscaler context is not loaded");
							generationError.clear();
							break;
						}

						sd_image_t* upscaledOutput = nullptr;
						int upscaledCount = 0;
						if (!upscale(upscalerCtx, frameOutput[0], videoTaskData.upscalerSettings.multiplier,
								&upscaledOutput, &upscaledCount) || !upscaledOutput || upscaledCount < 1) {
							ofxSdReleaseImageArray(frameOutput, 1);
							sd->setLastError(
								ofxGgmlStableDiffusionErrorCode::UpscaleFailed,
								"Upscaling failed for one or more video frames");
							generationError.clear();
							break;
						}

						ofxSdReleaseImage(frameOutput[0]);
						frameOutput[0] = upscaledOutput[0];
						free(upscaledOutput);
					}

					if (isCancellationRequested()) {
						ofxSdReleaseImageArray(frameOutput, 1);
						generationError = "Animated video generation cancelled";
						break;
					}

					OwnedImage generatedFrame;
					if (!generatedFrame.assign(frameOutput[0])) {
						ofxSdReleaseImageArray(frameOutput, 1);
						generationError = "Animated video generation produced an invalid frame at index " +
							std::to_string(frameIndex) + " (width=" +
							std::to_string(frameOutput[0].width) + ", height=" +
							std::to_string(frameOutput[0].height) + ", channels=" +
							std::to_string(frameOutput[0].channel) + ")";
						ofLogError("ofxGgmlStableDiffusion")
							<< "Frame " << frameIndex << " assignment failed: invalid image data";
						break;
					}

					ofxSdReleaseImageArray(frameOutput, 1);
					generatedFrames.push_back(std::move(generatedFrame));
				}

			videoTaskData.animationProgressEnabled = false;
			videoTaskData.animationFrameIndex = 0;
			videoTaskData.animationFrameCount = 0;
			videoTaskData.animationSampleSteps = 0;

			if (isCancellationRequested()) {
				finishTask(true, generationError.empty() ? "Animated video generation cancelled" : generationError);
				return;
			}

			if (generatedFrames.size() != static_cast<std::size_t>(frameCount)) {
				if (!generationError.empty()) {
					sd->setLastError(generationError);
				}
				finishTask();
				return;
			}

			sd_image_t* output = makeOwnedImageArray(generatedFrames);
			const float elapsedMs =
				static_cast<float>(ofGetElapsedTimeMicros() - sd->taskStartMicros) / 1000.0f;
			if (!output) {
				sd->setLastError("Animated video generation could not allocate output frames");
				finishTask();
				return;
			}

			const int64_t actualSeedUsed = frameSeeds.empty() ? videoTaskData.request.seed : frameSeeds.front();
			sd->captureVideoResults(
				output,
				static_cast<int>(generatedFrames.size()),
				actualSeedUsed,
				frameSeeds,
				frameGeneration,
				elapsedMs,
				task,
				videoTaskData.request);
			generationContextNeedsRefresh =
				generationContextSettings.freeParamsImmediately;
			finishTask();
			return;
		}

		sd_vid_gen_params_t params =
			ofxGgmlStableDiffusionNativeAdapter::buildVideoParams(videoTaskData, sdCtx, loraBuffer);
		params.seed = resolveNativeGenerationSeed(params.seed);
		const std::string resolvedVideoSummary =
			ofxGgmlStableDiffusionNativeAdapter::describeVideoParams(params);
		const std::string resolvedVideoCliCommand =
			ofxGgmlStableDiffusionNativeAdapter::buildResolvedVideoCliCommand(
				params,
				videoTaskData.contextSettings);
		sd->setLastResolvedVideoRequestSummary(resolvedVideoSummary);
		sd->setLastResolvedVideoCliCommand(resolvedVideoCliCommand);
		ofLogNotice("ofxGgmlStableDiffusion")
			<< "Wrapper video request: "
			<< resolvedVideoSummary;
		int generatedFrameCount = 0;
		sd_image_t* output = nullptr;
		sd_audio_t* audio = nullptr;
		{
			ProgressCallbackGuard progressGuard(
				generationCallbackMutex(),
				videoTaskData.progressCallback ? threadProgressCallback : nullptr,
				videoTaskData.progressCallback ? this : nullptr);
			const bool ok = generate_video(sdCtx, &params, &output, &generatedFrameCount, &audio);
			if (!ok) {
				output = nullptr;
				generatedFrameCount = 0;
			}
		}
		if (audio) {
			free_sd_audio(audio);
		}
		const float elapsedMs = static_cast<float>(ofGetElapsedTimeMicros() - sd->taskStartMicros) / 1000.0f;
		if (!output || generatedFrameCount <= 0) {
			ofxSdReleaseImageArray(output, generatedFrameCount);
			sd->setLastError("Image-to-video generation returned no frames");
			finishTask();
			return;
		}
		if (isCancellationRequested()) {
			ofxSdReleaseImageArray(output, generatedFrameCount);
			finishTask(true, "Video generation cancelled");
			return;
		}
		sd->captureVideoResults(
			output,
			generatedFrameCount,
			params.seed,
			{},
			{},
			elapsedMs,
			task,
			videoTaskData.request);
		generationContextNeedsRefresh =
			generationContextSettings.freeParamsImmediately;
		finishTask();
		return;
	}

	const std::string effectivePrompt =
		ofxGgmlStableDiffusionNativeAdapter::buildEffectivePrompt(imageTaskData.request);
	std::vector<ofPixels> pmPixels;
	std::vector<sd_image_t> pmImageViews;

	if (imageTaskData.upscalerSettings.enabled && !upscalerAvailable) {
		imageTaskData.upscalerSettings.enabled = false;
		sd->setLastError(
			ofxGgmlStableDiffusionErrorCode::UpscaleFailed,
			"Upscaler context is not available for image generation");
		finishTask();
		return;
	}

	sd_img_gen_params_t params =
		ofxGgmlStableDiffusionNativeAdapter::buildImageParams(
			imageTaskData,
			sdCtx,
			effectivePrompt,
			loraBuffer,
			pmPixels,
			pmImageViews);
	params.seed = resolveNativeGenerationSeed(params.seed);
	sd_image_t* output = nullptr;
	{
		ProgressCallbackGuard progressGuard(
			generationCallbackMutex(),
			imageTaskData.progressCallback ? threadProgressCallback : nullptr,
			imageTaskData.progressCallback ? this : nullptr);
		int outputCount = 0;
		if (!generate_image(sdCtx, &params, &output, &outputCount) || outputCount < 1) {
			output = nullptr;
		}
	}

	if (output && imageTaskData.upscalerSettings.enabled) {
		if (!upscalerCtx) {
			ofxSdReleaseImageArray(output, imageTaskData.request.batchCount);
			sd->setLastError(ofxGgmlStableDiffusionErrorCode::UpscaleFailed, "Upscaler context is not loaded");
			finishTask();
			return;
		}

		for (int i = 0; i < imageTaskData.request.batchCount; i++) {
			sd_image_t* upscaledOutput = nullptr;
			int upscaledCount = 0;
			if (!upscale(upscalerCtx, output[i], imageTaskData.upscalerSettings.multiplier,
					&upscaledOutput, &upscaledCount) || !upscaledOutput || upscaledCount < 1) {
				ofxSdReleaseImageArray(output, imageTaskData.request.batchCount);
				sd->setLastError(ofxGgmlStableDiffusionErrorCode::UpscaleFailed, "Upscaling failed for one or more images");
				finishTask();
				return;
			}
			ofxSdReleaseImage(output[i]);
			output[i] = upscaledOutput[0];
			free(upscaledOutput);
		}
	}

	const float elapsedMs = static_cast<float>(ofGetElapsedTimeMicros() - sd->taskStartMicros) / 1000.0f;
	if (!output) {
		sd->setLastError("Image generation returned no images");
		finishTask();
		return;
	}

	if (isCancellationRequested()) {
		ofxSdReleaseImageArray(output, imageTaskData.request.batchCount);
		finishTask(true, "Image generation cancelled");
		return;
	}

	sd->captureImageResults(
		output,
		imageTaskData.request.batchCount,
		params.seed,
		elapsedMs,
		task,
		imageTaskData.request,
		imageTaskData.imageRankCallback);
	generationContextNeedsRefresh =
		generationContextSettings.freeParamsImmediately;
	finishTask();
}

void ofxGgmlStableDiffusionThread::requestCancellation() {
	cancellationRequested.store(true);
}

bool ofxGgmlStableDiffusionThread::isCancellationRequested() const {
	return cancellationRequested.load();
}

void ofxGgmlStableDiffusionThread::resetCancellation() {
	cancellationRequested.store(false);
}
