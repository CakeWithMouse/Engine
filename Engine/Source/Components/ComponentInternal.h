#pragma once
#include <Engine/Components/GameComponents.h>

// Private bridge for engine services, regression tests and bundled legacy scenes.
// Source/ is not exported with the SDK. Do not include from a game project.
namespace EngineInternal
{
struct ComponentAccess
{
    static DirectX::XMFLOAT4X4 WorldMatrix(GameComponent& component) { return component.GetWorldMatrix(); }
    static bool ParentWithoutScale(GameComponent& component, GameComponent* parent) { return component.SetParentWithoutScale(parent); }
    static void Shaders(GameComponent& component, const std::string& vertex, const std::string& pixel) { component.SetShaderNames(vertex, pixel); }
    static void Reflection(GameComponent& component, float strength, float fresnel = 5.0f) { component.SetReflectionSettings(strength, fresnel); }
    static void StaticShadow(GameComponent& component, bool enabled) { component.SetStaticShadowCaster(enabled); }
    static void Remap(GameComponent& component, const std::map<ComponentId, ComponentId>& ids) { component.RemapProjectReferences(ids); }
};
}
