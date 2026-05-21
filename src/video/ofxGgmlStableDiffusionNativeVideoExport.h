#pragma once

#include "../core/ofxGgmlStableDiffusionTypes.h"

namespace ofxGgmlStableDiffusionNativeVideoExport {

bool isWebmExportAvailable();
bool saveWebm(const std::string& path, const ofxGgmlStableDiffusionVideoClip& clip, int quality = 90);

} // namespace ofxGgmlStableDiffusionNativeVideoExport
