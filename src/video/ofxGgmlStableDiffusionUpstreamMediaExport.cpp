#include "ofxGgmlStableDiffusionUpstreamMediaExport.h"

#include "ofLog.h"

#include <vector>

#if !defined(_DEBUG) && defined(__has_include)
#if __has_include("../../libs/stable-diffusion/source/examples/common/media_io.cpp")
#define OFXGGML_STABLE_DIFFUSION_HAS_UPSTREAM_MEDIA_IO 1
#endif
#endif

#if OFXGGML_STABLE_DIFFUSION_HAS_UPSTREAM_MEDIA_IO
#if defined(_MSC_VER)
#pragma comment(lib, "webm.lib")
#pragma comment(lib, "libwebp.lib")
#pragma comment(lib, "libwebpmux.lib")
#pragma comment(lib, "libsharpyuv.lib")
#endif
#ifndef SD_USE_WEBP
#define SD_USE_WEBP
#endif
#ifndef SD_USE_WEBM
#define SD_USE_WEBM
#endif
#include "../../libs/stable-diffusion/source/examples/common/log.cpp"
#include "../../libs/stable-diffusion/source/examples/common/media_io.cpp"
#endif

namespace ofxGgmlStableDiffusionUpstreamMediaExport {

bool isAvailable() {
#if OFXGGML_STABLE_DIFFUSION_HAS_UPSTREAM_MEDIA_IO
	return true;
#else
	return false;
#endif
}

bool saveVideo(const std::string& path, const ofxGgmlStableDiffusionVideoClip& clip, int quality) {
#if OFXGGML_STABLE_DIFFUSION_HAS_UPSTREAM_MEDIA_IO
	if (clip.frames.empty()) {
		ofLogWarning("ofxGgmlStableDiffusion") << "Video export skipped because there are no frames.";
		return false;
	}
	if (clip.fps <= 0) {
		ofLogWarning("ofxGgmlStableDiffusion") << "Video export skipped because FPS is not positive.";
		return false;
	}

	std::vector<sd_image_t> images;
	images.reserve(clip.frames.size());
	for (std::size_t i = 0; i < clip.frames.size(); ++i) {
		const auto& frame = clip.frames[i];
		if (!frame.isAllocated()) {
			ofLogWarning("ofxGgmlStableDiffusion") << "Video export skipped because frame " << i << " is not allocated.";
			return false;
		}
		images.push_back({
			static_cast<uint32_t>(frame.width()),
			static_cast<uint32_t>(frame.height()),
			static_cast<uint32_t>(frame.channels()),
			const_cast<uint8_t*>(frame.pixels.getData())
		});
	}

	const int result = create_video_from_sd_images(
		path.c_str(),
		images.data(),
		static_cast<int>(images.size()),
		clip.fps,
		quality,
		nullptr);
	return result == 0;
#else
	(void)path;
	(void)clip;
	(void)quality;
	return false;
#endif
}

} // namespace ofxGgmlStableDiffusionUpstreamMediaExport
