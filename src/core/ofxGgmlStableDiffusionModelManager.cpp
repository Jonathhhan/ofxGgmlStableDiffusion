#include "ofxGgmlStableDiffusionModelManager.h"
#include <algorithm>
#include <cmath>

namespace {

const char* emptyToNull(const std::string& value) {
	return value.empty() ? nullptr : value.c_str();
}

} // namespace

//--------------------------------------------------------------
ofxGgmlStableDiffusionModelManager::ofxGgmlStableDiffusionModelManager() {
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionModelManager::~ofxGgmlStableDiffusionModelManager() {
	clearCache();
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::setMaxCacheSize(uint64_t bytes) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	maxCacheSizeBytes = bytes;
	// Evict models if we're over the new limit
	while (maxCacheSizeBytes > 0 && calculateCacheSize() > maxCacheSizeBytes) {
		if (!evictLRUModel()) {
			break;
		}
	}
}

//--------------------------------------------------------------
uint64_t ofxGgmlStableDiffusionModelManager::getCurrentCacheSize() const {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return calculateCacheSize();
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::setMaxCachedModels(int count) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	maxCachedModels = count;
	// Evict models if we're over the new limit
	while (maxCachedModels > 0 && static_cast<int>(modelCache.size()) > maxCachedModels) {
		if (!evictLRUModel()) {
			break;
		}
	}
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionModelManager::getCachedModelCount() const {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return static_cast<int>(modelCache.size());
}

//--------------------------------------------------------------
std::vector<ofxGgmlStableDiffusionModelInfo> ofxGgmlStableDiffusionModelManager::scanModelsInDirectory(const std::string& directory) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	std::vector<ofxGgmlStableDiffusionModelInfo> models;

	ofDirectory dir(directory);
	if (!dir.exists()) {
		ofLogWarning("ofxGgmlStableDiffusionModelManager") << "Directory does not exist: " << directory;
		return models;
	}

	dir.allowExt("safetensors");
	dir.allowExt("ckpt");
	dir.allowExt("gguf");
	dir.listDir();

	for (std::size_t i = 0; i < dir.size(); ++i) {
		const std::string modelPath = dir.getPath(i);
		ofxGgmlStableDiffusionModelInfo info = extractModelInfo(modelPath);
		if (info.isValid) {
			models.push_back(info);
		}
	}

	return models;
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionModelInfo ofxGgmlStableDiffusionModelManager::extractModelInfo(const std::string& modelPath) {
	ofxGgmlStableDiffusionModelInfo info;
	info.modelPath = modelPath;
	info.modelName = ofFilePath::getFileName(modelPath);

	// Check if file exists
	ofFile file(modelPath);
	if (!file.exists()) {
		info.isValid = false;
		info.errorMessage = "Model file not found";
		return info;
	}

	info.fileSizeBytes = static_cast<uint64_t>(file.getSize());

	// Validate model file format
	if (!isValidModelFile(modelPath)) {
		info.isValid = false;
		info.errorMessage = "Invalid model file format";
		return info;
	}

	// Extract model type from filename or path
	info.modelType = extractModelType(modelPath);

	// Estimate memory requirements (rough estimate: file size * 1.2)
	info.estimatedMemoryBytes = static_cast<uint64_t>(info.fileSizeBytes * 1.2);

	// Set native resolution based on model type
	if (info.modelType.find("XL") != std::string::npos) {
		info.nativeWidth = 1024;
		info.nativeHeight = 1024;
	} else {
		info.nativeWidth = 512;
		info.nativeHeight = 512;
	}

	info.isValid = true;
	return info;
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionModelManager::validateModel(const std::string& modelPath, std::string& errorMessage) {
	ofFile file(modelPath);
	if (!file.exists()) {
		errorMessage = "Model file not found: " + modelPath;
		return false;
	}

	if (!isValidModelFile(modelPath)) {
		errorMessage = "Invalid model file format: " + modelPath;
		return false;
	}

	return true;
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionModelManager::preloadModel(const ofxGgmlStableDiffusionModelInfo& modelInfo, std::string& errorMessage) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	// Check if already loaded
	if (isModelLoaded(modelInfo.modelPath)) {
		ofLogNotice("ofxGgmlStableDiffusionModelManager") << "Model already loaded: " << modelInfo.modelName;
		return true;
	}

	// Validate model
	if (!validateModel(modelInfo.modelPath, errorMessage)) {
		return false;
	}

	// Check cache limits and evict if necessary
	if (autoEvictionEnabled) {
		while ((maxCachedModels > 0 && static_cast<int>(modelCache.size()) >= maxCachedModels) ||
			   (maxCacheSizeBytes > 0 && calculateCacheSize() + modelInfo.estimatedMemoryBytes > maxCacheSizeBytes)) {
			if (!evictLRUModel()) {
				errorMessage = "Cache full and cannot evict models";
				return false;
			}
		}
	}

	// Load the model context
	reportProgress(modelInfo.modelPath, 0.0f, "Starting model load");

	sd_ctx_t* ctx = loadModelContext(modelInfo, errorMessage);
	if (!ctx) {
		return false;
	}

	// Add to cache
	ofxGgmlStableDiffusionModelCacheEntry entry;
	entry.info = modelInfo;
	entry.info.isLoaded = true;
	entry.info.loadTimeMicros = ofGetElapsedTimeMicros();
	entry.sdCtx = ctx;
	entry.lastUsedTimeMicros = ofGetElapsedTimeMicros();
	entry.referenceCount = 0;

	modelCache[modelInfo.modelPath] = entry;

	reportProgress(modelInfo.modelPath, 1.0f, "Model loaded successfully");

	ofLogNotice("ofxGgmlStableDiffusionModelManager") << "Model preloaded: " << modelInfo.modelName;
	return true;
}

//--------------------------------------------------------------
sd_ctx_t* ofxGgmlStableDiffusionModelManager::getModelContext(const std::string& modelPath,
	const ofxGgmlStableDiffusionModelInfo& modelInfo,
	std::string& errorMessage) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	// Check if already cached
	auto it = modelCache.find(modelPath);
	if (it != modelCache.end()) {
		cacheHits++;
		updateAccessTime(modelPath);
		it->second.referenceCount++;
		return it->second.sdCtx;
	}

	cacheMisses++;

	// Not cached, load it
	if (!preloadModel(modelInfo, errorMessage)) {
		return nullptr;
	}

	// Get the newly loaded context
	it = modelCache.find(modelPath);
	if (it != modelCache.end()) {
		it->second.referenceCount++;
		return it->second.sdCtx;
	}

	errorMessage = "Failed to cache model after loading";
	return nullptr;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::releaseModelContext(const std::string& modelPath) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	auto it = modelCache.find(modelPath);
	if (it != modelCache.end()) {
		if (it->second.referenceCount > 0) {
			it->second.referenceCount--;
		}
		updateAccessTime(modelPath);
	}
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionModelManager::unloadModel(const std::string& modelPath) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	auto it = modelCache.find(modelPath);
	if (it == modelCache.end()) {
		return false;
	}

	// Don't unload if it's in use
	if (it->second.referenceCount > 0) {
		ofLogWarning("ofxGgmlStableDiffusionModelManager") << "Cannot unload model in use: " << it->second.info.modelName;
		return false;
	}

	// Free the native context
	if (it->second.sdCtx) {
		free_sd_ctx(it->second.sdCtx);
		it->second.sdCtx = nullptr;
	}

	modelCache.erase(it);
	ofLogNotice("ofxGgmlStableDiffusionModelManager") << "Model unloaded: " << modelPath;
	return true;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::clearCache() {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	for (auto it = modelCache.begin(); it != modelCache.end();) {
		if (it->second.referenceCount > 0) {
			ofLogWarning("ofxGgmlStableDiffusionModelManager")
				<< "Skipping in-use model during cache clear: "
				<< it->second.info.modelName;
			++it;
			continue;
		}
		if (it->second.sdCtx) {
			free_sd_ctx(it->second.sdCtx);
			it->second.sdCtx = nullptr;
		}
		it = modelCache.erase(it);
	}
	ofLogNotice("ofxGgmlStableDiffusionModelManager") << "Model cache cleared";
}

//--------------------------------------------------------------
std::optional<ofxGgmlStableDiffusionModelInfo> ofxGgmlStableDiffusionModelManager::getCachedModelInfo(const std::string& modelPath) const {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	auto it = modelCache.find(modelPath);
	if (it != modelCache.end()) {
		return it->second.info;
	}
	return std::nullopt;
}

//--------------------------------------------------------------
std::vector<ofxGgmlStableDiffusionModelInfo> ofxGgmlStableDiffusionModelManager::getCachedModels() const {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	std::vector<ofxGgmlStableDiffusionModelInfo> models;
	models.reserve(modelCache.size());
	for (const auto& pair : modelCache) {
		models.push_back(pair.second.info);
	}
	return models;
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionModelManager::isModelLoaded(const std::string& modelPath) const {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	auto it = modelCache.find(modelPath);
	return it != modelCache.end() && it->second.sdCtx != nullptr;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::setProgressCallback(ofxModelLoadProgressCallback cb) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	progressCallback = cb;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::setAutoEviction(bool enabled) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	autoEvictionEnabled = enabled;
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionModelManager::CacheStats ofxGgmlStableDiffusionModelManager::getCacheStats() const {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	CacheStats stats;
	stats.totalModels = static_cast<int>(modelCache.size());
	stats.loadedModels = 0;
	stats.totalMemoryBytes = 0;

	for (const auto& pair : modelCache) {
		if (pair.second.sdCtx) {
			stats.loadedModels++;
		}
		stats.totalMemoryBytes += pair.second.info.estimatedMemoryBytes;
	}

	stats.availableMemoryBytes = maxCacheSizeBytes > stats.totalMemoryBytes ?
		maxCacheSizeBytes - stats.totalMemoryBytes : 0;
	stats.cacheHits = cacheHits;
	stats.cacheMisses = cacheMisses;

	return stats;
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionModelManager::evictLRUModel() {
	if (modelCache.empty()) {
		return false;
	}

	// Find the least recently used model with zero references
	std::string lruPath;
	uint64_t oldestTime = UINT64_MAX;

	for (const auto& pair : modelCache) {
		if (pair.second.referenceCount == 0 && pair.second.lastUsedTimeMicros < oldestTime) {
			oldestTime = pair.second.lastUsedTimeMicros;
			lruPath = pair.first;
		}
	}

	if (lruPath.empty()) {
		ofLogWarning("ofxGgmlStableDiffusionModelManager") << "Cannot evict: all models are in use";
		return false;
	}

	ofLogNotice("ofxGgmlStableDiffusionModelManager") << "Evicting LRU model: " << lruPath;
	return unloadModel(lruPath);
}

//--------------------------------------------------------------
uint64_t ofxGgmlStableDiffusionModelManager::calculateCacheSize() const {
	uint64_t totalSize = 0;
	for (const auto& pair : modelCache) {
		totalSize += pair.second.info.estimatedMemoryBytes;
	}
	return totalSize;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::updateAccessTime(const std::string& modelPath) {
	auto it = modelCache.find(modelPath);
	if (it != modelCache.end()) {
		it->second.lastUsedTimeMicros = ofGetElapsedTimeMicros();
		it->second.info.lastAccessTimeMicros = ofGetElapsedTimeMicros();
	}
}

//--------------------------------------------------------------
sd_ctx_t* ofxGgmlStableDiffusionModelManager::loadModelContext(const ofxGgmlStableDiffusionModelInfo& modelInfo, std::string& errorMessage) {
	reportProgress(modelInfo.modelPath, 0.1f, "Validating model file");

	if (!validateModel(modelInfo.modelPath, errorMessage)) {
		return nullptr;
	}

	reportProgress(modelInfo.modelPath, 0.3f, "Loading model into memory");

	sd_ctx_params_t ctxParams{};
	sd_ctx_params_init(&ctxParams);

	ctxParams.model_path = emptyToNull(modelInfo.modelPath);
	ctxParams.vae_path = emptyToNull(modelInfo.vaePath);
	ctxParams.taesd_path = emptyToNull(modelInfo.taesdPath);
	ctxParams.control_net_path = emptyToNull(modelInfo.controlNetPath);
	ctxParams.photo_maker_path = nullptr;
	ctxParams.vae_decode_only = false;
	ctxParams.free_params_immediately = false;
	ctxParams.n_threads = -1;  // auto
	ctxParams.wtype = modelInfo.weightType;
	ctxParams.rng_type = CUDA_RNG;
	ctxParams.sampler_rng_type = RNG_TYPE_COUNT;

	// Load model using stable-diffusion.cpp API
	sd_ctx_t* ctx = new_sd_ctx(&ctxParams);

	if (!ctx) {
		errorMessage = "Failed to create model context for: " + modelInfo.modelPath;
		reportProgress(modelInfo.modelPath, 0.0f, "Failed to load model");
		return nullptr;
	}

	reportProgress(modelInfo.modelPath, 0.9f, "Finalizing model load");

	return ctx;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionModelManager::reportProgress(const std::string& modelPath, float progress, const std::string& stage) {
	ofxModelLoadProgressCallback callback;
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		callback = progressCallback;
	}
	if (callback) {
		callback(modelPath, progress, stage);
	}
}

//--------------------------------------------------------------
uint64_t ofxGgmlStableDiffusionModelManager::estimateModelMemory(const std::string& modelPath) const {
	ofFile file(modelPath);
	if (!file.exists()) {
		return 0;
	}
	// Rough estimate: file size * 1.2 (accounting for runtime overhead)
	return static_cast<uint64_t>(file.getSize() * 1.2);
}

//--------------------------------------------------------------
std::string ofxGgmlStableDiffusionModelManager::extractModelType(const std::string& modelPath) const {
	std::string filename = ofToLower(ofFilePath::getFileName(modelPath));

	if (filename.find("xl") != std::string::npos || filename.find("sdxl") != std::string::npos) {
		return "SDXL";
	} else if (filename.find("turbo") != std::string::npos) {
		return "SD-Turbo";
	} else if (filename.find("1.5") != std::string::npos || filename.find("v1-5") != std::string::npos) {
		return "SD1.5";
	} else if (filename.find("2.1") != std::string::npos || filename.find("v2-1") != std::string::npos) {
		return "SD2.1";
	}

	return "Unknown";
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionModelManager::isValidModelFile(const std::string& modelPath) const {
	std::string ext = ofToLower(ofFilePath::getFileExt(modelPath));
	return ext == "safetensors" || ext == "ckpt" || ext == "gguf";
}
