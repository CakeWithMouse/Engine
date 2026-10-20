#pragma once

#include "IVfs.h"
#include <Engine/Runtime/VfsApi.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace HostVfsDetail
{
struct AllocContext
{
    std::vector<uint8_t>* buffer = nullptr;
    bool simulateAllocFailure = false;
};

inline void* ENGINE_MODULE_CALL ProxyAllocFn(void* allocUser, uint64_t size) noexcept
{
    try
    {
        auto* ctx = static_cast<AllocContext*>(allocUser);
        if (!ctx) return nullptr;
        if (ctx->simulateAllocFailure)
        {
            throw std::bad_alloc();
        }
        if (size == 0)
        {
            return nullptr;
        }
        if (!ctx->buffer)
        {
            return nullptr;
        }
        ctx->buffer->resize(static_cast<size_t>(size));
        return ctx->buffer->data();
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
    catch (...)
    {
        return nullptr;
    }
}
} // namespace HostVfsDetail

class HostVfsProxy : public IVfs
{
public:
    static HostVfsProxy& Instance()
    {
        static HostVfsProxy s_instance;
        return s_instance;
    }

    bool Bind(const EngineVfsApi* api)
    {
        if (!api)
        {
            std::fprintf(stderr, "[VfsRead] Bind rejected: null api pointer\n");
            return false;
        }
        if (api->header.version < ENGINE_VFS_API_VERSION)
        {
            std::fprintf(stderr, "[VfsRead] Bind rejected: incompatible version %u (expected >= %u)\n",
                api->header.version, ENGINE_VFS_API_VERSION);
            return false;
        }
        if (api->header.size < sizeof(EngineVfsApi))
        {
            std::fprintf(stderr, "[VfsRead] Bind rejected: invalid size %llu (expected >= %zu)\n",
                static_cast<unsigned long long>(api->header.size), sizeof(EngineVfsApi));
            return false;
        }

        m_api.store(api);
        std::printf("[VfsRead] Bind host VFS service v%u\n", api->header.version);
        return true;
    }

    void Unbind()
    {
        m_api.store(nullptr);
        std::printf("[VfsRead] Unbind\n");
    }

    bool IsBound() const
    {
        return m_api.load() != nullptr;
    }

    void SetFallbackVfs(IVfs* fallback)
    {
        m_fallback.store(fallback);
    }

    IVfs* GetFallbackVfs() const
    {
        return m_fallback.load();
    }

    bool Exists(std::string_view virtualPath) const override
    {
        const auto* api = m_api.load();
        if (!api || !api->exists)
        {
            auto* fb = m_fallback.load();
            return fb ? fb->Exists(virtualPath) : false;
        }
        std::string vpathStr(virtualPath);
        return api->exists(api->context, vpathStr.c_str()) != 0;
    }

    std::string ToVirtualPath(std::string_view path) const override
    {
        const auto* api = m_api.load();
        if (!api || !api->toVirtual)
        {
            auto* fb = m_fallback.load();
            return fb ? fb->ToVirtualPath(path) : std::string(path);
        }

        std::string pathStr(path);
        char stackBuf[512];
        uint64_t needed = 0;
        uint32_t status = api->toVirtual(api->context, pathStr.c_str(), stackBuf, sizeof(stackBuf), &needed);
        if (status == ENGINE_VFS_OK)
        {
            return std::string(stackBuf);
        }
        if (status == ENGINE_VFS_BUFFER_TOO_SMALL && needed > 0)
        {
            std::vector<char> dynBuf(static_cast<size_t>(needed));
            status = api->toVirtual(api->context, pathStr.c_str(), dynBuf.data(), needed, &needed);
            if (status == ENGINE_VFS_OK)
            {
                return std::string(dynBuf.data());
            }
        }

        auto* fb = m_fallback.load();
        return fb ? fb->ToVirtualPath(path) : std::string(path);
    }

    std::vector<uint8_t> ReadBytes(std::string_view path, std::string* outVirtualPath = nullptr) const override
    {
        const auto* api = m_api.load();
        if (!api || !api->readFile)
        {
            auto* fb = m_fallback.load();
            return fb ? fb->ReadBytes(path, outVirtualPath) : std::vector<uint8_t>{};
        }

        const std::string vpath = ToVirtualPath(path);
        if (outVirtualPath)
        {
            *outVirtualPath = vpath;
        }

        std::vector<uint8_t> result;
        HostVfsDetail::AllocContext ctx;
        ctx.buffer = &result;
        ctx.simulateAllocFailure = m_simulateAllocFailure.load();

        uint64_t bytesRead = 0;
        const uint32_t status = api->readFile(api->context, vpath.c_str(), HostVfsDetail::ProxyAllocFn, &ctx, &bytesRead);
        if (status != ENGINE_VFS_OK)
        {
            return {};
        }

        return result;
    }

    // Test hook for simulated allocator failure
    void SetSimulateAllocFailureForTesting(bool simulate)
    {
        m_simulateAllocFailure.store(simulate);
    }

    bool IsSimulateAllocFailureForTesting() const
    {
        return m_simulateAllocFailure.load();
    }

private:
    HostVfsProxy() = default;

    std::atomic<const EngineVfsApi*> m_api{nullptr};
    std::atomic<IVfs*> m_fallback{nullptr};
    std::atomic<bool> m_simulateAllocFailure{false};
};
