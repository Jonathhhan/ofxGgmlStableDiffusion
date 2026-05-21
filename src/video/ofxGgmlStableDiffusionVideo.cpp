#include "../core/ofxGgmlStableDiffusionTypes.h"
#include "../core/ofxGgmlStableDiffusionImageHelpers.h"
#include "../core/ofxGgmlStableDiffusionValidationHelpers.h"
#include "ofxGgmlStableDiffusionVideoHelpers.h"
#include "ofxGgmlStableDiffusionNativeVideoExport.h"

#include <functional>

namespace {

void appendFrameCopy(
	std::vector<ofxGgmlStableDiffusionImageFrame> & destination,
	const ofxGgmlStableDiffusionImageFrame & source,
	int sequenceIndex) {
	ofxGgmlStableDiffusionImageFrame frame = source;
	frame.index = sequenceIndex;
	destination.push_back(std::move(frame));
}

} // namespace

bool ofxGgmlStableDiffusionVideoClip::empty() const {
	return frames.empty();
}

std::size_t ofxGgmlStableDiffusionVideoClip::size() const {
	return frames.size();
}

float ofxGgmlStableDiffusionVideoClip::durationSeconds() const {
	return ofxGgmlStableDiffusionVideoDurationSeconds(frames.size(), fps);
}

int ofxGgmlStableDiffusionVideoClip::frameIndexForTime(float seconds) const {
	return ofxGgmlStableDiffusionVideoFrameIndexForTime(frames.size(), fps, seconds);
}

const ofxGgmlStableDiffusionImageFrame * ofxGgmlStableDiffusionVideoClip::frameForTime(float seconds) const {
	const int index = frameIndexForTime(seconds);
	if (index < 0 || index >= static_cast<int>(frames.size())) {
		return nullptr;
	}
	return &frames[static_cast<std::size_t>(index)];
}

std::vector<int64_t> ofxGgmlStableDiffusionVideoClip::seeds() const {
	std::vector<int64_t> output;
	output.reserve(frames.size());
	for (const auto& frame : frames) {
		output.push_back(frame.seed);
	}
	return output;
}

bool ofxGgmlStableDiffusionVideoClip::saveFrameSequence(
	const std::string & directory,
	const std::string & prefix) const {
	if (frames.empty()) {
		return false;
	}
	if (directory.empty() || ofxSdPathHasParentTraversal(directory) || !ofxSdIsSafeChildPathComponent(prefix)) {
		return false;
	}

	ofDirectory::createDirectory(directory, true, true);
	for (std::size_t i = 0; i < frames.size(); ++i) {
		const auto & frame = frames[i];
		if (!frame.isAllocated()) {
			return false;
		}
		const std::string filename = prefix + "_" + ofToString(static_cast<int>(i), 4, '0') + ".png";
		const std::string path = ofFilePath::join(directory, filename);
		if (!ofSaveImage(frame.pixels, path, OF_IMAGE_QUALITY_BEST)) {
			return false;
		}
	}

	return true;
}

bool ofxGgmlStableDiffusionVideoClip::saveMetadataJson(const std::string & path) const {
	if (frames.empty()) {
		return false;
	}
	if (path.empty() || ofxSdPathHasParentTraversal(path)) {
		return false;
	}

	ofJson root;
	root["fps"] = fps;
	root["frame_count"] = static_cast<int>(frames.size());
	root["source_frame_count"] = sourceFrameCount;
	root["mode"] = ofxGgmlStableDiffusionVideoModeName(mode);
	root["duration_seconds"] = durationSeconds();

	ofJson frameArray = ofJson::array();
	for (const auto& frame : frames) {
		ofJson frameJson;
		frameJson["index"] = frame.index;
		frameJson["source_index"] = frame.sourceIndex;
		frameJson["seed"] = frame.seed;
		frameJson["width"] = frame.width();
		frameJson["height"] = frame.height();
		frameJson["channels"] = frame.channels();
		if (!frame.generation.prompt.empty()) {
			frameJson["prompt"] = frame.generation.prompt;
		}
		if (!frame.generation.negativePrompt.empty()) {
			frameJson["negative_prompt"] = frame.generation.negativePrompt;
		}
		if (frame.generation.cfgScale >= 0.0f) {
			frameJson["cfg_scale"] = frame.generation.cfgScale;
		}
		if (frame.generation.strength >= 0.0f) {
			frameJson["strength"] = frame.generation.strength;
		}
		frameArray.push_back(std::move(frameJson));
	}

	root["frames"] = std::move(frameArray);
	return ofSavePrettyJson(path, root);
}

bool ofxGgmlStableDiffusionVideoClip::saveFrameSequenceWithMetadata(
	const std::string & directory,
	const std::string & prefix,
	const std::string & metadataFilename) const {
	if (!ofxSdIsSafeChildPathComponent(metadataFilename)) {
		return false;
	}
	if (!saveFrameSequence(directory, prefix)) {
		return false;
	}
	return saveMetadataJson(ofFilePath::join(directory, metadataFilename));
}

bool ofxGgmlStableDiffusionVideoClip::saveWebm(const std::string & path, int quality) const {
	return ofxGgmlStableDiffusionNativeVideoExport::saveWebm(path, *this, quality);
}

const char * ofxGgmlStableDiffusionTaskLabel(ofxGgmlStableDiffusionTask task) {
	switch (task) {
	case ofxGgmlStableDiffusionTask::LoadModel: return "LoadModel";
	case ofxGgmlStableDiffusionTask::TextToImage: return "TextToImage";
	case ofxGgmlStableDiffusionTask::ImageToImage: return "ImageToImage";
	case ofxGgmlStableDiffusionTask::Inpainting: return "Inpainting";
	case ofxGgmlStableDiffusionTask::ImageToVideo: return "ImageToVideo";
	case ofxGgmlStableDiffusionTask::Upscale: return "Upscale";
	case ofxGgmlStableDiffusionTask::None:
	default:
		return "None";
	}
}

const char * ofxGgmlStableDiffusionImageModeLabel(ofxGgmlStableDiffusionImageMode mode) {
	return ofxGgmlStableDiffusionImageModeName(mode);
}

const char * ofxGgmlStableDiffusionImageSelectionModeLabel(
	ofxGgmlStableDiffusionImageSelectionMode mode) {
	return ofxGgmlStableDiffusionImageSelectionModeName(mode);
}

const char * ofxGgmlStableDiffusionVideoModeLabel(ofxGgmlStableDiffusionVideoMode mode) {
	return ofxGgmlStableDiffusionVideoModeName(mode);
}

const char * ofxGgmlStableDiffusionErrorCodeLabel(ofxGgmlStableDiffusionErrorCode code) {
	switch (code) {
	case ofxGgmlStableDiffusionErrorCode::ModelNotFound: return "ModelNotFound";
	case ofxGgmlStableDiffusionErrorCode::ModelCorrupted: return "ModelCorrupted";
	case ofxGgmlStableDiffusionErrorCode::ModelLoadFailed: return "ModelLoadFailed";
	case ofxGgmlStableDiffusionErrorCode::OutOfMemory: return "OutOfMemory";
	case ofxGgmlStableDiffusionErrorCode::InvalidDimensions: return "InvalidDimensions";
	case ofxGgmlStableDiffusionErrorCode::InvalidBatchCount: return "InvalidBatchCount";
	case ofxGgmlStableDiffusionErrorCode::InvalidFrameCount: return "InvalidFrameCount";
	case ofxGgmlStableDiffusionErrorCode::InvalidParameter: return "InvalidParameter";
	case ofxGgmlStableDiffusionErrorCode::MissingInputImage: return "MissingInputImage";
	case ofxGgmlStableDiffusionErrorCode::GenerationFailed: return "GenerationFailed";
	case ofxGgmlStableDiffusionErrorCode::ThreadBusy: return "ThreadBusy";
	case ofxGgmlStableDiffusionErrorCode::UpscaleFailed: return "UpscaleFailed";
	case ofxGgmlStableDiffusionErrorCode::Cancelled: return "Cancelled";
	case ofxGgmlStableDiffusionErrorCode::Unknown: return "Unknown";
	case ofxGgmlStableDiffusionErrorCode::None:
	default:
		return "None";
	}
}

std::string ofxGgmlStableDiffusionErrorCodeSuggestion(ofxGgmlStableDiffusionErrorCode code) {
	switch (code) {
	case ofxGgmlStableDiffusionErrorCode::ModelNotFound:
		return "Verify the model file path exists and is accessible";
	case ofxGgmlStableDiffusionErrorCode::ModelCorrupted:
		return "Re-download the model file or try a different model";
	case ofxGgmlStableDiffusionErrorCode::ModelLoadFailed:
		return "Check model format compatibility and ensure sufficient RAM/VRAM";
	case ofxGgmlStableDiffusionErrorCode::OutOfMemory:
		return "Reduce batch count, image dimensions, or enable VAE tiling";
	case ofxGgmlStableDiffusionErrorCode::InvalidDimensions:
		return "Use positive multiples of 64 (e.g., 512, 768, 1024) for width and height";
	case ofxGgmlStableDiffusionErrorCode::InvalidBatchCount:
		return "Set batch count between 1 and 16";
	case ofxGgmlStableDiffusionErrorCode::InvalidFrameCount:
		return "Set frame count between 1 and 100";
	case ofxGgmlStableDiffusionErrorCode::InvalidParameter:
		return "Verify numeric parameters are within supported ranges";
	case ofxGgmlStableDiffusionErrorCode::MissingInputImage:
		return "Load an input image using loadImage() before calling this operation";
	case ofxGgmlStableDiffusionErrorCode::GenerationFailed:
		return "Check model compatibility and system resources";
	case ofxGgmlStableDiffusionErrorCode::ThreadBusy:
		return "Wait for current generation to complete or use isGenerating() to check status";
	case ofxGgmlStableDiffusionErrorCode::UpscaleFailed:
		return "Verify upscaler model path and compatibility";
	case ofxGgmlStableDiffusionErrorCode::Cancelled:
		return "Operation was cancelled by user request";
	case ofxGgmlStableDiffusionErrorCode::Unknown:
		return "Check logs for more details";
	case ofxGgmlStableDiffusionErrorCode::None:
	default:
		return "";
	}
}

std::vector<ofxGgmlStableDiffusionImageFrame> ofxGgmlStableDiffusionBuildVideoFrames(
	const std::vector<ofxGgmlStableDiffusionImageFrame> & sourceFrames,
	ofxGgmlStableDiffusionVideoMode mode) {
	std::vector<ofxGgmlStableDiffusionImageFrame> frames;
	if (sourceFrames.empty()) {
		return frames;
	}

	const std::vector<int> sequence =
		ofxGgmlStableDiffusionBuildVideoFrameSequence(static_cast<int>(sourceFrames.size()), mode);
	frames.reserve(sequence.size());
	for (std::size_t i = 0; i < sequence.size(); ++i) {
		const int sourceIndex = sequence[i];
		appendFrameCopy(frames, sourceFrames[static_cast<std::size_t>(sourceIndex)], static_cast<int>(i));
	}

	return frames;
}

int64_t ofxGgmlStableDiffusionHashStringToSeed(const std::string& text) {
	if (text.empty()) {
		return -1;
	}
	std::hash<std::string> hasher;
	size_t hash = hasher(text);
	// Convert to int64_t, ensuring we stay in valid seed range
	return static_cast<int64_t>(hash & 0x7FFFFFFFFFFFFFFF);
}
