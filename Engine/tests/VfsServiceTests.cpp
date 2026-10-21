#include "VfsServiceTests.h"
#include "VfsServiceFixture.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

void VfsService_ResolveAndToVirtual()
{
    VfsTestFixture f;
    CHECK(f.api.header.version >= ENGINE_VFS_API_VERSION);
    CHECK(f.api.header.size >= sizeof(EngineVfsApi));
    CHECK(f.api.resolve != nullptr);
    CHECK(f.api.toVirtual != nullptr);

    // resolve: buffer sizing with capacity = 0
    uint64_t needed = 0;
    uint32_t status = f.api.resolve(f.api.context, "/Test/hello.txt", nullptr, 0, &needed);
    CHECK(status == ENGINE_VFS_BUFFER_TOO_SMALL);
    CHECK(needed > 0);

    // resolve: normal buffer
    std::vector<char> buf(static_cast<size_t>(needed));
    uint64_t actualNeeded = 0;
    status = f.api.resolve(f.api.context, "/Test/hello.txt", buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(actualNeeded == needed);
    const std::filesystem::path resolvedPath(buf.data());
    CHECK(std::filesystem::exists(resolvedPath));
    CHECK(resolvedPath.filename() == "hello.txt");

    // resolve: nonexistent (unmounted root)
    status = f.api.resolve(f.api.context, "/UnmountedRoot/nonexistent.txt", buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_NOT_FOUND);

    // resolve: bad argument
    status = f.api.resolve(f.api.context, nullptr, buf.data(), buf.size(), nullptr);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);

    // resolve: invalid UTF-8
    const char invalidUtf8[] = { '\xFF', '\xFE', '\xFD', '\0' };
    status = f.api.resolve(f.api.context, invalidUtf8, buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);

    // toVirtual: normal
    const std::string physicalStr = (f.root / "hello.txt").string();
    needed = 0;
    status = f.api.toVirtual(f.api.context, physicalStr.c_str(), nullptr, 0, &needed);
    CHECK(status == ENGINE_VFS_BUFFER_TOO_SMALL);
    CHECK(needed > 0);

    buf.resize(static_cast<size_t>(needed));
    status = f.api.toVirtual(f.api.context, physicalStr.c_str(), buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(std::string(buf.data()) == "/Test/hello.txt");

    // toVirtual: casing (c: vs C:) and backslashes
    std::string lowerPath = physicalStr;
    if (lowerPath.size() > 1 && lowerPath[1] == ':')
    {
        lowerPath[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerPath[0])));
    }
    for (char& c : lowerPath) { if (c == '/') c = '\\'; }

    std::string upperPath = physicalStr;
    if (upperPath.size() > 1 && upperPath[1] == ':')
    {
        upperPath[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(upperPath[0])));
    }
    for (char& c : upperPath) { if (c == '\\') c = '/'; }

    status = f.api.toVirtual(f.api.context, lowerPath.c_str(), buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(std::string(buf.data()) == "/Test/hello.txt");

    status = f.api.toVirtual(f.api.context, upperPath.c_str(), buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(std::string(buf.data()) == "/Test/hello.txt");

    // toVirtual: physical path outside mounts -> NOT_FOUND without adding dynamic mounts
    const size_t mountsBefore = f.vfs.GetMounts().size();
    const auto outsidePath = (std::filesystem::temp_directory_path() / "OutsideVfs_Random_NonExistent.xyz").string();
    status = f.api.toVirtual(f.api.context, outsidePath.c_str(), buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_NOT_FOUND);
    const size_t mountsAfter = f.vfs.GetMounts().size();
    CHECK(mountsBefore == mountsAfter);

    // toVirtual: path containing standard marker substring outside mounts -> NOT_FOUND (no heuristics)
    const auto markerPath = (std::filesystem::temp_directory_path() / "EngineContent" / "Shaders" / "RotatedFigure.hlsl").string();
    status = f.api.toVirtual(f.api.context, markerPath.c_str(), buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_NOT_FOUND);
    CHECK(mountsBefore == f.vfs.GetMounts().size());

    // toVirtual: bad arguments
    status = f.api.toVirtual(f.api.context, nullptr, buf.data(), buf.size(), nullptr);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);

    // toVirtual: invalid UTF-8
    status = f.api.toVirtual(f.api.context, invalidUtf8, buf.data(), buf.size(), &actualNeeded);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);
}

void VfsService_ExistsAndStat()
{
    VfsTestFixture f;
    CHECK(f.api.exists != nullptr);
    CHECK(f.api.stat != nullptr);

    // exists
    CHECK(f.api.exists(f.api.context, "/Test/hello.txt") == 1);
    CHECK(f.api.exists(f.api.context, "/Test/dir1") == 1);
    CHECK(f.api.exists(f.api.context, "/Test/dir1/nested/deep.txt") == 1);
    CHECK(f.api.exists(f.api.context, "/Test/missing.txt") == 0);
    CHECK(f.api.exists(f.api.context, nullptr) == 0);

    // exists: invalid UTF-8
    const char invalidUtf8[] = { '\xFF', '\xFE', '\xFD', '\0' };
    CHECK(f.api.exists(f.api.context, invalidUtf8) == 0);

    // stat: file
    EngineVfsStat st{};
    uint32_t status = f.api.stat(f.api.context, "/Test/hello.txt", &st);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(st.type == ENGINE_VFS_TYPE_FILE);
    CHECK(st.size == 12); // "Hello World!"
    CHECK(st.mtime > 0);

    // stat: directory
    std::memset(&st, 0, sizeof(st));
    status = f.api.stat(f.api.context, "/Test/dir1", &st);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(st.type == ENGINE_VFS_TYPE_DIRECTORY);

    // stat: nonexistent
    status = f.api.stat(f.api.context, "/Test/missing.txt", &st);
    CHECK(status == ENGINE_VFS_NOT_FOUND);

    // stat: bad arguments
    status = f.api.stat(f.api.context, nullptr, &st);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);
    status = f.api.stat(f.api.context, "/Test/hello.txt", nullptr);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);

    // stat: invalid UTF-8
    status = f.api.stat(f.api.context, invalidUtf8, &st);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);
}

void VfsService_GetMountsAndStubs()
{
    VfsTestFixture f;
    CHECK(f.api.getMounts != nullptr);

    // getMounts
    struct MountResult
    {
        std::vector<std::pair<std::string, std::string>> mounts;
    } mRes;

    auto mountCb = [](void* user, const char* vroot, const char* physical) -> int {
        auto* r = static_cast<MountResult*>(user);
        r->mounts.emplace_back(vroot ? vroot : "", physical ? physical : "");
        return 1;
    };

    uint32_t status = f.api.getMounts(f.api.context, mountCb, &mRes);
    CHECK(status == ENGINE_VFS_OK);
    bool foundTestMount = false;
    for (const auto& m : mRes.mounts)
    {
        if (m.first == "/Test") foundTestMount = true;
    }
    CHECK(foundTestMount);

    // getMounts early abort
    mRes.mounts.clear();
    auto mountAbortCb = [](void* user, const char* vroot, const char* physical) -> int {
        auto* r = static_cast<MountResult*>(user);
        r->mounts.emplace_back(vroot ? vroot : "", physical ? physical : "");
        return 0; // stop immediately
    };
    status = f.api.getMounts(f.api.context, mountAbortCb, &mRes);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(mRes.mounts.size() == 1);

    // getMounts null callback
    status = f.api.getMounts(f.api.context, nullptr, nullptr);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);

    // Unsupported stubs (reserved for Phase 4)
    CHECK(f.api.writeFile(f.api.context, "/Test/x.txt", "abc", 3, 1) == ENGINE_VFS_NOT_SUPPORTED);
    CHECK(f.api.createDir(f.api.context, "/Test/newdir", 1) == ENGINE_VFS_NOT_SUPPORTED);
    CHECK(f.api.moveFile(f.api.context, "/Test/a", "/Test/b") == ENGINE_VFS_NOT_SUPPORTED);
    CHECK(f.api.copyFile(f.api.context, "/Test/a", "/Test/b") == ENGINE_VFS_NOT_SUPPORTED);
    CHECK(f.api.deleteFile(f.api.context, "/Test/a") == ENGINE_VFS_NOT_SUPPORTED);

    uint64_t token = 0;
    CHECK(f.api.subscribe(f.api.context, "/Test", nullptr, nullptr, &token) == ENGINE_VFS_NOT_SUPPORTED);
    CHECK(f.api.unsubscribe(f.api.context, token) == ENGINE_VFS_NOT_SUPPORTED);
}
