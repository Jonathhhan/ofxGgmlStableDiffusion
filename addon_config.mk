# All variables and this file are optional, if they are not present the PG and the
# makefiles will try to parse the correct values from the file system.
#
# Variables that specify exclusions can use % as a wildcard to specify that anything in
# that position will match. A partial path can also be specified to, for example, exclude
# a whole folder from the parsed paths from the file system
#
# Variables can be specified using = or +=
# = will clear the contents of that variable both specified from the file or the ones parsed
# from the file system
# += will add the values to the previous ones in the file or the ones parsed from the file
# system
#
# The PG can be used to detect errors in this file, just create a new project with this addon
# and the PG will write to the console the kind of error and in which line it is

meta:
	ADDON_NAME = ofxGgmlStableDiffusion
	ADDON_DESCRIPTION = Stable Diffusion: https://github.com/leejet/stable-diffusion.cpp
	ADDON_AUTHOR = Jonathan Frank
	ADDON_TAGS = "Stable Diffusion" "Artificial Intelligence" "Image Generation"
	ADDON_URL = https://github.com/Jonathhhan/ofxGgmlStableDiffusion

common:
	ADDON_INCLUDES += src
	ADDON_INCLUDES += libs/stable-diffusion/include
	ADDON_DEPENDENCIES += ofxGgmlCore
	# stable-diffusion.cpp is bundled as a separately built native library.
	ADDON_SOURCES_EXCLUDE += .github/%
	ADDON_SOURCES_EXCLUDE += benchmarks/%
	ADDON_SOURCES_EXCLUDE += docs/%
	ADDON_SOURCES_EXCLUDE += examples/%
	ADDON_SOURCES_EXCLUDE += libs/ggml/%
	ADDON_SOURCES_EXCLUDE += libs/stable-diffusion/source/%
	ADDON_SOURCES_EXCLUDE += libs/stable-diffusion/build/%
	ADDON_SOURCES_EXCLUDE += libs/variants/%
	ADDON_SOURCES_EXCLUDE += scripts/%
	ADDON_SOURCES_EXCLUDE += tests/%
	ADDON_SOURCES_EXCLUDE += ofxGgmlStableDiffusionBasicGenerationExample/%
	ADDON_SOURCES_EXCLUDE += ofxGgmlStableDiffusionCreativeLoopExample/%
	ADDON_SOURCES_EXCLUDE += ofxGgmlStableDiffusionExample/%
	ADDON_SOURCES_EXCLUDE += ofxGgmlStableDiffusionImageWorkflowExample/%
	ADDON_SOURCES_EXCLUDE += ofxGgmlStableDiffusionLoraEmbeddingExample/%
	ADDON_SOURCES_EXCLUDE += ofxGgmlStableDiffusionVideoControlFramesExample/%
	ADDON_SOURCES_EXCLUDE += ofxGgmlStableDiffusionVideoGenerationExample/%
	ADDON_INCLUDES_EXCLUDE += .github/%
	ADDON_INCLUDES_EXCLUDE += benchmarks/%
	ADDON_INCLUDES_EXCLUDE += docs/%
	ADDON_INCLUDES_EXCLUDE += examples/%
	ADDON_INCLUDES_EXCLUDE += libs/ggml/%
	ADDON_INCLUDES_EXCLUDE += libs/stable-diffusion/source/%
	ADDON_INCLUDES_EXCLUDE += libs/stable-diffusion/build/%
	ADDON_INCLUDES_EXCLUDE += libs/variants/%
	ADDON_INCLUDES_EXCLUDE += scripts/%
	ADDON_INCLUDES_EXCLUDE += tests/%
	ADDON_INCLUDES_EXCLUDE += ofxGgmlStableDiffusionBasicGenerationExample/%
	ADDON_INCLUDES_EXCLUDE += ofxGgmlStableDiffusionCreativeLoopExample/%
	ADDON_INCLUDES_EXCLUDE += ofxGgmlStableDiffusionExample/%
	ADDON_INCLUDES_EXCLUDE += ofxGgmlStableDiffusionImageWorkflowExample/%
	ADDON_INCLUDES_EXCLUDE += ofxGgmlStableDiffusionLoraEmbeddingExample/%
	ADDON_INCLUDES_EXCLUDE += ofxGgmlStableDiffusionVideoControlFramesExample/%
	ADDON_INCLUDES_EXCLUDE += ofxGgmlStableDiffusionVideoGenerationExample/%
	ADDON_LIBS_EXCLUDE += libs/ggml/%
	ADDON_LIBS_EXCLUDE += libs/variants/%

linux64:
	ADDON_LIBS += libs/stable-diffusion/lib/Linux64/libstable-diffusion.so
	ADDON_LDFLAGS += -Wl,-rpath=../../../../addons/ofxGgmlStableDiffusion/libs/stable-diffusion/lib/Linux64

linux:

linuxarmv6l:

linuxarmv7l:

msys2:

vs:
	ADDON_LIBS += libs/stable-diffusion/lib/vs/stable-diffusion.lib

android/armeabi:

android/armeabi-v7a:

osx:
	ADDON_LIBS += libs/stable-diffusion/lib/osx/libstable-diffusion.dylib
	ADDON_LDFLAGS += -Wl,-rpath,@loader_path/../../../../addons/ofxGgmlStableDiffusion/libs/stable-diffusion/lib/osx

ios:
	# iOS requires static linking
	ADDON_LIBS += libs/stable-diffusion/lib/ios/libstable-diffusion.a
	ADDON_CFLAGS += -DIOS_PLATFORM

emscripten:
