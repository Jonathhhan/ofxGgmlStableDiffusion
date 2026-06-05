#include "video/ofxGgmlStableDiffusionNativeVideoExport.h"
#include "video/ofxGgmlStableDiffusionUpstreamMediaExport.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace ofxGgmlStableDiffusionUpstreamMediaExport {

bool isAvailable() {
	return false;
}

bool saveVideo(const std::string&, const ofxGgmlStableDiffusionVideoClip&, int) {
	return false;
}

} // namespace ofxGgmlStableDiffusionUpstreamMediaExport

namespace {

bool expect(bool condition, const std::string& message) {
	if (condition) {
		return true;
	}

	std::cerr << "FAIL: " << message << std::endl;
	return false;
}

ofxGgmlStableDiffusionImageFrame makeFrame(int width, int height, int value) {
	std::vector<unsigned char> bytes(static_cast<std::size_t>(width * height * 3), static_cast<unsigned char>(value));
	ofxGgmlStableDiffusionImageFrame frame;
	frame.pixels.setFromPixels(bytes.data(), width, height, OF_IMAGE_COLOR);
	return frame;
}

bool fileStartsWith(const std::filesystem::path& path, const std::string& expected) {
	std::ifstream input(path, std::ios::binary);
	if (!input.is_open()) {
		return false;
	}

	std::string actual(expected.size(), '\0');
	input.read(actual.data(), static_cast<std::streamsize>(actual.size()));
	return actual == expected;
}

} // namespace

int main() {
	bool ok = true;

	ofxGgmlStableDiffusionVideoClip emptyClip;
	ok &= expect(
		!ofxGgmlStableDiffusionNativeVideoExport::saveWebm("empty.avi", emptyClip),
		"empty clips are rejected");

	ofxGgmlStableDiffusionVideoClip validClip;
	validClip.fps = 12;
	validClip.frames.push_back(makeFrame(4, 4, 64));
	validClip.frames.push_back(makeFrame(4, 4, 128));

	ok &= expect(
		!ofxGgmlStableDiffusionNativeVideoExport::saveWebm("unsupported.mov", validClip),
		"unsupported extensions are rejected");

	ofxGgmlStableDiffusionVideoClip mismatchedClip = validClip;
	mismatchedClip.frames.push_back(makeFrame(2, 4, 255));
	ok &= expect(
		!ofxGgmlStableDiffusionNativeVideoExport::saveWebm("mismatched.avi", mismatchedClip),
		"mismatched frame sizes are rejected");

	const auto outputPath = std::filesystem::temp_directory_path() / "ofxggml-stable-diffusion-video-export-test.avi";
	std::filesystem::remove(outputPath);
	ok &= expect(
		ofxGgmlStableDiffusionNativeVideoExport::saveWebm(outputPath.string(), validClip, 85),
		"avi fallback saves valid clips");
	ok &= expect(std::filesystem::is_regular_file(outputPath), "avi output file exists");
	ok &= expect(fileStartsWith(outputPath, "RIFF"), "avi output starts with RIFF");

	std::ifstream input(outputPath, std::ios::binary);
	if (input.is_open()) {
		input.seekg(8);
		std::string signature(4, '\0');
		input.read(signature.data(), static_cast<std::streamsize>(signature.size()));
		ok &= expect(signature == "AVI ", "avi output has AVI signature");
		input.close();
	}
	else {
		ok &= expect(false, "avi output can be reopened");
	}

	std::filesystem::remove(outputPath);
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
