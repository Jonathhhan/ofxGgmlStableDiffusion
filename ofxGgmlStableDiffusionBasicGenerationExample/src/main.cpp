#include "ofMain.h"
#include "ofApp.h"

int main() {
	ofGLFWWindowSettings settings;
	settings.setSize(1280, 720);
	settings.visible = true;
	settings.windowMode = OF_WINDOW;

	auto window = ofCreateWindow(settings);
	auto app = std::make_shared<ofApp>();
	ofRunApp(window, app);
	ofRunMainLoop();
}
