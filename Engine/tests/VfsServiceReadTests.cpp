#include "VfsServiceTests.h"
#include "VfsServiceFixture.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

void VfsService_ReadFileAndAllocator()
{
    VfsTestFixture f;
    CHECK(f.api.readFile != nullptr);

    struct AllocContext
    {
        std::vector<uint8_t> buffer;
        bool allocCalled = false;
        bool failAlloc = false;
    } allocCtx;

    auto allocFn = [](void* user, uint64_t size) -> void* {
        auto* c = static_cast<AllocContext*>(user);
        c->allocCalled = true;
        if (c->failAlloc) return nullptr;
        c->buffer.resize(static_cast<size_t>(size));
        return c->buffer.empty() ? nullptr : c->buffer.data();
    };

    // normal file read
    uint64_t bytesRead = 0;
    uint32_t status = f.api.readFile(f.api.context, "/Test/hello.txt", allocFn, &allocCtx, &bytesRead);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(allocCtx.allocCalled);
    CHECK(bytesRead == 12);
    CHECK(allocCtx.buffer.size() == 12);
    CHECK(std::string(reinterpret_cast<const char*>(allocCtx.buffer.data()), 12) == "Hello World!");

    // 0-byte file read (empty.bin)
    allocCtx.allocCalled = false;
    allocCtx.buffer.clear();
    bytesRead = 999;
    status = f.api.readFile(f.api.context, "/Test/empty.bin", allocFn, &allocCtx, &bytesRead);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(bytesRead == 0);

    // nonexistent file
    allocCtx.allocCalled = false;
    status = f.api.readFile(f.api.context, "/Test/missing.bin", allocFn, &allocCtx, &bytesRead);
    CHECK(status == ENGINE_VFS_NOT_FOUND);
    CHECK(!allocCtx.allocCalled);

    // allocator failure returns ENGINE_VFS_ALLOC_FAILED
    allocCtx.allocCalled = false;
    allocCtx.failAlloc = true;
    status = f.api.readFile(f.api.context, "/Test/hello.txt", allocFn, &allocCtx, &bytesRead);
    CHECK(status == ENGINE_VFS_ALLOC_FAILED);
    CHECK(allocCtx.allocCalled);

    // bad argument checks
    status = f.api.readFile(f.api.context, nullptr, allocFn, &allocCtx, &bytesRead);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);
    status = f.api.readFile(f.api.context, "/Test/hello.txt", nullptr, &allocCtx, &bytesRead);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);

    // invalid UTF-8
    const char invalidUtf8[] = { '\xFF', '\xFE', '\xFD', '\0' };
    status = f.api.readFile(f.api.context, invalidUtf8, allocFn, &allocCtx, &bytesRead);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);
}

void VfsService_EnumerateAndFind()
{
    VfsTestFixture f;
    CHECK(f.api.enumerate != nullptr);
    CHECK(f.api.find != nullptr);

    // enumerate non-recursive
    struct EnumResult
    {
        std::vector<std::string> names;
        std::vector<uint32_t> types;
    } enumRes;

    auto enumCb = [](void* user, const char* name, uint32_t type, uint64_t /*size*/) -> int {
        auto* r = static_cast<EnumResult*>(user);
        r->names.push_back(name ? name : "");
        r->types.push_back(type);
        return 1;
    };

    uint32_t status = f.api.enumerate(f.api.context, "/Test", 0, enumCb, &enumRes);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(!enumRes.names.empty());

    bool foundHello = false;
    for (size_t i = 0; i < enumRes.names.size(); ++i)
    {
        if (enumRes.names[i].find("hello.txt") != std::string::npos && enumRes.types[i] == ENGINE_VFS_TYPE_FILE) foundHello = true;
    }
    CHECK(foundHello);

    // enumerate early abort (stop after 1 item)
    enumRes.names.clear();
    auto abortCb = [](void* user, const char* name, uint32_t /*type*/, uint64_t /*size*/) -> int {
        auto* r = static_cast<EnumResult*>(user);
        r->names.push_back(name ? name : "");
        return 0; // abort immediately
    };
    status = f.api.enumerate(f.api.context, "/Test", 0, abortCb, &enumRes);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(enumRes.names.size() == 1);

    // enumerate nonexistent directory
    status = f.api.enumerate(f.api.context, "/Test/nonexistent_folder", 0, enumCb, &enumRes);
    CHECK(status == ENGINE_VFS_NOT_FOUND);

    // enumerate invalid UTF-8
    const char invalidUtf8[] = { '\xFF', '\xFE', '\xFD', '\0' };
    status = f.api.enumerate(f.api.context, invalidUtf8, 0, enumCb, &enumRes);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);

    // find recursive pattern "*.txt"
    struct FindResult
    {
        std::vector<std::string> vpaths;
    } findRes;

    auto findCb = [](void* user, const char* vpath, uint32_t /*type*/, uint64_t /*size*/) -> int {
        auto* r = static_cast<FindResult*>(user);
        r->vpaths.push_back(vpath ? vpath : "");
        return 1;
    };

    status = f.api.find(f.api.context, "/Test", 0, "*.txt", 1, findCb, &findRes);
    CHECK(status == ENGINE_VFS_OK);
    bool foundDeep = false;
    for (const auto& p : findRes.vpaths)
    {
        if (p.find("deep.txt") != std::string::npos) foundDeep = true;
    }
    CHECK(foundDeep);

    // find early abort
    findRes.vpaths.clear();
    auto findAbortCb = [](void* user, const char* vpath, uint32_t /*type*/, uint64_t /*size*/) -> int {
        auto* r = static_cast<FindResult*>(user);
        r->vpaths.push_back(vpath ? vpath : "");
        return 0;
    };
    status = f.api.find(f.api.context, "/Test", 0, "*.txt", 1, findAbortCb, &findRes);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(findRes.vpaths.size() == 1);

    // find nonexistent directory
    status = f.api.find(f.api.context, "/Test/nonexistent_folder", 0, "*.txt", 1, findCb, &findRes);
    CHECK(status == ENGINE_VFS_NOT_FOUND);

    // find invalid UTF-8
    status = f.api.find(f.api.context, invalidUtf8, 0, "*.txt", 1, findCb, &findRes);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);
    status = f.api.find(f.api.context, "/Test", 0, invalidUtf8, 1, findCb, &findRes);
    CHECK(status == ENGINE_VFS_BAD_ARGUMENT);
}

void VfsService_CallbackReentrancyAndNoDeadlock()
{
    VfsTestFixture f;
    CHECK(f.api.enumerate != nullptr);
    CHECK(f.api.find != nullptr);
    CHECK(f.api.exists != nullptr);
    CHECK(f.api.stat != nullptr);
    CHECK(f.api.resolve != nullptr);

    // Create 10 files to verify multiple successive callback iterations
    for (int i = 0; i < 10; ++i)
    {
        std::ofstream(f.root / ("reentrant_" + std::to_string(i) + ".dat")) << "Reentrant content " << i;
    }

    struct ReentrantContext
    {
        EngineVfsApi* api = nullptr;
        int callbackCount = 0;
        int statSuccessCount = 0;
        int existsSuccessCount = 0;
        int resolveSuccessCount = 0;
    } reCtx;
    reCtx.api = &f.api;

    // Callback during enumerate invokes exists, stat, resolve re-entrantly on each item
    auto reentrantEnumCb = [](void* user, const char* name, uint32_t type, uint64_t /*size*/) -> int {
        auto* c = static_cast<ReentrantContext*>(user);
        c->callbackCount++;

        const char* fullVpath = name;

        // Re-entrant call 1: exists
        if (c->api->exists(c->api->context, fullVpath) == 1)
        {
            c->existsSuccessCount++;
        }

        // Re-entrant call 2: stat
        EngineVfsStat st{};
        if (c->api->stat(c->api->context, fullVpath, &st) == ENGINE_VFS_OK)
        {
            if (st.type == type)
            {
                c->statSuccessCount++;
            }
        }

        // Re-entrant call 3: resolve
        char physBuf[512]{};
        uint64_t needed = 0;
        if (c->api->resolve(c->api->context, fullVpath, physBuf, sizeof(physBuf), &needed) == ENGINE_VFS_OK)
        {
            c->resolveSuccessCount++;
        }

        return 1; // continue iteration
    };

    uint32_t status = f.api.enumerate(f.api.context, "/Test", 0, reentrantEnumCb, &reCtx);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(reCtx.callbackCount > 10);
    CHECK(reCtx.existsSuccessCount == reCtx.callbackCount);
    CHECK(reCtx.statSuccessCount == reCtx.callbackCount);
    CHECK(reCtx.resolveSuccessCount == reCtx.callbackCount);

    // Callback during find invokes exists, stat, readFile re-entrantly on each match
    struct FindReentrantContext
    {
        EngineVfsApi* api = nullptr;
        int matchCount = 0;
        int readSuccessCount = 0;
    } findReCtx;
    findReCtx.api = &f.api;

    auto reentrantFindCb = [](void* user, const char* vpath, uint32_t /*type*/, uint64_t /*size*/) -> int {
        auto* c = static_cast<FindReentrantContext*>(user);
        c->matchCount++;

        // Re-entrant call: readFile
        struct ReadCtx
        {
            std::vector<uint8_t> bytes;
        } readCtx;

        auto alloc = [](void* u, uint64_t sz) -> void* {
            auto* rc = static_cast<ReadCtx*>(u);
            rc->bytes.resize(static_cast<size_t>(sz));
            return rc->bytes.empty() ? nullptr : rc->bytes.data();
        };

        uint64_t bytesRead = 0;
        if (c->api->readFile(c->api->context, vpath, alloc, &readCtx, &bytesRead) == ENGINE_VFS_OK)
        {
            if (bytesRead > 0)
            {
                c->readSuccessCount++;
            }
        }
        return 1;
    };

    status = f.api.find(f.api.context, "/Test", ENGINE_VFS_TYPE_FILE, "reentrant_*.dat", 0, reentrantFindCb, &findReCtx);
    CHECK(status == ENGINE_VFS_OK);
    CHECK(findReCtx.matchCount == 10);
    CHECK(findReCtx.readSuccessCount == 10);
}
