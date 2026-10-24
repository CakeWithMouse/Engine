#include <Engine/Render/SkeletalLoader.h>

#if !defined(ENGINE_ENABLE_ASSIMP)

SkeletalLoadResult LoadSkeletalModelFromMemory(const void* /*data*/, size_t /*size*/, std::string_view /*hint*/)
{
    SkeletalLoadResult result;
    result.Success = false;
    result.ErrorMessage = "Assimp not enabled";
    return result;
}

SkeletalLoadResult LoadSkeletalModelFromVfs(std::string_view /*vpath*/)
{
    SkeletalLoadResult result;
    result.Success = false;
    result.ErrorMessage = "Assimp not enabled";
    return result;
}

#endif
