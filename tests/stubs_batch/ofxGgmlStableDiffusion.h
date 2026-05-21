#pragma once

#include "core/ofxGgmlStableDiffusionTypes.h"

#include <deque>
#include <vector>

class ofxGgmlStableDiffusion {
public:
	void setQueuedResults(const std::vector<ofxGgmlStableDiffusionResult>& results) {
		queuedResults_.assign(results.begin(), results.end());
	}

	void generate(const ofxGgmlStableDiffusionImageRequest& request) {
		imageRequests_.push_back(request);
		if (!queuedResults_.empty()) {
			lastResult_ = queuedResults_.front();
			queuedResults_.pop_front();
		} else {
			lastResult_ = ofxGgmlStableDiffusionResult();
			lastResult_.success = true;
		}
	}

	bool isGenerating() const {
		return false;
	}

	ofxGgmlStableDiffusionResult getLastResult() const {
		return lastResult_;
	}

	bool requestCancellation() {
		cancelRequested_ = true;
		return true;
	}

	bool wasCancellationRequested() const {
		return cancelRequested_;
	}

	const std::vector<ofxGgmlStableDiffusionImageRequest>& getImageRequests() const {
		return imageRequests_;
	}

private:
	std::deque<ofxGgmlStableDiffusionResult> queuedResults_;
	std::vector<ofxGgmlStableDiffusionImageRequest> imageRequests_;
	ofxGgmlStableDiffusionResult lastResult_;
	bool cancelRequested_ = false;
};
