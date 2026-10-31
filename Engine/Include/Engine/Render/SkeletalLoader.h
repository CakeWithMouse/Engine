#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <Engine/Render/SkeletalData.h>

struct SkeletalLoadResult
{
    bool Success = false;
    std::string ErrorMessage;
    std::shared_ptr<SkeletalModel> Model;
};

/**
 * Loads a skeletal model from raw memory bytes (e.g. GLB).
 * Does not throw exceptions across module boundaries.
 *
 * If Assimp is not enabled during build, returns Success=false and ErrorMessage="Assimp not enabled".
 */
SkeletalLoadResult LoadSkeletalModelFromMemory(const void* data, size_t size, std::string_view hint = "glb");

/**
 * Loads a skeletal model via VFS using VfsRead::Active().ReadBytes(vpath).
 */
SkeletalLoadResult LoadSkeletalModelFromVfs(std::string_view vpath);
