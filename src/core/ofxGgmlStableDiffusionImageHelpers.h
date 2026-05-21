#pragma once

#include "ofxGgmlStableDiffusionEnums.h"

inline const char * ofxGgmlStableDiffusionImageModeName(ofxGgmlStableDiffusionImageMode mode) {
	switch (mode) {
	case ofxGgmlStableDiffusionImageMode::ImageToImage: return "ImageToImage";
	case ofxGgmlStableDiffusionImageMode::Inpainting: return "Inpainting";
	case ofxGgmlStableDiffusionImageMode::TextToImage:
	default:
		return "TextToImage";
	}
}

inline bool ofxGgmlStableDiffusionImageModeUsesInputImage(ofxGgmlStableDiffusionImageMode mode) {
	return mode != ofxGgmlStableDiffusionImageMode::TextToImage;
}

inline ofxGgmlStableDiffusionTask ofxGgmlStableDiffusionTaskForImageMode(ofxGgmlStableDiffusionImageMode mode) {
	switch (mode) {
	case ofxGgmlStableDiffusionImageMode::Inpainting:
		return ofxGgmlStableDiffusionTask::Inpainting;
	case ofxGgmlStableDiffusionImageMode::ImageToImage:
		return ofxGgmlStableDiffusionTask::ImageToImage;
	case ofxGgmlStableDiffusionImageMode::TextToImage:
	default:
		return ofxGgmlStableDiffusionTask::TextToImage;
	}
}

inline float ofxGgmlStableDiffusionDefaultStrengthForImageMode(ofxGgmlStableDiffusionImageMode mode) {
	switch (mode) {
	case ofxGgmlStableDiffusionImageMode::Inpainting: return 0.75f;
	case ofxGgmlStableDiffusionImageMode::ImageToImage: return 0.50f;
	case ofxGgmlStableDiffusionImageMode::TextToImage:
	default:
		return 0.50f;
	}
}

inline float ofxGgmlStableDiffusionDefaultCfgScaleForImageMode(ofxGgmlStableDiffusionImageMode mode) {
	switch (mode) {
	case ofxGgmlStableDiffusionImageMode::Inpainting: return 7.5f;
	case ofxGgmlStableDiffusionImageMode::ImageToImage: return 7.0f;
	case ofxGgmlStableDiffusionImageMode::TextToImage:
	default:
		return 7.0f;
	}
}
