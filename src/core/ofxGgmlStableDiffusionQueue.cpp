#include "ofxGgmlStableDiffusionQueue.h"
#include <algorithm>

//--------------------------------------------------------------
ofxGgmlStableDiffusionQueue::ofxGgmlStableDiffusionQueue() {}

//--------------------------------------------------------------
ofxGgmlStableDiffusionQueue::~ofxGgmlStableDiffusionQueue() {
	std::string filepath;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (autoSaveEnabled) {
			filepath = autoSaveFilepath;
		}
	}
	if (!filepath.empty()) {
		saveToFile(filepath);
	}
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionQueue::addImageRequest(const ofxGgmlStableDiffusionImageRequest &request,
											 ofxGgmlStableDiffusionPriority priority, const std::string &tag) {
	int requestId = -1;
	int queueSize = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!enabled) {
			ofLogWarning("ofxGgmlStableDiffusionQueue") << "Queue is disabled";
			return -1;
		}

		if (maxQueueSize > 0 && queuedCount >= maxQueueSize) {
			ofLogWarning("ofxGgmlStableDiffusionQueue") << "Queue is full (max: " << maxQueueSize << ")";
			return -1;
		}

		auto queueRequest = createRequest(ofxGgmlStableDiffusionTaskForImageMode(request.mode), priority, tag);
		queueRequest->imageRequest = request;

		requestQueue.push(queueRequest);
		allRequests[queueRequest->requestId] = queueRequest;
		queuedCount++;
		requestId = queueRequest->requestId;
		queueSize = queuedCount;
	}

	ofLogNotice("ofxGgmlStableDiffusionQueue")
		<< "Added image request #" << requestId << " (priority: " << static_cast<int>(priority)
		<< ", queue size: " << queueSize << ")";

	triggerAutoSave();
	return requestId;
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionQueue::addVideoRequest(const ofxGgmlStableDiffusionVideoRequest &request,
											 ofxGgmlStableDiffusionPriority priority, const std::string &tag) {
	int requestId = -1;
	int queueSize = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!enabled) {
			ofLogWarning("ofxGgmlStableDiffusionQueue") << "Queue is disabled";
			return -1;
		}

		if (maxQueueSize > 0 && queuedCount >= maxQueueSize) {
			ofLogWarning("ofxGgmlStableDiffusionQueue") << "Queue is full (max: " << maxQueueSize << ")";
			return -1;
		}

		auto queueRequest = createRequest(ofxGgmlStableDiffusionTask::ImageToVideo, priority, tag);
		queueRequest->videoRequest = request;

		requestQueue.push(queueRequest);
		allRequests[queueRequest->requestId] = queueRequest;
		queuedCount++;
		requestId = queueRequest->requestId;
		queueSize = queuedCount;
	}

	ofLogNotice("ofxGgmlStableDiffusionQueue")
		<< "Added video request #" << requestId << " (priority: " << static_cast<int>(priority)
		<< ", queue size: " << queueSize << ")";

	triggerAutoSave();
	return requestId;
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionQueue::addModelLoadRequest(const ofxGgmlStableDiffusionContextSettings &settings,
												 ofxGgmlStableDiffusionPriority priority, const std::string &tag) {
	int requestId = -1;
	int queueSize = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!enabled) {
			ofLogWarning("ofxGgmlStableDiffusionQueue") << "Queue is disabled";
			return -1;
		}

		if (maxQueueSize > 0 && queuedCount >= maxQueueSize) {
			ofLogWarning("ofxGgmlStableDiffusionQueue") << "Queue is full (max: " << maxQueueSize << ")";
			return -1;
		}

		auto queueRequest = createRequest(ofxGgmlStableDiffusionTask::LoadModel, priority, tag);
		queueRequest->contextSettings = settings;

		requestQueue.push(queueRequest);
		allRequests[queueRequest->requestId] = queueRequest;
		queuedCount++;
		requestId = queueRequest->requestId;
		queueSize = queuedCount;
	}

	ofLogNotice("ofxGgmlStableDiffusionQueue")
		<< "Added model load request #" << requestId << " (priority: " << static_cast<int>(priority)
		<< ", queue size: " << queueSize << ")";

	triggerAutoSave();
	return requestId;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::setCompletionCallback(int requestId,
													std::function<void(const ofxGgmlStableDiffusionResult &)> callback) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = allRequests.find(requestId);
	if (it != allRequests.end()) {
		it->second->onComplete = callback;
	}
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::setErrorCallback(int requestId, std::function<void(const std::string &)> callback) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = allRequests.find(requestId);
	if (it != allRequests.end()) {
		it->second->onError = callback;
	}
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::setProgressCallback(int requestId, std::function<void(int, int, float)> callback) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = allRequests.find(requestId);
	if (it != allRequests.end()) {
		it->second->onProgress = callback;
	}
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionQueue::cancelRequest(int requestId) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = allRequests.find(requestId);
		if (it == allRequests.end()) {
			return false;
		}

		auto request = it->second;
		if (request->state != ofxGgmlStableDiffusionQueueState::Queued) {
			ofLogWarning("ofxGgmlStableDiffusionQueue")
				<< "Cannot cancel request #" << requestId << " (state: " << static_cast<int>(request->state) << ")";
			return false;
		}

		request->state = ofxGgmlStableDiffusionQueueState::Cancelled;
		request->completedTimeMicros = ofGetElapsedTimeMicros();
		queuedCount--;
	}

	ofLogNotice("ofxGgmlStableDiffusionQueue") << "Cancelled request #" << requestId;
	triggerAutoSave();
	return true;
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionQueue::cancelRequestsByTag(const std::string &tag) {
	int cancelledCount = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto &pair : allRequests) {
			if (pair.second->tag == tag && pair.second->state == ofxGgmlStableDiffusionQueueState::Queued) {
				pair.second->state = ofxGgmlStableDiffusionQueueState::Cancelled;
				pair.second->completedTimeMicros = ofGetElapsedTimeMicros();
				cancelledCount++;
			}
		}

		if (cancelledCount > 0) {
			queuedCount -= cancelledCount;
		}
	}

	if (cancelledCount > 0) {
		ofLogNotice("ofxGgmlStableDiffusionQueue") << "Cancelled " << cancelledCount << " requests with tag: " << tag;
		triggerAutoSave();
	}

	return cancelledCount;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::cancelAll() {
	int cancelledCount = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto &pair : allRequests) {
			if (pair.second->state == ofxGgmlStableDiffusionQueueState::Queued) {
				pair.second->state = ofxGgmlStableDiffusionQueueState::Cancelled;
				pair.second->completedTimeMicros = ofGetElapsedTimeMicros();
				cancelledCount++;
			}
		}

		if (cancelledCount > 0) {
			queuedCount -= cancelledCount;
		}
	}

	if (cancelledCount > 0) {
		ofLogNotice("ofxGgmlStableDiffusionQueue") << "Cancelled all " << cancelledCount << " queued requests";
		triggerAutoSave();
	}
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::clearHistory() {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = allRequests.begin();
		while (it != allRequests.end()) {
			if (it->second->state == ofxGgmlStableDiffusionQueueState::Completed ||
				it->second->state == ofxGgmlStableDiffusionQueueState::Failed ||
				it->second->state == ofxGgmlStableDiffusionQueueState::Cancelled) {
				it = allRequests.erase(it);
			} else {
				++it;
			}
		}
	}

	ofLogNotice("ofxGgmlStableDiffusionQueue") << "Cleared completed/failed/cancelled requests";
	triggerAutoSave();
}

//--------------------------------------------------------------
std::shared_ptr<ofxGgmlStableDiffusionQueueRequest> ofxGgmlStableDiffusionQueue::getNextRequest() {
	std::lock_guard<std::mutex> lock(mutex_);
	while (!requestQueue.empty()) {
		auto request = requestQueue.top();
		requestQueue.pop();

		if (request->state == ofxGgmlStableDiffusionQueueState::Queued) {
			return request;
		}
	}

	return nullptr;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::markRequestProcessing(int requestId) {
	bool updated = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = allRequests.find(requestId);
		if (it != allRequests.end()) {
			if (it->second->state == ofxGgmlStableDiffusionQueueState::Queued) {
				queuedCount--;
			}
			it->second->state = ofxGgmlStableDiffusionQueueState::Processing;
			it->second->startedTimeMicros = ofGetElapsedTimeMicros();
			currentRequest = it->second;
			updated = true;
		}
	}

	if (updated) {
		ofLogNotice("ofxGgmlStableDiffusionQueue") << "Processing request #" << requestId;
		triggerAutoSave();
	}
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::markRequestCompleted(int requestId, const ofxGgmlStableDiffusionResult &result) {
	std::function<void(const ofxGgmlStableDiffusionResult &)> onComplete;
	float processingTimeSeconds = 0.0f;
	bool updated = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = allRequests.find(requestId);
		if (it != allRequests.end()) {
			it->second->state = ofxGgmlStableDiffusionQueueState::Completed;
			it->second->completedTimeMicros = ofGetElapsedTimeMicros();
			it->second->result = result;
			onComplete = it->second->onComplete;
			processingTimeSeconds = it->second->getProcessingTimeSeconds();

			if (currentRequest && currentRequest->requestId == requestId) {
				currentRequest = nullptr;
			}
			updated = true;
		}
	}

	if (!updated) {
		return;
	}
	if (onComplete) {
		onComplete(result);
	}

	ofLogNotice("ofxGgmlStableDiffusionQueue")
		<< "Completed request #" << requestId << " (processing time: " << processingTimeSeconds << "s)";
	triggerAutoSave();
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::markRequestFailed(int requestId, const std::string &errorMessage) {
	std::function<void(const std::string &)> onError;
	bool updated = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = allRequests.find(requestId);
		if (it != allRequests.end()) {
			it->second->state = ofxGgmlStableDiffusionQueueState::Failed;
			it->second->completedTimeMicros = ofGetElapsedTimeMicros();
			it->second->result.success = false;
			it->second->result.error = errorMessage;
			onError = it->second->onError;

			if (currentRequest && currentRequest->requestId == requestId) {
				currentRequest = nullptr;
			}
			updated = true;
		}
	}

	if (!updated) {
		return;
	}
	if (onError) {
		onError(errorMessage);
	}

	ofLogError("ofxGgmlStableDiffusionQueue") << "Failed request #" << requestId << ": " << errorMessage;
	triggerAutoSave();
}

//--------------------------------------------------------------
std::shared_ptr<ofxGgmlStableDiffusionQueueRequest> ofxGgmlStableDiffusionQueue::getRequest(int requestId) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = allRequests.find(requestId);
	if (it != allRequests.end()) {
		return it->second;
	}
	return nullptr;
}

//--------------------------------------------------------------
std::vector<std::shared_ptr<ofxGgmlStableDiffusionQueueRequest>>
ofxGgmlStableDiffusionQueue::getRequestsByState(ofxGgmlStableDiffusionQueueState state) const {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<std::shared_ptr<ofxGgmlStableDiffusionQueueRequest>> requests;
	for (const auto &pair : allRequests) {
		if (pair.second->state == state) {
			requests.push_back(pair.second);
		}
	}
	return requests;
}

//--------------------------------------------------------------
std::vector<std::shared_ptr<ofxGgmlStableDiffusionQueueRequest>>
ofxGgmlStableDiffusionQueue::getRequestsByTag(const std::string &tag) const {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<std::shared_ptr<ofxGgmlStableDiffusionQueueRequest>> requests;
	for (const auto &pair : allRequests) {
		if (pair.second->tag == tag) {
			requests.push_back(pair.second);
		}
	}
	return requests;
}

//--------------------------------------------------------------
ofxGgmlStableDiffusionQueue::QueueStats ofxGgmlStableDiffusionQueue::getStats() const {
	std::lock_guard<std::mutex> lock(mutex_);
	QueueStats stats;
	stats.totalRequests = static_cast<int>(allRequests.size());

	float totalWaitTime = 0.0f;
	float totalProcessingTime = 0.0f;
	int completedCount = 0;

	for (const auto &pair : allRequests) {
		switch (pair.second->state) {
		case ofxGgmlStableDiffusionQueueState::Queued:
			stats.queuedRequests++;
			break;
		case ofxGgmlStableDiffusionQueueState::Processing:
			stats.processingRequests++;
			break;
		case ofxGgmlStableDiffusionQueueState::Completed:
			stats.completedRequests++;
			totalWaitTime += pair.second->getWaitTimeSeconds();
			totalProcessingTime += pair.second->getProcessingTimeSeconds();
			completedCount++;
			break;
		case ofxGgmlStableDiffusionQueueState::Failed:
			stats.failedRequests++;
			break;
		case ofxGgmlStableDiffusionQueueState::Cancelled:
			stats.cancelledRequests++;
			break;
		}
	}

	if (completedCount > 0) {
		stats.avgWaitTimeSeconds = totalWaitTime / completedCount;
		stats.avgProcessingTimeSeconds = totalProcessingTime / completedCount;
	}

	return stats;
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionQueue::getQueueSize() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return queuedCount;
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionQueue::isEmpty() const { return getQueueSize() == 0; }

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionQueue::isProcessing() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return currentRequest != nullptr;
}

//--------------------------------------------------------------
std::shared_ptr<ofxGgmlStableDiffusionQueueRequest> ofxGgmlStableDiffusionQueue::getCurrentRequest() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return currentRequest;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::setEnabled(bool enabled_) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		enabled = enabled_;
	}
	ofLogNotice("ofxGgmlStableDiffusionQueue") << "Queue " << (enabled_ ? "enabled" : "disabled");
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionQueue::isEnabled() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return enabled;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::setMaxQueueSize(int size) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		maxQueueSize = size;
	}
	ofLogNotice("ofxGgmlStableDiffusionQueue")
		<< "Max queue size set to: " << (size == 0 ? "unlimited" : std::to_string(size));
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionQueue::getMaxQueueSize() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return maxQueueSize;
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionQueue::saveToFile(const std::string &filepath) {
	ofJson json;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		json["version"] = "1.0";
		json["timestamp"] = ofGetElapsedTimeMicros();
		json["nextRequestId"] = nextRequestId.load(std::memory_order_relaxed);

		ofJson requestsJson = ofJson::array();
		for (const auto &pair : allRequests) {
			ofJson reqJson;
			reqJson["requestId"] = pair.second->requestId;
			reqJson["priority"] = static_cast<int>(pair.second->priority);
			reqJson["state"] = static_cast<int>(pair.second->state);
			reqJson["taskType"] = static_cast<int>(pair.second->taskType);
			reqJson["tag"] = pair.second->tag;
			reqJson["queuedTime"] = pair.second->queuedTimeMicros;
			reqJson["startedTime"] = pair.second->startedTimeMicros;
			reqJson["completedTime"] = pair.second->completedTimeMicros;
			requestsJson.push_back(reqJson);
		}
		json["requests"] = requestsJson;
	}

	return ofSaveJson(filepath, json);
}

//--------------------------------------------------------------
bool ofxGgmlStableDiffusionQueue::loadFromFile(const std::string &filepath) {
	ofJson json = ofLoadJson(filepath);
	if (json.empty()) {
		ofLogError("ofxGgmlStableDiffusionQueue") << "Failed to load queue from: " << filepath;
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		nextRequestId.store(json.value("nextRequestId", 1), std::memory_order_relaxed);
	}

	ofLogNotice("ofxGgmlStableDiffusionQueue") << "Loaded queue state from: " << filepath;
	return true;
}

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::setAutoSave(bool enabled_, const std::string &filepath) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		autoSaveEnabled = enabled_;
		autoSaveFilepath = filepath;
	}

	if (enabled_ && !filepath.empty()) {
		ofLogNotice("ofxGgmlStableDiffusionQueue") << "Auto-save enabled: " << filepath;
	}
}

//--------------------------------------------------------------
int ofxGgmlStableDiffusionQueue::generateRequestId() { return nextRequestId.fetch_add(1, std::memory_order_relaxed); }

//--------------------------------------------------------------
void ofxGgmlStableDiffusionQueue::triggerAutoSave() {
	bool enabledSnapshot = false;
	std::string filepath;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		enabledSnapshot = autoSaveEnabled;
		filepath = autoSaveFilepath;
	}
	if (enabledSnapshot && !filepath.empty()) {
		saveToFile(filepath);
	}
}

//--------------------------------------------------------------
std::shared_ptr<ofxGgmlStableDiffusionQueueRequest>
ofxGgmlStableDiffusionQueue::createRequest(ofxGgmlStableDiffusionTask taskType, ofxGgmlStableDiffusionPriority priority,
									   const std::string &tag) {
	auto request = std::make_shared<ofxGgmlStableDiffusionQueueRequest>();
	request->requestId = generateRequestId();
	request->priority = priority;
	request->state = ofxGgmlStableDiffusionQueueState::Queued;
	request->taskType = taskType;
	request->tag = tag;
	request->queuedTimeMicros = ofGetElapsedTimeMicros();
	return request;
}
