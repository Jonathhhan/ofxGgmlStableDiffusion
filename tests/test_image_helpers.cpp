#include "core/ofxGgmlStableDiffusionImageHelpers.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const std::string & message) {
	if (condition) {
		return true;
	}

	std::cerr << "FAIL: " << message << std::endl;
	return false;
}

bool expectNear(float actual, float expected, float epsilon, const std::string & label) {
	if (std::fabs(actual - expected) <= epsilon) {
		return true;
	}

	std::cerr << "FAIL: " << label << " expected " << expected << " but got " << actual << std::endl;
	return false;
}

} // namespace

int main() {
	bool ok = true;

	ok &= expect(std::string(ofxGgmlStableDiffusionImageModeName(ofxGgmlStableDiffusionImageMode::TextToImage)) == "TextToImage", "text label");
	ok &= expect(std::string(ofxGgmlStableDiffusionImageModeName(ofxGgmlStableDiffusionImageMode::ImageToImage)) == "ImageToImage", "img2img label");
	ok &= expect(std::string(ofxGgmlStableDiffusionImageModeName(ofxGgmlStableDiffusionImageMode::Inpainting)) == "Inpainting", "inpainting label");

	ok &= expect(!ofxGgmlStableDiffusionImageModeUsesInputImage(ofxGgmlStableDiffusionImageMode::TextToImage), "text does not need input image");
	ok &= expect(ofxGgmlStableDiffusionImageModeUsesInputImage(ofxGgmlStableDiffusionImageMode::ImageToImage), "img2img needs input image");
	ok &= expect(ofxGgmlStableDiffusionImageModeUsesInputImage(ofxGgmlStableDiffusionImageMode::Inpainting), "inpainting needs input image");
	ok &= expect(ofxGgmlStableDiffusionTaskForImageMode(ofxGgmlStableDiffusionImageMode::TextToImage) == ofxGgmlStableDiffusionTask::TextToImage, "text task");
	ok &= expect(ofxGgmlStableDiffusionTaskForImageMode(ofxGgmlStableDiffusionImageMode::ImageToImage) == ofxGgmlStableDiffusionTask::ImageToImage, "img2img task");
	ok &= expect(ofxGgmlStableDiffusionTaskForImageMode(ofxGgmlStableDiffusionImageMode::Inpainting) == ofxGgmlStableDiffusionTask::Inpainting, "inpainting task");

	ok &= expectNear(ofxGgmlStableDiffusionDefaultStrengthForImageMode(ofxGgmlStableDiffusionImageMode::ImageToImage), 0.50f, 0.0001f, "img2img strength");
	ok &= expectNear(ofxGgmlStableDiffusionDefaultCfgScaleForImageMode(ofxGgmlStableDiffusionImageMode::Inpainting), 7.5f, 0.0001f, "inpainting cfg");

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
