#pragma once

enum class ofxGgmlStableDiffusionTask {
	None = 0,
	LoadModel,
	TextToImage,
	ImageToImage,
	Inpainting,
	ImageToVideo,
	Upscale
};

enum class ofxGgmlStableDiffusionImageMode {
	TextToImage = 0,
	ImageToImage,
	Inpainting
};

enum class ofxGgmlStableDiffusionVideoMode {
	Standard = 0,
	Loop,
	PingPong,
	Boomerang
};

enum class ofxGgmlStableDiffusionErrorCode {
	None = 0,
	ModelNotFound,
	ModelCorrupted,
	ModelLoadFailed,
	OutOfMemory,
	InvalidDimensions,
	InvalidBatchCount,
	InvalidFrameCount,
	InvalidParameter,
	MissingInputImage,
	GenerationFailed,
	ThreadBusy,
	UpscaleFailed,
	Cancelled,
	Unknown
};
