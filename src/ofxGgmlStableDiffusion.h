#pragma once

#include "ofMain.h"
#include "bridges/ofxGgmlStableDiffusionHoloscanBridge.h"
#include "core/ofxGgmlStableDiffusionBatchProcessor.h"
#include "core/ofxGgmlStableDiffusionCreativeWorkflow.h"
#include "core/ofxGgmlStableDiffusionLimits.h"
#include "core/ofxGgmlStableDiffusionModelManager.h"
#include "core/ofxGgmlStableDiffusionParameterTuningHelpers.h"
#include "core/ofxGgmlStableDiffusionPerformanceProfiler.h"
#include "core/ofxGgmlStableDiffusionQueue.h"
#include "core/ofxGgmlStableDiffusionRealtimeSession.h"
#include "core/ofxGgmlStableDiffusionRealtimeVideoSession.h"
#include "core/ofxGgmlStableDiffusionSamplingHelpers.h"
#include "core/ofxGgmlStableDiffusionTypes.h"
#include "core/ofxGgmlStableDiffusionVersion.h"
#include "video/ofxGgmlStableDiffusionLongVideoManifest.h"
#include "video/ofxGgmlStableDiffusionLongVideoWorkflow.h"
#include "video/ofxGgmlStableDiffusionVideoWorkflowHelpers.h"
#include "ofxGgmlStableDiffusionThread.h"
#include "core/ofxGgmlStableDiffusionNativeApi.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

/// Callback type for generation progress reporting.
/// @param step Current step number.
/// @param steps Total number of steps.
/// @param time Time elapsed in seconds.
using ofxSdProgressCallback = std::function<void(int step, int steps, float time)>;
using ofxSdImageRankCallback = std::function<std::vector<ofxGgmlStableDiffusionImageScore>(
	const ofxGgmlStableDiffusionImageRequest& request,
	const std::vector<ofxGgmlStableDiffusionImageFrame>& images)>;

/// @brief Main interface for Stable Diffusion image and video generation.
///
/// This class wraps stable-diffusion.cpp and provides typed request/result objects,
/// internal background execution, progress callbacks, and error handling.
///
/// @threadsafety The public stateful API is internally synchronized unless a method
/// explicitly says otherwise. Generation, context reload, and upscaling entry points may
/// be called from any thread, but only one long-running task can run at a time;
/// concurrent starts fail with ThreadBusy. Progress and image-ranking callbacks are
/// invoked from the worker thread, so UI work should be deferred to the caller's
/// update()/draw() thread.
class ofxGgmlStableDiffusion {
public:
	ofxGgmlStableDiffusion();
	virtual ~ofxGgmlStableDiffusion();

	/// @brief Configure the Stable Diffusion context (model, VAE, settings).
	/// @threadsafe Yes. Will fail if generation is in progress.
	/// @note Starts an asynchronous background model-load task.
	void configureContext(const ofxGgmlStableDiffusionContextSettings& settings);

	/// @brief Generate one or more images from a text/image prompt.
	/// @threadsafe Yes, but only one generation at a time. Returns immediately; results
	/// available via callbacks or getLastResult() after completion.
	/// @note Requires a previously loaded context. Check hasLoadedContext() or getLastError() if failed.
	void generate(const ofxGgmlStableDiffusionImageRequest& request);

	/// @brief Generate a video from a prompt.
	/// @threadsafe Yes, but only one generation at a time. Returns immediately; results
	/// available via callbacks or getLastResult() after completion.
	/// @note Video generation supports animation via keyframes (prompt interpolation,
	/// parameter animation, seed sequences). Use ofxGgmlStableDiffusionVideoAnimationSettings
	/// to configure animation. For animated videos, progress callbacks report overall
	/// progress across all frames.
	void generateVideo(const ofxGgmlStableDiffusionVideoRequest& request);

	/// @brief Configure upscaler settings (ESRGAN).
	/// @threadsafe Yes. Will fail if generation is in progress.
	void setUpscalerSettings(const ofxGgmlStableDiffusionUpscalerSettings& settings);

	/// @brief Get current context settings.
	/// @threadsafe Yes (returns copy under lock).
	ofxGgmlStableDiffusionContextSettings getContextSettings() const;

	/// @brief Get current upscaler settings.
	/// @threadsafe Yes (returns copy under lock).
	ofxGgmlStableDiffusionUpscalerSettings getUpscalerSettings() const;

	/// @brief Get capabilities of currently loaded model.
	/// @threadsafe Yes (returns copy under lock).
	ofxGgmlStableDiffusionCapabilities getCapabilities() const;

	/// @brief Get complete result from last generation.
	/// @threadsafe Yes (returns copy under lock).
	ofxGgmlStableDiffusionResult getLastResult() const;

	/// @brief Get generated images from last result.
	/// @threadsafe Yes (returns copy under lock).
	std::vector<ofxGgmlStableDiffusionImageFrame> getImages() const;

	/// @brief Get generated video clip from last result.
	/// @threadsafe Yes (returns copy under lock).
	/// @note For animated videos, frames contain per-frame generation parameters.
	ofxGgmlStableDiffusionVideoClip getVideoClip() const;

	/// @brief Check if last result contains images.
	/// @threadsafe Yes.
	bool hasImageResult() const;

	/// @brief Check if last result contains video.
	/// @threadsafe Yes.
	bool hasVideoResult() const;

	/// @brief Check if a model context is loaded.
	/// @threadsafe Yes.
	bool hasLoadedContext() const;

	/// @brief Get number of output images/frames in last result.
	/// @threadsafe Yes.
	int getOutputCount() const;

	/// @brief Get last error message (empty if no error).
	/// @threadsafe Yes (returns copy under lock).
	std::string getLastError() const;

	/// @brief Get last error code.
	/// @threadsafe Yes.
	ofxGgmlStableDiffusionErrorCode getLastErrorCode() const;

	/// @brief Get detailed error information.
	/// @threadsafe Yes (returns copy under lock).
	ofxGgmlStableDiffusionError getLastErrorInfo() const;

	/// @brief Get summary of last video request resolution.
	/// @threadsafe Yes (returns copy under lock).
	std::string getLastResolvedVideoRequestSummary() const;

	/// @brief Get CLI command for last video request.
	/// @threadsafe Yes (returns copy under lock).
	std::string getLastResolvedVideoCliCommand() const;

	/// @brief Resolve sample method (auto-selects default if SAMPLE_METHOD_COUNT).
	/// @threadsafe Yes (returns copy under lock).
	sample_method_t getResolvedSampleMethod(sample_method_t requested = SAMPLE_METHOD_COUNT) const;

	/// @brief Resolve scheduler for given sample method.
	/// @threadsafe Yes (returns copy under lock).
	scheduler_t getResolvedScheduler(
		sample_method_t requestedSampleMethod = SAMPLE_METHOD_COUNT,
		scheduler_t requestedSchedule = SCHEDULER_COUNT) const;

	/// @brief Get name of resolved sample method.
	/// @threadsafe Yes (returns copy under lock).
	std::string getResolvedSampleMethodName(sample_method_t requested = SAMPLE_METHOD_COUNT) const;

	/// @brief Get name of resolved scheduler.
	/// @threadsafe Yes (returns copy under lock).
	std::string getResolvedSchedulerName(
		sample_method_t requestedSampleMethod = SAMPLE_METHOD_COUNT,
		scheduler_t requestedSchedule = SCHEDULER_COUNT) const;

	/// @brief Get error history (up to 10 most recent errors).
	/// @threadsafe Yes (returns copy under lock).
	std::vector<ofxGgmlStableDiffusionError> getErrorHistory() const;

	/// @brief Clear error history.
	/// @threadsafe Yes.
	void clearErrorHistory();

	/// @brief Get video frame index for given time in seconds.
	/// @threadsafe Yes.
	int getVideoFrameIndexForTime(float seconds) const;

	/// @brief Get pointer to image pixels (lifetime tied to lastResult).
	/// @threadsafe No. Pointer becomes invalid when new generation completes.
	/// @warning Do not cache this pointer. Use copyImagePixels() for safe access.
	const ofPixels* getImagePixels(int index) const;

	/// @brief Copy image pixels safely.
	/// @threadsafe Yes (copies data under lock).
	bool copyImagePixels(int index, ofPixels& pixels) const;

	/// @brief Get image frame metadata (score, selection status).
	/// @threadsafe Yes.
	bool getImageFrameMetadata(
		int index,
		ofxGgmlStableDiffusionImageScore& score,
		bool& isSelected) const;

	/// @brief Get pointer to video frame pixels (lifetime tied to lastResult).
	/// @threadsafe No. Pointer becomes invalid when new generation completes.
	/// @warning Do not cache this pointer. Use copyVideoFramePixels() for safe access.
	const ofPixels* getVideoFramePixels(int index) const;

	/// @brief Copy video frame pixels safely.
	/// @threadsafe Yes (copies data under lock).
	bool copyVideoFramePixels(int index, ofPixels& pixels) const;

	/// @brief Get video frame metadata (seed, generation parameters).
	/// @threadsafe Yes.
	bool getVideoFrameMetadata(
		int index,
		int64_t& seed,
		ofxGgmlStableDiffusionGenerationParameters& generation) const;

	/// @brief Save video frames to directory as individual image files.
	/// @threadsafe Yes.
	bool saveVideoFrames(const std::string& directory, const std::string& prefix = "frame") const;

	/// @brief Save video metadata to JSON file.
	/// @threadsafe Yes.
	bool saveVideoMetadata(const std::string& path) const;

	/// @brief Save video frames and metadata together.
	/// @threadsafe Yes.
	bool saveVideoFramesWithMetadata(
		const std::string& directory,
		const std::string& prefix = "frame",
		const std::string& metadataFilename = "metadata.json") const;

	/// @brief Save video as WebM file (requires FFmpeg).
	/// @threadsafe Yes.
	bool saveVideoWebm(const std::string& path, int quality = 90) const;

	/// @brief Render a long video by generating multiple chunked clips sequentially.
	///
	/// This helper orchestrates the manifest-based long-video workflow:
	/// - validates the manifest
	/// - generates each chunk via generateVideo()
	/// - optionally uses the previous chunk's last frame as the next initImage
	/// - saves each chunk's PNG sequence + metadata JSON
	/// - returns a playlist manifest JSON describing the rendered chunks
	///
	/// @threadsafe No. This method blocks and drives the worker thread.
	/// @note Requires a loaded model context that supports image-to-video.
	/// @warning This function spins a small sleep loop while waiting for generation.
	ofxGgmlStableDiffusionLongVideoRunResult renderLongVideo(
		const ofxGgmlStableDiffusionLongVideoManifest& manifest,
		const std::string& framePrefix = "frame",
		const std::string& metadataFilename = "metadata.json",
		int pollIntervalMs = 10);

	/// @brief Set video generation mode (Standard, Loop, PingPong, Boomerang).
	/// @threadsafe Yes.
	/// @note This affects frame sequence construction for video output modes.
	/// Standard: forward only. Loop: adds loop-back frame. PingPong: forward then backward (excluding endpoints).
	/// Boomerang: forward then full reverse.
	void setVideoGenerationMode(ofxGgmlStableDiffusionVideoMode mode);

	/// @brief Get current video generation mode.
	/// @threadsafe Yes.
	ofxGgmlStableDiffusionVideoMode getVideoGenerationMode() const;

	/// @brief Set image generation mode (TextToImage, ImageToImage, etc).
	/// @threadsafe Yes.
	void setImageGenerationMode(ofxGgmlStableDiffusionImageMode mode);

	/// @brief Get current image generation mode.
	/// @threadsafe Yes.
	ofxGgmlStableDiffusionImageMode getImageGenerationMode() const;

	/// @brief Set image selection mode for multi-image generation.
	/// @threadsafe Yes.
	void setImageSelectionMode(ofxGgmlStableDiffusionImageSelectionMode mode);

	/// @brief Get current image selection mode.
	/// @threadsafe Yes.
	ofxGgmlStableDiffusionImageSelectionMode getImageSelectionMode() const;

	/// @brief Set callback for custom image ranking/scoring.
	/// @threadsafe Yes. Callback will be invoked from worker thread.
	/// @note Callback must be thread-safe and should not block for long.
	void setImageRankCallback(ofxSdImageRankCallback cb);

	/// @brief Get index of selected image in last batch.
	/// @threadsafe Yes.
	int getSelectedImageIndex() const;

	/// Load an input image from ofPixels. A deep copy is made immediately;
	/// the caller's pixels do not need to remain valid after this call returns.
	void loadImage(const ofPixels& pixels);

	/// Replace the active LoRA/LoCon stack for subsequent generations.
	void setLoras(const std::vector<ofxGgmlStableDiffusionLora>& loras_);
	std::vector<ofxGgmlStableDiffusionLora> getLoras() const;

	/// Query available LoRA files in the loraModelDir; returns name and absolute path pairs.
	std::vector<std::pair<std::string, std::string>> listLoras() const;

	/// Add a ControlNet to the active stack for subsequent generations.
	void addControlNet(const ofxGgmlStableDiffusionControlNet& controlNet);

	/// Clear all active ControlNets.
	void clearControlNets();

	/// Get the current ControlNet stack.
	std::vector<ofxGgmlStableDiffusionControlNet> getControlNets() const;

	/// Scan a directory for available model files
	std::vector<ofxGgmlStableDiffusionModelInfo> scanModels(const std::string& directory);

	/// Get metadata for a specific model file
	ofxGgmlStableDiffusionModelInfo getModelInfo(const std::string& modelPath);

	/// Get list of cached models
	std::vector<ofxGgmlStableDiffusionModelInfo> getCachedModels() const;

	/// Preload a model into cache for faster switching
	bool preloadModel(const std::string& modelPath, std::string& errorMessage);

	/// Clear the model cache
	void clearModelCache();

	/// Set model manager cache limits
	void setModelCacheSize(uint64_t maxBytes);
	void setMaxCachedModels(int count);

	/// Enable or disable performance profiling
	void setProfilingEnabled(bool enabled);
	bool isProfilingEnabled() const;

	/// Get performance statistics from the profiler
	ofxGgmlStableDiffusionPerformanceStats getPerformanceStats() const;

	/// Get a specific performance profile entry
	ofxGgmlStableDiffusionProfileEntry getPerformanceEntry(const std::string& name) const;

	/// Reset all performance profiling data
	void resetProfiling();

	/// Print performance summary to console
	void printPerformanceSummary() const;

	/// Get performance bottlenecks (operations taking > threshold % of total time)
	std::vector<std::string> getPerformanceBottlenecks(float thresholdPercent = 10.0f) const;

	/// Export performance data to JSON
	std::string exportPerformanceJSON() const;

	/// Export performance data to CSV
	std::string exportPerformanceCSV() const;

	/// Reload embeddings by rebuilding the context with the current (or provided) embed directory.
	void reloadEmbeddings(const std::string& embedDir = "");

	/// Query current embeddings on disk (resolved from embedDir); loads names and paths.
	std::vector<std::pair<std::string, std::string>> listEmbeddings() const;

	bool isDiffused() const;
	void setDiffused(bool diffused);
	/// Returned buffers are owned by the addon; they become invalid after a new
	/// generation starts, output is cleared, or the addon is destroyed.
	sd_image_t* returnImages() const;

	/// Return the human-readable name for an sd_type_t enum value.
	const char* typeName(enum sd_type_t type);

	int32_t getNumPhysicalCores();
	const char* getSystemInfo();

	/// Set an optional progress callback that fires on each diffusion step.
	/// The callback runs on the worker thread; keep it lightweight and avoid touching
	/// UI/rendering objects directly.
	void setProgressCallback(ofxSdProgressCallback cb);
	void setNativeLoggingEnabled(bool enabled);
	bool isNativeLoggingEnabled() const;
	void setNativeLogLevel(sd_log_level_t level);
	sd_log_level_t getNativeLogLevel() const;

	void newSdCtx(const ofxGgmlStableDiffusionContextSettings& settings);
	void freeSdCtx();

	void txt2img(const std::string& prompt,
		const std::string& negativePrompt,
		int clipSkip,
		float cfgScale,
		int width,
		int height,
		sample_method_t sampleMethod,
		int sampleSteps,
		int64_t seed,
		int batchCount,
		sd_image_t* controlCond,
		float controlStrength,
		float styleStrength,
		const std::string& inputIdImagesPath);

	void img2img(sd_image_t initImage,
		const std::string& prompt,
		const std::string& negativePrompt,
		int clipSkip,
		float cfgScale,
		int width,
		int height,
		enum sample_method_t sampleMethod,
		int sampleSteps,
		float strength,
		int64_t seed,
		int batchCount,
		sd_image_t* controlCond,
		float controlStrength,
		float styleStrength,
		const std::string& inputIdImagesPath);
	void img2vid(sd_image_t init_image,
		int width,
		int height,
		int videoFrames,
		int fps,
		float cfgScale,
		enum sample_method_t sampleMethod,
		int sampleSteps,
		float strength,
		int64_t seed);

	void newUpscalerCtx(const std::string& esrganPath,
		int nThreads,
		enum sd_type_t wType);
	void freeUpscalerCtx();

	sd_image_t upscaleImage(sd_image_t inputImage,
		uint32_t upscaleFactor);

	bool convert(const char* inputPath,
		const char* vaePath,
		const char* outputPath,
		sd_type_t outputType);

	/// Run Canny edge detection on an image buffer in-place.
	/// The buffer is modified in-place; the returned pointer is the same as @p img.
	/// @param channels Number of channels in the image buffer (typically 1 for grayscale).
	uint8_t* preprocessCanny(uint8_t* img,
		int width,
		int height,
		float highThreshold,
		float lowThreshold,
		float weak,
		float strong,
		bool inverse,
		int channels = 1);

	/// Returns true if generation is currently in progress.
	/// @note Thread-safe. Can be called from any thread.
	bool isGenerating() const;
	bool isBusy() const;

	/// Request cancellation of current generation.
	/// @return true if cancellation was requested, false if nothing is running
	/// @note Thread-safe. Cancellation is best-effort and completes at the earliest safe checkpoint.
	bool requestCancellation();

	/// Check if cancellation was requested.
	/// @return true if cancellation is pending
	/// @note Thread-safe. Can be called from any thread.
	bool isCancellationRequested() const;

	/// Check if last operation was cancelled.
	/// @return true if the last operation completed due to cancellation
	bool wasCancelled() const;


	bool matchesContextSettings(const ofxGgmlStableDiffusionContextSettings& settings) const;

	/// Returns the actual seed used in the last generation (auto-generated if seed was -1).
	int64_t getLastUsedSeed() const;

	/// Returns the seed history from recent generations (up to 20 most recent).
	std::vector<int64_t> getSeedHistory() const;

	/// Clear the seed history.
	void clearSeedHistory();

	/// Hash a string to a deterministic seed value for reproducibility.
	static int64_t hashStringToSeed(const std::string& text);

	std::string prompt;
	std::string negativePrompt;
	int width = 512;
	int height = 512;
	float cfgScale = 7.0f;
	int batchCount = 1;
	float strength = 0.5f;
	int64_t seed = -1;
	int clipSkip = -1;
	// Context model paths
	std::string modelPath;
	std::string modelName;
	std::string diffusionModelPath;
	std::string clipLPath;
	std::string clipGPath;
	std::string t5xxlPath;
	std::string taesdPath;
	std::string controlNetPathCStr;
	std::string embedDirCStr;
	std::string loraModelDir;
	std::string vaePath;
	std::string esrganPath;
	std::string stackedIdEmbedDirCStr;
	std::string inputIdImagesPath;
	sample_method_t sampleMethodEnum = EULER_A_SAMPLE_METHOD;
	int sampleSteps = 20;
	int videoFrames = 6;
	int fps = 6;
	float vaceStrength = 1.0f;
	bool vaeDecodeOnly = false;
	bool vaeTiling = false;
	bool freeParamsImmediately = false;
	bool isFullScreen = false;
	bool isTAESD = false;
	bool isESRGAN = false;
	bool keepClipOnCpu = false;
	bool keepControlNetCpu = false;
	bool keepVaeOnCpu = false;
	bool offloadParamsToCpu = false;
	bool flashAttn = false;
	bool diffusionFlashAttn = false;
	bool enableMmap = true;
	std::string maxVram;
	bool streamLayers = false;
	bool eagerLoad = false;
	std::string backend;
	std::string paramsBackend;
	std::string splitMode;
	bool autoFit = false;
	float styleStrength = 20.0f;
	int nThreads = -1;
	int esrganMultiplier = 4;
	sd_type_t wType = SD_TYPE_COUNT;
	scheduler_t schedule = SCHEDULER_COUNT;
	rng_type_t rngType = CUDA_RNG;
	prediction_t prediction = EPS_PRED;
	lora_apply_mode_t loraApplyMode = LORA_APPLY_AUTO;
	std::string controlImagePath;
	float controlStrength = 0.9f;
	std::vector<ofxGgmlStableDiffusionLora> loras;
	std::vector<ofxGgmlStableDiffusionControlNet> controlNets;

	sd_image_t inputImage = {0, 0, 0, nullptr};
	sd_image_t maskImage = {0, 0, 0, nullptr};
	sd_image_t endImage = {0, 0, 0, nullptr};
	sd_image_t* outputImages = nullptr;
	sd_image_t* controlCond = nullptr;
	ofxGgmlStableDiffusionThread thread;
	/// @deprecated Internal state flags; use getLastResult().task instead. Thread-safe via atomic.
	std::atomic<bool> isTextToImage{false};
	/// @deprecated Internal state flags; use getLastResult().task instead. Thread-safe via atomic.
	std::atomic<bool> isImageToVideo{false};
	std::atomic<bool> isModelLoading{false};
	bool diffused = false;

	ofxSdProgressCallback progressCallback;
	ofxSdImageRankCallback imageRankCallback;

private:
	friend class ofxGgmlStableDiffusionThread;

	bool beginBackgroundTask(ofxGgmlStableDiffusionTask task);
	void finishBackgroundTask(bool cancelled = false, const std::string& cancelMessage = "");
	void applyContextSettings(const ofxGgmlStableDiffusionContextSettings& settings);
	bool applyImageRequest(const ofxGgmlStableDiffusionImageRequest& request);
	bool applyVideoRequest(const ofxGgmlStableDiffusionVideoRequest& request);
	void clearResolvedDefaultCachesNoLock();
	void refreshResolvedDefaultCachesNoLock(sd_ctx_t* ctx);
	bool validateImageRequestAndSetError(const ofxGgmlStableDiffusionImageRequest& request, ofxGgmlStableDiffusionTask task);
	bool validateVideoRequestAndSetError(const ofxGgmlStableDiffusionVideoRequest& request);
	void clearOutputState();
	ofxGgmlStableDiffusionContextSettings captureContextSettingsNoLock() const;
	ofxGgmlStableDiffusionUpscalerSettings captureUpscalerSettingsNoLock() const;
	void setLastError(const std::string& errorMessage, ofxGgmlStableDiffusionErrorCode code = ofxGgmlStableDiffusionErrorCode::Unknown);
	void setLastError(ofxGgmlStableDiffusionErrorCode code, const std::string& errorMessage);
	void setLastErrorPreservingResult(
		const std::string& errorMessage,
		ofxGgmlStableDiffusionErrorCode code = ofxGgmlStableDiffusionErrorCode::Unknown);
	void setLastErrorPreservingResult(ofxGgmlStableDiffusionErrorCode code, const std::string& errorMessage);
	void setLastResolvedVideoRequestSummary(const std::string& summary);
	void setLastResolvedVideoCliCommand(const std::string& command);
	void clearLastError();
	void captureImageResults(
		sd_image_t* images,
		int count,
		int64_t seedValue,
		float elapsedMs,
		ofxGgmlStableDiffusionTask task,
		const ofxGgmlStableDiffusionImageRequest& request,
		const ofxSdImageRankCallback& rankCallback);
	void captureVideoResults(
		sd_image_t* images,
		int count,
		int64_t seedValue,
		const std::vector<int64_t>& frameSeeds,
		const std::vector<ofxGgmlStableDiffusionGenerationParameters>& frameGeneration,
		float elapsedMs,
		ofxGgmlStableDiffusionTask task,
		const ofxGgmlStableDiffusionVideoRequest& request);
	void applyImageRanking(
		std::vector<ofxGgmlStableDiffusionImageFrame>& frames,
		ofxGgmlStableDiffusionResult& result,
		const ofxGgmlStableDiffusionImageRequest& request,
		const ofxSdImageRankCallback& rankCallback);
	std::vector<sd_image_t> buildOutputImageViews(const ofxGgmlStableDiffusionResult& result) const;
	ofPixels makePixelsCopy(const sd_image_t& image) const;

	ofxGgmlStableDiffusionTask activeTask = ofxGgmlStableDiffusionTask::None;
	ofxGgmlStableDiffusionImageMode imageMode = ofxGgmlStableDiffusionImageMode::TextToImage;
	ofxGgmlStableDiffusionImageSelectionMode imageSelectionMode =
		ofxGgmlStableDiffusionImageSelectionMode::KeepOrder;
	ofxGgmlStableDiffusionVideoMode videoMode = ofxGgmlStableDiffusionVideoMode::Standard;
	ofxGgmlStableDiffusionResult lastResult;
	std::vector<sd_image_t> outputImageViews;
	std::string lastError;
	ofxGgmlStableDiffusionError lastErrorInfo;
	std::string lastResolvedVideoRequestSummary;
	std::string lastResolvedVideoCliCommand;
	std::deque<ofxGgmlStableDiffusionError> errorHistory;
	static constexpr std::size_t maxErrorHistorySize = 10;
	std::deque<int64_t> seedHistory;
	static constexpr std::size_t maxSeedHistorySize = 20;
	uint64_t taskStartMicros = 0;
	ofxGgmlStableDiffusionThread::OwnedImage loadedInputImage;
	ofxGgmlStableDiffusionModelManager modelManager;
	ofxGgmlStableDiffusionPerformanceProfiler performanceProfiler;
	ofxGgmlStableDiffusionUpscalerSettings cachedUpscalerSettings;
	mutable std::mutex stateMutex;
	sample_method_t cachedResolvedDefaultSampleMethod = SAMPLE_METHOD_COUNT;
	scheduler_t cachedResolvedDefaultScheduler = SCHEDULER_COUNT;
	std::vector<scheduler_t> cachedResolvedSchedulersBySampleMethod;
	bool lastOperationCancelled = false;
};


