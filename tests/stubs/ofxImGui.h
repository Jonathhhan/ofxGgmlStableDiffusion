#pragma once

#include <cstddef>
#include <memory>

#include "ofMain.h"

using ImGuiConfigFlags = int;
static constexpr ImGuiConfigFlags ImGuiConfigFlags_None = 0;
static constexpr int ImGuiCond_Once = 1;

#ifndef IM_ARRAYSIZE
#define IM_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*(_ARR))))
#endif

struct ImVec2 {
	float x;
	float y;

	ImVec2(float valueX = 0.0f, float valueY = 0.0f)
		: x(valueX)
		, y(valueY) {
	}
};

namespace ImGui {
inline void SetNextWindowSize(const ImVec2&, int = 0) {}
inline bool Begin(const char*) { return true; }
inline void End() {}
inline void Separator() {}
inline void SameLine(float = 0.0f, float = -1.0f) {}
inline void BeginDisabled(bool = true) {}
inline void EndDisabled() {}
inline void Text(const char*, ...) {}
inline void TextWrapped(const char*, ...) {}
inline bool Button(const char*) { return false; }
inline bool Checkbox(const char*, bool*) { return false; }
inline bool CollapsingHeader(const char*) { return false; }
inline bool Combo(const char*, int*, const char* const[], int, int = -1) { return false; }
inline bool BeginCombo(const char*, const char*) { return false; }
inline void EndCombo() {}
inline bool Selectable(const char*, bool = false) { return false; }
inline void SetItemDefaultFocus() {}
inline bool InputText(const char*, char*, std::size_t, int = 0) { return false; }
inline bool InputTextMultiline(const char*, char*, std::size_t, const ImVec2&, int = 0) { return false; }
inline bool InputInt(const char*, int*, int = 1, int = 100) { return false; }
inline bool SliderInt(const char*, int*, int, int) { return false; }
inline bool SliderFloat(const char*, float*, float, float) { return false; }
inline void ProgressBar(float, const ImVec2& = ImVec2(), const char* = nullptr) {}
}

namespace ofxImGui {
struct SetupState {
	enum Value {
		Success = 1
	};
};

class Gui {
public:
	int setup(std::nullptr_t = nullptr, bool = true, ImGuiConfigFlags = ImGuiConfigFlags_None, bool = true) {
		return SetupState::Success;
	}

	int setup(std::shared_ptr<ofAppBaseWindow>&, void* = nullptr, bool = true, ImGuiConfigFlags = ImGuiConfigFlags_None, bool = true) {
		return SetupState::Success;
	}

	void begin() {}
	void end() {}
};
}
