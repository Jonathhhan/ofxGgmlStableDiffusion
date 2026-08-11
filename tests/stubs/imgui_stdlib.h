#pragma once

#include "ofxImGui.h"

#include <string>

namespace ImGui {
inline bool InputText(const char*, std::string*, int = 0) {
	return false;
}

inline bool InputTextMultiline(
	const char*,
	std::string*,
	const ImVec2& = ImVec2(0.0f, 0.0f),
	int = 0) {
	return false;
}
}
