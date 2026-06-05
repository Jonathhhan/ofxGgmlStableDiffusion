#pragma once

#include "../core/ofxGgmlStableDiffusionTypes.h"

#include <string>

namespace ofxGgmlStableDiffusionUpstreamMediaExport {

bool isAvailable();
bool saveVideo(const std::string& path, const ofxGgmlStableDiffusionVideoClip& clip, int quality = 90);

} // namespace ofxGgmlStableDiffusionUpstreamMediaExport
