#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::filesystem::path& path) {
	std::ifstream input(path);
	if (!input.is_open()) {
		std::cerr << "Failed to open " << path << std::endl;
		std::exit(1);
	}
	return std::string(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>());
}

void expectContains(
	const std::string& content,
	const std::string& needle,
	const std::filesystem::path& path) {
	if (content.find(needle) == std::string::npos) {
		std::cerr << "Expected to find '" << needle << "' in " << path << std::endl;
		std::exit(1);
	}
}

void expectNotContains(
	const std::string& content,
	const std::string& needle,
	const std::filesystem::path& path) {
	if (content.find(needle) != std::string::npos) {
		std::cerr << "Did not expect to find '" << needle << "' in " << path << std::endl;
		std::exit(1);
	}
}

} // namespace

int main() {
	const std::filesystem::path repoRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path();

	const std::filesystem::path readmePath = repoRoot / "README.md";
	const std::filesystem::path docsWorkflowPath = repoRoot / ".github" / "workflows" / "docs.yml";
	const std::filesystem::path apiRefPath = repoRoot / "docs" / "API_REFERENCE.md";
	const std::filesystem::path troubleshootingPath = repoRoot / "docs" / "TROUBLESHOOTING.md";
	const std::filesystem::path starterReadmePath =
		repoRoot / "ofxGgmlStableDiffusionExample" / "README.md";
	const std::filesystem::path starterAppPath =
		repoRoot / "ofxGgmlStableDiffusionExample" / "src" / "ofApp.cpp";
	const std::filesystem::path basicReadmePath =
		repoRoot / "ofxGgmlStableDiffusionBasicGenerationExample" / "README.md";
	const std::filesystem::path basicAppPath =
		repoRoot / "ofxGgmlStableDiffusionBasicGenerationExample" / "src" / "ofApp.cpp";
	const std::filesystem::path imageWorkflowReadmePath =
		repoRoot / "ofxGgmlStableDiffusionImageWorkflowExample" / "README.md";
	const std::filesystem::path imageWorkflowAppPath =
		repoRoot / "ofxGgmlStableDiffusionImageWorkflowExample" / "src" / "ofApp.cpp";
	const std::filesystem::path videoReadmePath =
		repoRoot / "ofxGgmlStableDiffusionVideoGenerationExample" / "README.md";
	const std::filesystem::path videoAppPath =
		repoRoot / "ofxGgmlStableDiffusionVideoGenerationExample" / "src" / "ofApp.cpp";
	const std::filesystem::path videoControlReadmePath =
		repoRoot / "ofxGgmlStableDiffusionVideoControlFramesExample" / "README.md";
	const std::filesystem::path videoControlAppPath =
		repoRoot / "ofxGgmlStableDiffusionVideoControlFramesExample" / "src" / "ofApp.cpp";
	const std::filesystem::path creativeLoopReadmePath =
		repoRoot / "ofxGgmlStableDiffusionCreativeLoopExample" / "README.md";
	const std::filesystem::path creativeLoopAppPath =
		repoRoot / "ofxGgmlStableDiffusionCreativeLoopExample" / "src" / "ofApp.cpp";
	const std::filesystem::path loraEmbeddingReadmePath =
		repoRoot / "ofxGgmlStableDiffusionLoraEmbeddingExample" / "README.md";
	const std::filesystem::path loraEmbeddingAppPath =
		repoRoot / "ofxGgmlStableDiffusionLoraEmbeddingExample" / "src" / "ofApp.cpp";

	const std::string readme = readFile(readmePath);
	const std::string docsWorkflow = readFile(docsWorkflowPath);
	const std::string apiReference = readFile(apiRefPath);
	const std::string troubleshooting = readFile(troubleshootingPath);
	const std::string starterReadme = readFile(starterReadmePath);
	const std::string starterApp = readFile(starterAppPath);
	const std::string basicReadme = readFile(basicReadmePath);
	const std::string basicApp = readFile(basicAppPath);
	const std::string imageWorkflowReadme = readFile(imageWorkflowReadmePath);
	const std::string imageWorkflowApp = readFile(imageWorkflowAppPath);
	const std::string videoReadme = readFile(videoReadmePath);
	const std::string videoApp = readFile(videoAppPath);
	const std::string videoControlReadme = readFile(videoControlReadmePath);
	const std::string videoControlApp = readFile(videoControlAppPath);
	const std::string creativeLoopReadme = readFile(creativeLoopReadmePath);
	const std::string creativeLoopApp = readFile(creativeLoopAppPath);
	const std::string loraEmbeddingReadme = readFile(loraEmbeddingReadmePath);
	const std::string loraEmbeddingApp = readFile(loraEmbeddingAppPath);

	expectContains(readme, "## Feature Readiness", readmePath);
	expectContains(readme, "## Threading Contract", readmePath);
	expectContains(readme, "Canonical starter", readmePath);
	expectContains(readme, "The canonical `ofxGgmlStableDiffusionExample/` project is intentionally small", readmePath);
	expectContains(docsWorkflow, "ofxGgmlStableDiffusionExample", docsWorkflowPath);
	expectContains(docsWorkflow, "Starter Example", docsWorkflowPath);
	expectContains(readme, "Current addon version: `1.0.2`", readmePath);
	expectContains(readme, "run-stable-diffusion-runtime-smoke.ps1", readmePath);
	expectContains(readme, "run-wan-context-smoke.ps1", readmePath);
	expectContains(apiReference, "`weightType` - Weight precision type", apiRefPath);
	expectContains(apiReference, "Generator-backed experimentation surface", apiRefPath);
	expectContains(apiReference, "ofxGgmlStableDiffusionCreativeWorkflow", apiRefPath);
	expectContains(apiReference, "dispatch their callbacks from the thread that calls `update()`", apiRefPath);
	expectContains(troubleshooting, "**Safe from any thread:**", troubleshootingPath);
	expectContains(starterReadme, "Small canonical", starterReadmePath);
	expectContains(starterReadme, "ofxImGui", starterReadmePath);
	expectContains(starterApp, "Stable Diffusion Starter", starterAppPath);
	expectContains(starterApp, "stableDiffusion.generate(request)", starterAppPath);
	expectContains(starterApp, "ofxGgmlStableDiffusionExampleRequestCancel", starterAppPath);
	expectContains(basicReadme, "worker thread", basicReadmePath);
	expectContains(basicReadme, "ofxImGui", basicReadmePath);
	expectContains(imageWorkflowReadme, "ControlNet", imageWorkflowReadmePath);
	expectContains(imageWorkflowReadme, "ofxImGui", imageWorkflowReadmePath);
	expectContains(videoReadme, "ofxImGui", videoReadmePath);
	expectContains(videoReadme, "UMT5 / T5XXL", videoReadmePath);
	expectContains(videoControlReadme, "ofxImGui", videoControlReadmePath);
	expectContains(videoControlReadme, "UMT5 / T5XXL", videoControlReadmePath);
	expectContains(creativeLoopReadme, "ofxImGui", creativeLoopReadmePath);
	expectContains(loraEmbeddingReadme, "ofxImGui", loraEmbeddingReadmePath);
	expectContains(basicApp, "settings.weightType = SD_TYPE_COUNT", basicAppPath);
	expectContains(basicApp, "gui.begin()", basicAppPath);
	expectContains(imageWorkflowApp, "ImageToImage", imageWorkflowAppPath);
	expectContains(imageWorkflowApp, "maskImage", imageWorkflowAppPath);
	expectContains(imageWorkflowApp, "controlCond", imageWorkflowAppPath);
	expectContains(imageWorkflowApp, "gui.begin()", imageWorkflowAppPath);
	expectContains(videoApp, "generateVideo", videoAppPath);
	expectContains(videoApp, "settings.diffusionModelPath", videoAppPath);
	expectContains(videoApp, "settings.t5xxlPath", videoAppPath);
	expectContains(videoApp, "settings.vaePath", videoAppPath);
	expectContains(videoApp, "OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE", videoAppPath);
	expectContains(videoApp, "gui.begin()", videoAppPath);
	expectContains(videoControlApp, "controlFrames", videoControlAppPath);
	expectContains(videoControlApp, "vaceStrength", videoControlAppPath);
	expectContains(videoControlApp, "settings.diffusionModelPath", videoControlAppPath);
	expectContains(videoControlApp, "settings.t5xxlPath", videoControlAppPath);
	expectContains(videoControlApp, "settings.vaePath", videoControlAppPath);
	expectContains(videoControlApp, "gui.begin()", videoControlAppPath);
	expectContains(creativeLoopApp, "loop.start", creativeLoopAppPath);
	expectContains(creativeLoopApp, "gui.begin()", creativeLoopAppPath);
	expectContains(loraEmbeddingApp, "setLoras", loraEmbeddingAppPath);
	expectContains(loraEmbeddingApp, "reloadEmbeddings", loraEmbeddingAppPath);
	expectContains(loraEmbeddingApp, "gui.begin()", loraEmbeddingAppPath);

	expectNotContains(apiReference, "`wType` - Weight precision type", apiRefPath);
	expectNotContains(apiReference, "placeholder results", apiRefPath);
	expectNotContains(troubleshooting, "settings.wType", troubleshootingPath);
	expectNotContains(troubleshooting, "Not Thread-Safe (main thread only)", troubleshootingPath);
	expectNotContains(readme, "All public API methods should be called from the main thread", readmePath);
	expectNotContains(readme, "single-threaded use from the main thread", readmePath);
	expectNotContains(docsWorkflow, "ofxGgmlStableDiffusionCancellationExample", docsWorkflowPath);
	expectNotContains(docsWorkflow, "Cancellation Example", docsWorkflowPath);
	expectNotContains(starterApp, "generateVideo", starterAppPath);
	expectNotContains(starterApp, "Holoscan", starterAppPath);
	expectNotContains(starterApp, "RealtimeVideoSession", starterAppPath);
	expectNotContains(starterApp, "setLoras", starterAppPath);
	expectNotContains(starterApp, "reloadEmbeddings", starterAppPath);
	expectNotContains(basicReadme, "settings.wType", basicReadmePath);

	const std::vector<std::filesystem::path> requiredExampleFiles = {
		repoRoot / "ofxGgmlStableDiffusionExample" / "README.md",
		repoRoot / "ofxGgmlStableDiffusionExample" / "Makefile",
		repoRoot / "ofxGgmlStableDiffusionExample" / "config.make",
		repoRoot / "ofxGgmlStableDiffusionExample" / "src" / "main.cpp",
		repoRoot / "ofxGgmlStableDiffusionBasicGenerationExample" / "Makefile",
		repoRoot / "ofxGgmlStableDiffusionBasicGenerationExample" / "config.make",
		repoRoot / "ofxGgmlStableDiffusionBasicGenerationExample" / "src" / "main.cpp",
		repoRoot / "ofxGgmlStableDiffusionImageWorkflowExample" / "Makefile",
		repoRoot / "ofxGgmlStableDiffusionImageWorkflowExample" / "config.make",
		repoRoot / "ofxGgmlStableDiffusionImageWorkflowExample" / "src" / "main.cpp",
		repoRoot / "ofxGgmlStableDiffusionVideoGenerationExample" / "Makefile",
		repoRoot / "ofxGgmlStableDiffusionVideoGenerationExample" / "config.make",
		repoRoot / "ofxGgmlStableDiffusionVideoGenerationExample" / "src" / "main.cpp",
		repoRoot / "ofxGgmlStableDiffusionVideoControlFramesExample" / "Makefile",
		repoRoot / "ofxGgmlStableDiffusionVideoControlFramesExample" / "config.make",
		repoRoot / "ofxGgmlStableDiffusionVideoControlFramesExample" / "src" / "main.cpp",
		repoRoot / "ofxGgmlStableDiffusionCreativeLoopExample" / "Makefile",
		repoRoot / "ofxGgmlStableDiffusionCreativeLoopExample" / "config.make",
		repoRoot / "ofxGgmlStableDiffusionCreativeLoopExample" / "src" / "main.cpp",
		repoRoot / "ofxGgmlStableDiffusionLoraEmbeddingExample" / "Makefile",
		repoRoot / "ofxGgmlStableDiffusionLoraEmbeddingExample" / "config.make",
		repoRoot / "ofxGgmlStableDiffusionLoraEmbeddingExample" / "src" / "main.cpp"
	};
	for (const auto& path : requiredExampleFiles) {
		if (!std::filesystem::exists(path)) {
			std::cerr << "Missing expected example project file: " << path << std::endl;
			return 1;
		}
	}

	return 0;
}
