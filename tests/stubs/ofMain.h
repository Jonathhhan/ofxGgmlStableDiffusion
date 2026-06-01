#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

enum ofImageType {
	OF_IMAGE_GRAYSCALE = 1,
	OF_IMAGE_COLOR = 3,
	OF_IMAGE_COLOR_ALPHA = 4
};

class ofPixels {
public:
	bool isAllocated() const {
		return !storage.empty();
	}

	std::size_t getWidth() const {
		return width;
	}

	std::size_t getHeight() const {
		return height;
	}

	std::size_t getNumChannels() const {
		return channels;
	}

	unsigned char* getData() {
		return storage.empty() ? nullptr : storage.data();
	}

	const unsigned char* getData() const {
		return storage.empty() ? nullptr : storage.data();
	}

	void setFromPixels(const unsigned char* data, int pixelWidth, int pixelHeight, ofImageType type) {
		width = pixelWidth > 0 ? static_cast<std::size_t>(pixelWidth) : 0;
		height = pixelHeight > 0 ? static_cast<std::size_t>(pixelHeight) : 0;
		channels = static_cast<std::size_t>(std::max(0, static_cast<int>(type)));
		const std::size_t byteCount = width * height * channels;
		storage.resize(byteCount);
		if (data && byteCount > 0) {
			std::copy(data, data + byteCount, storage.begin());
		}
	}

	void setFromPixels(const ofPixels& pixels) {
		storage = pixels.storage;
		width = pixels.width;
		height = pixels.height;
		channels = pixels.channels;
	}

	void clear() {
		storage.clear();
		width = 0;
		height = 0;
		channels = 0;
	}

private:
	std::size_t width = 0;
	std::size_t height = 0;
	std::size_t channels = 0;
	std::vector<unsigned char> storage;
};

class ofImage {
public:
	bool isAllocated() const {
		return pixels_.isAllocated();
	}

	void clear() {
		pixels_.clear();
	}

	void setFromPixels(const ofPixels& pixels) {
		pixels_.setFromPixels(pixels);
	}

	bool load(const std::string&) {
		return false;
	}

	bool save(const std::string& path) const {
		std::ofstream output(path, std::ios::binary);
		if (!output.is_open()) {
			return false;
		}
		output << "stub-image";
		return output.good();
	}

	void resize(int width, int height) {
		if (!pixels_.isAllocated()) {
			return;
		}
		ofPixels resized;
		const int channels = static_cast<int>(pixels_.getNumChannels());
		resized.setFromPixels(
			pixels_.getData(),
			width,
			height,
			channels == OF_IMAGE_COLOR_ALPHA ? OF_IMAGE_COLOR_ALPHA :
				(channels == OF_IMAGE_GRAYSCALE ? OF_IMAGE_GRAYSCALE : OF_IMAGE_COLOR));
		pixels_ = resized;
	}

	float getWidth() const {
		return static_cast<float>(pixels_.getWidth());
	}

	float getHeight() const {
		return static_cast<float>(pixels_.getHeight());
	}

	ofPixels& getPixels() {
		return pixels_;
	}

	const ofPixels& getPixels() const {
		return pixels_;
	}

	void draw(float, float, float, float) const {
	}

private:
	ofPixels pixels_;
};

class ofTexture {};
class ofFbo {};

class ofBaseApp {
public:
	virtual ~ofBaseApp() = default;
};

class ofThread {
public:
	virtual ~ofThread() = default;
	virtual void threadedFunction() {}

	bool isThreadRunning() const {
		return false;
	}

	void startThread() {}
	void stopThread() {}
	void waitForThread(bool = true) {}
	bool lock() { return true; }
	void unlock() {}
};

class ofLogStream {
public:
	template <typename T>
	ofLogStream& operator<<(const T&) {
		return *this;
	}
};

enum ofLogLevel {
	OF_LOG_VERBOSE,
	OF_LOG_NOTICE,
	OF_LOG_WARNING,
	OF_LOG_ERROR,
	OF_LOG_SILENT
};

inline void ofSetLogLevel(ofLogLevel) {}

#ifndef OF_MAIN_STUB_CUSTOM_LOG_FUNCTIONS
inline ofLogStream ofLogNotice(const std::string& = "") { return {}; }
inline ofLogStream ofLogWarning(const std::string& = "") { return {}; }
inline ofLogStream ofLogError(const std::string& = "") { return {}; }
inline ofLogStream ofLogVerbose(const std::string& = "") { return {}; }
#endif

class ofJson {
public:
	ofJson() = default;
	ofJson(const char*) {}
	ofJson(const std::string&) {}
	ofJson(bool) {}
	ofJson(int) {}
	ofJson(unsigned int) {}
	ofJson(long long) {}
	ofJson(unsigned long long) {}
	ofJson(float) {}
	ofJson(double) {}
	ofJson(std::initializer_list<std::pair<const char*, ofJson>>) {}

	class Proxy {
	public:
		Proxy& operator[](const std::string&) {
			return *this;
		}

		template <typename T>
		Proxy& operator=(const T&) {
			return *this;
		}

		template <typename T>
		void push_back(const T&) {
		}
	};

	Proxy operator[](const std::string&) {
		return {};
	}

	static ofJson array() {
		return {};
	}

	void push_back(const ofJson&) {
	}

	template <typename T>
	void push_back(const T&) {
	}

	std::string dump(int = -1) const {
		return "{}";
	}

	static ofJson parse(const std::string&) {
		return {};
	}
};

inline bool ofSaveJson(const std::string& path, const ofJson& json) {
	std::ofstream output(path);
	if (!output.is_open()) {
		return false;
	}
	output << json.dump();
	return true;
}

inline bool ofSavePrettyJson(const std::string& path, const ofJson& json) {
	std::ofstream output(path);
	if (!output.is_open()) {
		return false;
	}
	output << json.dump(2);
	return true;
}

#ifndef OF_MAIN_STUB_CUSTOM_TIME_FUNCTIONS
inline std::uint64_t ofGetElapsedTimeMicros() {
	using namespace std::chrono;
	return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

inline std::uint64_t ofGetElapsedTimeMillis() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
#endif

inline std::string ofTrim(const std::string& value) {
	const auto isSpaceChar = [](unsigned char c) { return std::isspace(c) != 0; };
	auto first = std::find_if_not(value.begin(), value.end(), isSpaceChar);
	auto last = std::find_if_not(value.rbegin(), value.rend(), isSpaceChar).base();
	if (first >= last) {
		return {};
	}
	return std::string(first, last);
}

inline std::string ofToUpper(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
	return value;
}

inline std::string ofToDataPath(const std::string& value, bool = false) {
	return value;
}

inline std::string ofGetEnv(const std::string& key) {
#if defined(_MSC_VER)
	char* value = nullptr;
	std::size_t length = 0;
	if (_dupenv_s(&value, &length, key.c_str()) != 0 || value == nullptr) {
		return "";
	}
	std::string result(value, length > 0 ? length - 1 : 0);
	free(value);
	return result;
#else
	const char* value = std::getenv(key.c_str());
	return value == nullptr ? "" : std::string(value);
#endif
}

template <typename T>
inline std::string ofToString(const T& value) {
	std::ostringstream stream;
	stream << value;
	return stream.str();
}

inline std::string ofToString(float value, int precision) {
	std::ostringstream stream;
	stream << std::fixed << std::setprecision(precision) << value;
	return stream.str();
}

inline std::string ofGetTimestampString() {
	return "19700101-000000";
}

inline std::string ofGetTimestampString(const std::string&) {
	return "19700101-000000";
}

inline int ofGetWidth() {
	return 1280;
}

inline int ofGetHeight() {
	return 720;
}

inline void ofBackground(int) {}
inline void ofSetColor(int, int = 255, int = 255, int = 255) {}
inline void ofDrawBitmapString(const std::string&, float, float) {}
inline void ofSetWindowTitle(const std::string&) {}
inline void ofSetFrameRate(int) {}

template <typename T>
inline T ofClamp(T value, T minValue, T maxValue) {
	return std::max(minValue, std::min(value, maxValue));
}

class ofFile {
public:
	ofFile() = default;
	explicit ofFile(const std::string& path)
		: path_(path) {
	}

	std::string getExtension() const {
		return std::filesystem::path(path_).extension().string();
	}

	std::string getBaseName() const {
		return std::filesystem::path(path_).stem().string();
	}

	std::string getAbsolutePath() const {
		std::error_code error;
		const auto absolute = std::filesystem::absolute(path_, error);
		return error ? path_ : absolute.string();
	}

	bool isFile() const {
		std::error_code error;
		return std::filesystem::is_regular_file(path_, error);
	}

	static bool doesFileExist(const std::string& path) {
		return std::filesystem::exists(path);
	}

private:
	std::string path_;
};

class ofDirectory {
public:
	ofDirectory() = default;
	explicit ofDirectory(const std::string& path)
		: path_(path) {
	}

	bool exists() const {
		return std::filesystem::exists(path_);
	}

	void allowExt(const std::string& extension) {
		std::string normalized = extension;
		if (!normalized.empty() && normalized.front() == '.') {
			normalized.erase(normalized.begin());
		}
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		allowedExtensions.push_back(normalized);
	}

	void listDir() {
		files.clear();
		std::error_code error;
		if (!std::filesystem::exists(path_, error)) {
			return;
		}
		for (const auto& entry : std::filesystem::directory_iterator(path_, error)) {
			if (error || !entry.is_regular_file(error)) {
				continue;
			}
			std::string extension = entry.path().extension().string();
			if (!extension.empty() && extension.front() == '.') {
				extension.erase(extension.begin());
			}
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			if (!allowedExtensions.empty() &&
				std::find(allowedExtensions.begin(), allowedExtensions.end(), extension) == allowedExtensions.end()) {
				continue;
			}
			files.emplace_back(entry.path().string());
		}
	}

	std::size_t size() const {
		return files.size();
	}

	const ofFile& getFile(int index) const {
		return files.at(static_cast<std::size_t>(index));
	}

private:
	std::string path_;
	std::vector<std::string> allowedExtensions;
	std::vector<ofFile> files;
};

class ofFileDialogResult {
public:
	bool bSuccess = false;

	std::string getPath() {
		return path;
	}

	std::string path;
};

inline ofFileDialogResult ofSystemLoadDialog(const std::string&) {
	return {};
}

enum ofWindowModeType {
	OF_WINDOW = 0
};

struct ofGLFWWindowSettings {
	int width = 0;
	int height = 0;
	bool visible = true;
	ofWindowModeType windowMode = OF_WINDOW;

	void setSize(int w, int h) {
		width = w;
		height = h;
	}
};

using ofAppBaseWindow = int;
using ofAppBaseWindowPtr = std::shared_ptr<ofAppBaseWindow>;

inline ofAppBaseWindowPtr ofCreateWindow(const ofGLFWWindowSettings&) {
	return std::make_shared<ofAppBaseWindow>(0);
}

inline ofAppBaseWindowPtr ofGetCurrentWindow() {
	return std::make_shared<ofAppBaseWindow>(0);
}

inline void ofRunApp(const ofAppBaseWindowPtr&, const std::shared_ptr<ofBaseApp>&) {}
inline int ofRunMainLoop() { return 0; }
inline void ofExit(int = 0) {}
