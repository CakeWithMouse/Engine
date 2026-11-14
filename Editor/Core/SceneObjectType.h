#pragma once
#include <string_view>

namespace EditorCore {
// Repository scenes v1-v3 and SceneAssets use this spelling. Do not invent aliases:
// project component types with model assets have their own serialization contract.
inline bool IsModelObjectType(std::string_view type) { return type == "Engine.Model"; }
inline bool IsPrimitiveObjectType(std::string_view type) { return type.substr(0, 10) == "Primitive."; }
}
