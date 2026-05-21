#include "ofxGgmlStableDiffusionQuantization.h"
#include "ofxGgmlStableDiffusionStringUtils.h"
#include <algorithm>

std::vector<ofxGgmlStableDiffusionQuantizationLevelInfo> ofxGgmlStableDiffusionQuantizationHelpers::levelInfoCache;

std::vector<ofxGgmlStableDiffusionQuantizationLevelInfo> ofxGgmlStableDiffusionQuantizationHelpers::getAvailableLevels() {
	if (levelInfoCache.empty()) {
		initializeLevelInfo();
	}
	return levelInfoCache;
}

ofxGgmlStableDiffusionQuantizationLevelInfo ofxGgmlStableDiffusionQuantizationHelpers::getLevelInfo(
	ofxGgmlStableDiffusionQuantization level) {

	if (levelInfoCache.empty()) {
		initializeLevelInfo();
	}

	for (const auto& info : levelInfoCache) {
		if (info.level == level) {
			return info;
		}
	}

	// Return default info for None
	ofxGgmlStableDiffusionQuantizationLevelInfo defaultInfo;
	defaultInfo.level = ofxGgmlStableDiffusionQuantization::None;
	defaultInfo.name = "None";
	defaultInfo.description = "Full precision (F32)";
	return defaultInfo;
}

ofxGgmlStableDiffusionQuantization ofxGgmlStableDiffusionQuantizationHelpers::getQuantizationByName(
	const std::string& name) {

	std::string lowerName = ofxSdToLowerCopy(name);

	if (lowerName == "none" || lowerName == "f32") return ofxGgmlStableDiffusionQuantization::None;
	if (lowerName == "f16") return ofxGgmlStableDiffusionQuantization::F16;
	if (lowerName == "q8_0") return ofxGgmlStableDiffusionQuantization::Q8_0;
	if (lowerName == "q5_1") return ofxGgmlStableDiffusionQuantization::Q5_1;
	if (lowerName == "q5_0") return ofxGgmlStableDiffusionQuantization::Q5_0;
	if (lowerName == "q4_1") return ofxGgmlStableDiffusionQuantization::Q4_1;
	if (lowerName == "q4_0") return ofxGgmlStableDiffusionQuantization::Q4_0;

	return ofxGgmlStableDiffusionQuantization::None;
}

std::string ofxGgmlStableDiffusionQuantizationHelpers::getQuantizationName(
	ofxGgmlStableDiffusionQuantization level) {

	switch (level) {
		case ofxGgmlStableDiffusionQuantization::None: return "None (F32)";
		case ofxGgmlStableDiffusionQuantization::F16: return "F16";
		case ofxGgmlStableDiffusionQuantization::Q8_0: return "Q8_0";
		case ofxGgmlStableDiffusionQuantization::Q5_1: return "Q5_1";
		case ofxGgmlStableDiffusionQuantization::Q5_0: return "Q5_0";
		case ofxGgmlStableDiffusionQuantization::Q4_1: return "Q4_1";
		case ofxGgmlStableDiffusionQuantization::Q4_0: return "Q4_0";
		default: return "Unknown";
	}
}

size_t ofxGgmlStableDiffusionQuantizationHelpers::estimateQuantizedSize(
	size_t originalSizeMB,
	ofxGgmlStableDiffusionQuantization level) {

	float reduction = getMemoryReduction(level);
	return static_cast<size_t>(originalSizeMB * (1.0f - reduction / 100.0f));
}

ofxGgmlStableDiffusionQuantization ofxGgmlStableDiffusionQuantizationHelpers::recommendQuantization(
	size_t availableVRAM_MB,
	size_t modelSizeMB) {

	// Leave some headroom for activations and intermediate buffers
	size_t usableVRAM = static_cast<size_t>(availableVRAM_MB * 0.7f);

	// If model fits comfortably, use full precision
	if (modelSizeMB < usableVRAM) {
		return ofxGgmlStableDiffusionQuantization::None;
	}

	// Try each quantization level from highest to lowest quality
	std::vector<ofxGgmlStableDiffusionQuantization> levels = {
		ofxGgmlStableDiffusionQuantization::F16,
		ofxGgmlStableDiffusionQuantization::Q8_0,
		ofxGgmlStableDiffusionQuantization::Q5_1,
		ofxGgmlStableDiffusionQuantization::Q5_0,
		ofxGgmlStableDiffusionQuantization::Q4_1,
		ofxGgmlStableDiffusionQuantization::Q4_0
	};

	for (auto level : levels) {
		size_t quantizedSize = estimateQuantizedSize(modelSizeMB, level);
		if (quantizedSize < usableVRAM) {
			return level;
		}
	}

	// If even Q4_0 doesn't fit, still recommend it as best option
	return ofxGgmlStableDiffusionQuantization::Q4_0;
}

ofxGgmlStableDiffusionQuantization ofxGgmlStableDiffusionQuantizationHelpers::getPreset(
	const std::string& preset) {

	std::string lowerPreset = ofxSdToLowerCopy(preset);

	if (lowerPreset == "ultra_fast") return ofxGgmlStableDiffusionQuantization::Q4_0;
	if (lowerPreset == "fast") return ofxGgmlStableDiffusionQuantization::Q5_0;
	if (lowerPreset == "balanced") return ofxGgmlStableDiffusionQuantization::Q8_0;
	if (lowerPreset == "quality") return ofxGgmlStableDiffusionQuantization::F16;
	if (lowerPreset == "max_quality") return ofxGgmlStableDiffusionQuantization::None;

	return ofxGgmlStableDiffusionQuantization::None;
}

bool ofxGgmlStableDiffusionQuantizationHelpers::isSupported(ofxGgmlStableDiffusionQuantization level) {
	// All levels are conceptually supported
	// In practice, support depends on the underlying stable-diffusion.cpp build
	return true;
}

float ofxGgmlStableDiffusionQuantizationHelpers::getMemoryReduction(
	ofxGgmlStableDiffusionQuantization level) {

	switch (level) {
		case ofxGgmlStableDiffusionQuantization::None: return 0.0f;
		case ofxGgmlStableDiffusionQuantization::F16: return 50.0f;
		case ofxGgmlStableDiffusionQuantization::Q8_0: return 50.0f;
		case ofxGgmlStableDiffusionQuantization::Q5_1: return 65.0f;
		case ofxGgmlStableDiffusionQuantization::Q5_0: return 65.0f;
		case ofxGgmlStableDiffusionQuantization::Q4_1: return 75.0f;
		case ofxGgmlStableDiffusionQuantization::Q4_0: return 75.0f;
		default: return 0.0f;
	}
}

float ofxGgmlStableDiffusionQuantizationHelpers::getSpeedMultiplier(
	ofxGgmlStableDiffusionQuantization level) {

	switch (level) {
		case ofxGgmlStableDiffusionQuantization::None: return 1.0f;
		case ofxGgmlStableDiffusionQuantization::F16: return 1.2f;
		case ofxGgmlStableDiffusionQuantization::Q8_0: return 1.3f;
		case ofxGgmlStableDiffusionQuantization::Q5_1: return 1.5f;
		case ofxGgmlStableDiffusionQuantization::Q5_0: return 1.5f;
		case ofxGgmlStableDiffusionQuantization::Q4_1: return 1.8f;
		case ofxGgmlStableDiffusionQuantization::Q4_0: return 2.0f;
		default: return 1.0f;
	}
}

void ofxGgmlStableDiffusionQuantizationHelpers::initializeLevelInfo() {
	levelInfoCache.clear();

	// F32 (None)
	{
		ofxGgmlStableDiffusionQuantizationLevelInfo info;
		info.level = ofxGgmlStableDiffusionQuantization::None;
		info.name = "None (F32)";
		info.description = "Full precision 32-bit floating point. Maximum quality, highest memory usage.";
		info.memoryReductionPercent = 0.0f;
		info.speedMultiplier = 1.0f;
		info.qualityEstimate = 1.0f;
		info.recommendation = "Use when VRAM is abundant and maximum quality is needed.";
		levelInfoCache.push_back(info);
	}

	// F16
	{
		ofxGgmlStableDiffusionQuantizationLevelInfo info;
		info.level = ofxGgmlStableDiffusionQuantization::F16;
		info.name = "F16";
		info.description = "Half precision 16-bit floating point. Excellent quality with 50% memory reduction.";
		info.memoryReductionPercent = 50.0f;
		info.speedMultiplier = 1.2f;
		info.qualityEstimate = 0.98f;
		info.recommendation = "Best balance for most use cases. Minimal quality loss with good memory savings.";
		levelInfoCache.push_back(info);
	}

	// Q8_0
	{
		ofxGgmlStableDiffusionQuantizationLevelInfo info;
		info.level = ofxGgmlStableDiffusionQuantization::Q8_0;
		info.name = "Q8_0";
		info.description = "8-bit integer quantization. Very good quality with 50% memory reduction.";
		info.memoryReductionPercent = 50.0f;
		info.speedMultiplier = 1.3f;
		info.qualityEstimate = 0.95f;
		info.recommendation = "Good for systems with moderate VRAM. Slight quality loss, faster inference.";
		levelInfoCache.push_back(info);
	}

	// Q5_1
	{
		ofxGgmlStableDiffusionQuantizationLevelInfo info;
		info.level = ofxGgmlStableDiffusionQuantization::Q5_1;
		info.name = "Q5_1";
		info.description = "5-bit quantization with improved accuracy. Balanced compression at 65% reduction.";
		info.memoryReductionPercent = 65.0f;
		info.speedMultiplier = 1.5f;
		info.qualityEstimate = 0.90f;
		info.recommendation = "Good for 6-8GB VRAM systems. Noticeable but acceptable quality trade-off.";
		levelInfoCache.push_back(info);
	}

	// Q5_0
	{
		ofxGgmlStableDiffusionQuantizationLevelInfo info;
		info.level = ofxGgmlStableDiffusionQuantization::Q5_0;
		info.name = "Q5_0";
		info.description = "5-bit quantization. Good compression at 65% reduction.";
		info.memoryReductionPercent = 65.0f;
		info.speedMultiplier = 1.5f;
		info.qualityEstimate = 0.88f;
		info.recommendation = "Suitable for limited VRAM scenarios. Some quality loss visible.";
		levelInfoCache.push_back(info);
	}

	// Q4_1
	{
		ofxGgmlStableDiffusionQuantizationLevelInfo info;
		info.level = ofxGgmlStableDiffusionQuantization::Q4_1;
		info.name = "Q4_1";
		info.description = "4-bit quantization with improved accuracy. Maximum compression at 75% reduction.";
		info.memoryReductionPercent = 75.0f;
		info.speedMultiplier = 1.8f;
		info.qualityEstimate = 0.82f;
		info.recommendation = "For very limited VRAM (4-6GB). Significant quality loss, fastest inference.";
		levelInfoCache.push_back(info);
	}

	// Q4_0
	{
		ofxGgmlStableDiffusionQuantizationLevelInfo info;
		info.level = ofxGgmlStableDiffusionQuantization::Q4_0;
		info.name = "Q4_0";
		info.description = "4-bit quantization. Ultra-compressed at 75% reduction.";
		info.memoryReductionPercent = 75.0f;
		info.speedMultiplier = 2.0f;
		info.qualityEstimate = 0.80f;
		info.recommendation = "Last resort for minimal VRAM systems. Maximum speed, lowest quality.";
		levelInfoCache.push_back(info);
	}
}
