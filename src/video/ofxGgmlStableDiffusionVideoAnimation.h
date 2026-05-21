#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

enum class ofxGgmlStableDiffusionInterpolationMode {
	Linear,
	Smooth,
	EaseIn,
	EaseOut,
	EaseInOut
};

struct ofxGgmlStableDiffusionPromptKeyframe {
	int frameNumber = 0;
	std::string prompt;
	float weight = 1.0f;

	ofxGgmlStableDiffusionPromptKeyframe() = default;
	ofxGgmlStableDiffusionPromptKeyframe(int frame, const std::string& value, float keyframeWeight = 1.0f)
		: frameNumber(frame), prompt(value), weight(keyframeWeight) {}
};

struct ofxGgmlStableDiffusionKeyframe {
	int frameNumber = 0;
	float cfgScale = -1.0f;
	float strength = -1.0f;
	int64_t seed = -1;
	std::string prompt;
	std::string negativePrompt;

	ofxGgmlStableDiffusionKeyframe() = default;
	explicit ofxGgmlStableDiffusionKeyframe(int frame) : frameNumber(frame) {}
};

struct ofxGgmlStableDiffusionVideoAnimationSettings {
	bool enablePromptInterpolation = false;
	std::vector<ofxGgmlStableDiffusionPromptKeyframe> promptKeyframes;
	ofxGgmlStableDiffusionInterpolationMode promptInterpolationMode = ofxGgmlStableDiffusionInterpolationMode::Smooth;

	bool enableParameterAnimation = false;
	std::vector<ofxGgmlStableDiffusionKeyframe> parameterKeyframes;
	ofxGgmlStableDiffusionInterpolationMode parameterInterpolationMode = ofxGgmlStableDiffusionInterpolationMode::Smooth;

	bool useSeedSequence = false;
	int64_t seedIncrement = 1;
};

inline float ofxGgmlStableDiffusionClampUnit(float value) {
	return std::max(0.0f, std::min(1.0f, value));
}

inline float ofxGgmlStableDiffusionApplyInterpolation(float t, ofxGgmlStableDiffusionInterpolationMode mode) {
	const float clamped = ofxGgmlStableDiffusionClampUnit(t);
	constexpr float pi = 3.14159265358979323846f;
	switch (mode) {
	case ofxGgmlStableDiffusionInterpolationMode::Linear:
		return clamped;
	case ofxGgmlStableDiffusionInterpolationMode::Smooth:
		return (1.0f - std::cos(clamped * pi)) * 0.5f;
	case ofxGgmlStableDiffusionInterpolationMode::EaseIn:
		return clamped * clamped;
	case ofxGgmlStableDiffusionInterpolationMode::EaseOut:
		return clamped * (2.0f - clamped);
	case ofxGgmlStableDiffusionInterpolationMode::EaseInOut:
		if (clamped < 0.5f) {
			return 4.0f * clamped * clamped * clamped;
		}
		return 0.5f * std::pow((2.0f * clamped) - 2.0f, 3.0f) + 1.0f;
	default:
		return clamped;
	}
}

inline float ofxGgmlStableDiffusionLerp(float a, float b, float t) {
	return a + ((b - a) * t);
}

template<typename T>
inline bool ofxGgmlStableDiffusionFindSurroundingKeyframes(
	const std::vector<T>& keyframes,
	int frameNumber,
	const T*& prevKeyframe,
	const T*& nextKeyframe) {
	prevKeyframe = nullptr;
	nextKeyframe = nullptr;

	for (const auto& keyframe : keyframes) {
		if (keyframe.frameNumber <= frameNumber &&
			(prevKeyframe == nullptr || keyframe.frameNumber > prevKeyframe->frameNumber)) {
			prevKeyframe = &keyframe;
		}
		if (keyframe.frameNumber >= frameNumber &&
			(nextKeyframe == nullptr || keyframe.frameNumber < nextKeyframe->frameNumber)) {
			nextKeyframe = &keyframe;
		}
	}

	if (prevKeyframe == nullptr && !keyframes.empty()) {
		prevKeyframe = &*std::min_element(
			keyframes.begin(),
			keyframes.end(),
			[](const T& left, const T& right) { return left.frameNumber < right.frameNumber; });
	}
	if (nextKeyframe == nullptr && !keyframes.empty()) {
		nextKeyframe = &*std::max_element(
			keyframes.begin(),
			keyframes.end(),
			[](const T& left, const T& right) { return left.frameNumber < right.frameNumber; });
	}

	return prevKeyframe != nullptr && nextKeyframe != nullptr;
}

inline float ofxGgmlStableDiffusionInterpolateParameter(
	float prevValue,
	float nextValue,
	int prevFrame,
	int nextFrame,
	int currentFrame,
	ofxGgmlStableDiffusionInterpolationMode mode) {
	if (prevFrame == nextFrame) {
		return prevValue;
	}

	const float t = static_cast<float>(currentFrame - prevFrame) /
		static_cast<float>(nextFrame - prevFrame);
	return ofxGgmlStableDiffusionLerp(
		prevValue,
		nextValue,
		ofxGgmlStableDiffusionApplyInterpolation(t, mode));
}

inline std::string ofxGgmlStableDiffusionInterpolatePrompts(
	const std::vector<ofxGgmlStableDiffusionPromptKeyframe>& keyframes,
	int currentFrame,
	ofxGgmlStableDiffusionInterpolationMode mode) {
	if (keyframes.empty()) {
		return "";
	}

	const ofxGgmlStableDiffusionPromptKeyframe* prev = nullptr;
	const ofxGgmlStableDiffusionPromptKeyframe* next = nullptr;
	if (!ofxGgmlStableDiffusionFindSurroundingKeyframes(keyframes, currentFrame, prev, next)) {
		return "";
	}

	if (prev == next || prev->frameNumber == currentFrame) {
		return prev->prompt;
	}
	if (next->frameNumber == currentFrame) {
		return next->prompt;
	}
	if (prev->prompt == next->prompt) {
		return prev->prompt;
	}

	const float rawT = static_cast<float>(currentFrame - prev->frameNumber) /
		static_cast<float>(next->frameNumber - prev->frameNumber);
	const float t = ofxGgmlStableDiffusionApplyInterpolation(rawT, mode);
	const float weight1 = (1.0f - t) * prev->weight;
	const float weight2 = t * next->weight;

	return "(" + prev->prompt + ":" + std::to_string(weight1) + ") AND (" +
		next->prompt + ":" + std::to_string(weight2) + ")";
}

inline float ofxGgmlStableDiffusionGetInterpolatedParameter(
	const std::vector<ofxGgmlStableDiffusionKeyframe>& keyframes,
	int currentFrame,
	ofxGgmlStableDiffusionInterpolationMode mode,
	const std::function<float(const ofxGgmlStableDiffusionKeyframe&)>& getter) {
	if (keyframes.empty()) {
		return -1.0f;
	}

	const ofxGgmlStableDiffusionKeyframe* prev = nullptr;
	const ofxGgmlStableDiffusionKeyframe* next = nullptr;
	if (!ofxGgmlStableDiffusionFindSurroundingKeyframes(keyframes, currentFrame, prev, next)) {
		return -1.0f;
	}

	const float prevValue = getter(*prev);
	const float nextValue = getter(*next);
	if (prevValue < 0.0f && nextValue < 0.0f) {
		return -1.0f;
	}
	if (prevValue < 0.0f) {
		return nextValue;
	}
	if (nextValue < 0.0f) {
		return prevValue;
	}
	return ofxGgmlStableDiffusionInterpolateParameter(
		prevValue,
		nextValue,
		prev->frameNumber,
		next->frameNumber,
		currentFrame,
		mode);
}

template<typename Getter>
inline std::string ofxGgmlStableDiffusionGetKeyframedString(
	const std::vector<ofxGgmlStableDiffusionKeyframe>& keyframes,
	int currentFrame,
	Getter getter) {
	const ofxGgmlStableDiffusionKeyframe* bestPrev = nullptr;
	const ofxGgmlStableDiffusionKeyframe* bestNext = nullptr;
	for (const auto& keyframe : keyframes) {
		const std::string value = getter(keyframe);
		if (value.empty()) {
			continue;
		}
		if (keyframe.frameNumber <= currentFrame &&
			(bestPrev == nullptr || keyframe.frameNumber > bestPrev->frameNumber)) {
			bestPrev = &keyframe;
		}
		if (keyframe.frameNumber >= currentFrame &&
			(bestNext == nullptr || keyframe.frameNumber < bestNext->frameNumber)) {
			bestNext = &keyframe;
		}
	}

	if (bestPrev != nullptr) {
		return getter(*bestPrev);
	}
	if (bestNext != nullptr) {
		return getter(*bestNext);
	}
	return "";
}

inline int64_t ofxGgmlStableDiffusionGetKeyframedSeed(
	const std::vector<ofxGgmlStableDiffusionKeyframe>& keyframes,
	int currentFrame) {
	const ofxGgmlStableDiffusionKeyframe* bestPrev = nullptr;
	const ofxGgmlStableDiffusionKeyframe* bestNext = nullptr;
	for (const auto& keyframe : keyframes) {
		if (keyframe.seed < 0) {
			continue;
		}
		if (keyframe.frameNumber <= currentFrame &&
			(bestPrev == nullptr || keyframe.frameNumber > bestPrev->frameNumber)) {
			bestPrev = &keyframe;
		}
		if (keyframe.frameNumber >= currentFrame &&
			(bestNext == nullptr || keyframe.frameNumber < bestNext->frameNumber)) {
			bestNext = &keyframe;
		}
	}

	if (bestPrev != nullptr) {
		return bestPrev->seed;
	}
	if (bestNext != nullptr) {
		return bestNext->seed;
	}
	return -1;
}

inline bool ofxGgmlStableDiffusionValidateAnimationKeyframes(
	const ofxGgmlStableDiffusionVideoAnimationSettings& settings,
	int frameCount,
	std::string& errorMessage) {
	if (settings.enablePromptInterpolation) {
		if (settings.promptKeyframes.empty()) {
			errorMessage = "Prompt interpolation enabled but no prompt keyframes provided";
			return false;
		}
		for (std::size_t i = 0; i < settings.promptKeyframes.size(); ++i) {
			const auto& keyframe = settings.promptKeyframes[i];
			if (keyframe.frameNumber < 0) {
				errorMessage = "Prompt keyframe " + std::to_string(i) +
					" has invalid frame number: " + std::to_string(keyframe.frameNumber);
				return false;
			}
			if (keyframe.frameNumber >= frameCount) {
				errorMessage = "Prompt keyframe " + std::to_string(i) +
					" frame number (" + std::to_string(keyframe.frameNumber) +
					") exceeds total frame count (" + std::to_string(frameCount) + ")";
				return false;
			}
			if (keyframe.weight < 0.0f || keyframe.weight > 2.0f) {
				errorMessage = "Prompt keyframe " + std::to_string(i) +
					" has invalid weight: " + std::to_string(keyframe.weight) +
					" (must be between 0.0 and 2.0)";
				return false;
			}
			if (keyframe.prompt.empty()) {
				errorMessage = "Prompt keyframe " + std::to_string(i) + " has empty prompt";
				return false;
			}
		}
	}

	if (settings.enableParameterAnimation) {
		if (settings.parameterKeyframes.empty()) {
			errorMessage = "Parameter animation enabled but no parameter keyframes provided";
			return false;
		}
		for (std::size_t i = 0; i < settings.parameterKeyframes.size(); ++i) {
			const auto& keyframe = settings.parameterKeyframes[i];
			if (keyframe.frameNumber < 0) {
				errorMessage = "Parameter keyframe " + std::to_string(i) +
					" has invalid frame number: " + std::to_string(keyframe.frameNumber);
				return false;
			}
			if (keyframe.frameNumber >= frameCount) {
				errorMessage = "Parameter keyframe " + std::to_string(i) +
					" frame number (" + std::to_string(keyframe.frameNumber) +
					") exceeds total frame count (" + std::to_string(frameCount) + ")";
				return false;
			}
			if (keyframe.cfgScale >= 0.0f && (keyframe.cfgScale < 0.0f || keyframe.cfgScale > 50.0f)) {
				errorMessage = "Parameter keyframe " + std::to_string(i) +
					" has invalid cfgScale: " + std::to_string(keyframe.cfgScale);
				return false;
			}
			if (keyframe.strength >= 0.0f && (keyframe.strength < 0.0f || keyframe.strength > 1.0f)) {
				errorMessage = "Parameter keyframe " + std::to_string(i) +
					" has invalid strength: " + std::to_string(keyframe.strength);
				return false;
			}
		}
	}

	return true;
}

