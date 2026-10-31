#pragma once
#include <Engine/Runtime/VfsApi.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include "../Source/Runtime/VfsService.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)

struct VfsTestFixture
{
    std::filesystem::path root;
    VirtualFileSystem vfs;
    EngineVfsApi api;

    VfsTestFixture()
    {
        std::error_code ec;
        root = std::filesystem::temp_directory_path() / "Engine_VfsServiceTests";
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root / "dir1" / "nested", ec);
        std::filesystem::create_directories(root / "dir2", ec);

        std::ofstream(root / "hello.txt") << "Hello World!";
        std::ofstream(root / "empty.bin"); // 0 bytes
        std::ofstream(root / "dir1" / "file1.txt") << "Content 1";
        std::ofstream(root / "dir1" / "data.bin") << "Binary data";
        std::ofstream(root / "dir1" / "nested" / "deep.txt") << "Deep text";

        vfs.Mount("/Test", root);
        api = CreateVfsService(vfs);
    }

    ~VfsTestFixture()
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};
