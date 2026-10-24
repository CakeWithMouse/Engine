#pragma once
#include <Engine/Components/GameComponents.h>
#include <cstddef>
#include <string_view>
#include <type_traits>

// Reserved metadata only: no loading, rendering or automatic application yet.
struct ComponentVisualDefaults
{
    const char* geometryAsset = nullptr;
    const char* materialAsset = nullptr;
};

// Module-local C++ API, NOT a DLL ABI. The owner module must outlive its objects.
struct ComponentType
{
    const char* typeName;
    const char* displayName;
    std::unique_ptr<GameComponent> (*create)();
    ComponentVisualDefaults visualDefaults{};
};

struct ComponentTypeCatalog
{
    const ComponentType* data = nullptr;
    std::size_t size = 0;

    const ComponentType* Find(std::string_view typeName) const noexcept
    {
        for (std::size_t i = 0; i < size; ++i)
            if (typeName == data[i].typeName) return &data[i];
        return nullptr;
    }
};

namespace EngineComponentDetail
{
    template<class T> std::unique_ptr<GameComponent> Create()
    {
        static_assert(std::is_convertible_v<T*, GameComponent*>,
            "ENGINE_COMPONENT requires public, unambiguous GameComponent inheritance");
        static_assert(!std::is_abstract_v<T>, "ENGINE_COMPONENT cannot create an abstract class");
        static_assert(std::is_default_constructible_v<T>,
            "ENGINE_COMPONENT requires a public default constructor");
        return std::make_unique<T>();
    }
}
