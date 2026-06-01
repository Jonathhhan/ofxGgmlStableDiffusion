#include "ofMain.h"
#include "ofApp.h"

#include <fstream>

namespace {

void writeContextSmokeStatus(const std::string& message) {
	const std::string path =
		ofGetEnv("OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_STATUS");
	if (path.empty()) {
		return;
	}
	std::ofstream output(path, std::ios::app);
	output << message << '\n';
}

} // namespace

int main() {
	writeContextSmokeStatus("main:start");
	ofGLFWWindowSettings settings;
	settings.setSize(1280, 720);
	settings.visible =
		ofGetEnv("OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_HIDE_WINDOW") != "1";
	settings.windowMode = OF_WINDOW;

	auto window = ofCreateWindow(settings);
	writeContextSmokeStatus("main:window-created");
	ofRunApp(window, std::make_shared<ofApp>());
	writeContextSmokeStatus("main:app-running");
	const int exitCode = ofRunMainLoop();
	writeContextSmokeStatus("main:loop-ended");
	return exitCode;
}
