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

        m_loggedStatusMask.store(0);
        m_api.store(api);
        std::printf("[VfsRead] Bind host VFS service v%u\n", api->header.version);
        return true;
    }

    void Unbind()
    {
        m_api.store(nullptr);
        m_loggedStatusMask.store(0);
        std::printf("[VfsRead] Unbind\n");
    }

    bool IsBound() const
    {
        return m_api.load() != nullptr;
    }

    bool Exists(std::string_view virtualPath) const override
    {
        const auto* api = m_api.load();
        if (!api || !api->exists)
        {
            return false;
        }
        std::string vpathStr(virtualPath);
        return api->exists(api->context, vpathStr.c_str()) != 0;
    }

    std::string Resolve(std::string_view vpath) const override
    {
        const auto* api = m_api.load();
        if (!api || !api->resolve)
        {
            return {};
        }

        std::string vpathStr(vpath);
        char stackBuf[512];
        uint64_t needed = 0;
        uint32_t status = api->resolve(api->context, vpathStr.c_str(), stackBuf, sizeof(stackBuf), &needed);
        if (status == ENGINE_VFS_OK)
        {
            return std::string(stackBuf);
        }

        if (status == ENGINE_VFS_BUFFER_TOO_SMALL && needed > 0)
        {
            std::vector<char> dynBuf(static_cast<size_t>(needed));
            status = api->resolve(api->context, vpathStr.c_str(), dynBuf.data(), dynBuf.size(), &needed);
            if (status == ENGINE_VFS_OK)
            {
                return std::string(dynBuf.data());
            }
        }

        return {};
    }

    std::string ToVirtualPath(std::string_view path) const override
    {
        const auto* api = m_api.load();
        if (!api || !api->toVirtual)
        {
            return std::string(path);
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

        LogHostFailureOnce(status, pathStr);
        return {};
    }

    std::vector<uint8_t> ReadBytes(std::string_view path, std::string* outVirtualPath = nullptr) const override
    {
        const auto* api = m_api.load();
        if (!api || !api->readFile)
        {
            return {};
        }

        const std::string vpath = ToVirtualPath(path);
        if (outVirtualPath)
        {
            *outVirtualPath = vpath;
        }
        if (vpath.empty())
        {
            return {};
        }

        std::vector<uint8_t> result;
        HostVfsDetail::AllocContext ctx;
        ctx.buffer = &result;
        ctx.simulateAllocFailure = m_simulateAllocFailure.load();

        uint64_t bytesRead = 0;
        const uint32_t status = api->readFile(api->context, vpath.c_str(), HostVfsDetail::ProxyAllocFn, &ctx, &bytesRead);
        if (status != ENGINE_VFS_OK)
        {
            LogHostFailureOnce(status, vpath);
            return {};
        }

        return result;
    }

    // testing only
    void SetSimulateAllocFailureForTesting(bool simulate)
    {
        m_simulateAllocFailure.store(simulate);
    }

    // testing only
    bool IsSimulateAllocFailureForTesting() const
    {
        return m_simulateAllocFailure.load();
    }

private:
    HostVfsProxy() = default;

    void LogHostFailureOnce(uint32_t status, std::string_view path) const
    {
        if (status < 32)
        {
            const uint32_t bit = (1u << status);
            const uint32_t prev = m_loggedStatusMask.fetch_or(bit);
            if (!(prev & bit))
            {
                std::fprintf(stderr, "[VfsRead] host service failed: %u for %.*s\n",
                    status, static_cast<int>(path.size()), path.data());
            }
        }
        else
        {
            static std::atomic<bool> s_otherLogged{false};
            bool expected = false;
            if (s_otherLogged.compare_exchange_strong(expected, true))
            {
                std::fprintf(stderr, "[VfsRead] host service failed: %u for %.*s\n",
                    status, static_cast<int>(path.size()), path.data());
            }
        }
    }

    std::atomic<const EngineVfsApi*> m_api{nullptr};
    mutable std::atomic<uint32_t> m_loggedStatusMask{0};
    std::atomic<bool> m_simulateAllocFailure{false};
};
