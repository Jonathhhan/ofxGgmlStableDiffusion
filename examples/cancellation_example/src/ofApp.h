#pragma once

#include "ofMain.h"
#include "ofxGgmlStableDiffusion.h"
#include <atomic>

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);

private:
    void startGeneration();
    void cancelGeneration();

    ofxGgmlStableDiffusion sd;
    ofImage resultImage;
    bool generating;
    bool wasCancelled;
    std::atomic<float> progress{0.0f};
    std::string statusMessage;
};
