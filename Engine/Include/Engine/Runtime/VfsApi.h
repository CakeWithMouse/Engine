#pragma once
#include <stdint.h>

// ============================================================================
// Engine VFS Service C-ABI Interface
// Windows x64, C calling convention, UTF-8 strings, no STL, no C++ exceptions.
// All buffer sizes and offsets use uint64_t.
// ============================================================================

#define ENGINE_SERVICE_VFS        0x53465601u // 'VFS' service ID
#define ENGINE_VFS_API_VERSION    1u

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EngineVfsStatus
{
    ENGINE_VFS_OK               = 0,
    ENGINE_VFS_NOT_FOUND        = 1,
    ENGINE_VFS_IO_ERROR         = 2,
    ENGINE_VFS_ALLOC_FAILED     = 3,
    ENGINE_VFS_BAD_ARGUMENT     = 4,
    ENGINE_VFS_NOT_SUPPORTED    = 5,
    ENGINE_VFS_BUFFER_TOO_SMALL = 6
} EngineVfsStatus;

typedef enum EngineVfsEntryType
{
    ENGINE_VFS_TYPE_FILE      = 1,
    ENGINE_VFS_TYPE_DIRECTORY = 2,
    ENGINE_VFS_TYPE_UNKNOWN   = 3
} EngineVfsEntryType;

struct EngineServiceHeader
{
    uint32_t version;
    uint32_t size;
};

struct EngineVfsStat
{
    uint32_t type;       // EngineVfsEntryType
    uint32_t reserved;   // 64-bit alignment padding
    uint64_t size;       // File size in bytes
    uint64_t mtime;      // Last modification time (UTC Unix epoch seconds)
};

// Allocator callback for file reading.
// Invoked synchronously by the host on the calling thread.
// Caller allocates memory of at least 'size' bytes and returns a pointer.
// If size == 0, returning nullptr or a non-null placeholder is valid.
// If memory allocation fails, returning nullptr causes readFile to abort and return ENGINE_VFS_ALLOC_FAILED.
// Host NEVER stores, reallocates, or frees this pointer.
typedef void* (*EngineVfsAllocFn)(void* allocUser, uint64_t size);

// Enumeration callback. Return 1 to continue iteration, 0 to abort early.
typedef int (*EngineVfsEnumerateCallback)(void* user, const char* name, uint32_t type, uint64_t size);

// Search callback. Return 1 to continue iteration, 0 to abort early.
typedef int (*EngineVfsFindCallback)(void* user, const char* vpath, uint32_t type, uint64_t size);

// Mount point enumeration callback. Return 1 to continue, 0 to abort.
typedef int (*EngineVfsMountCallback)(void* user, const char* vroot, const char* physicalPath);

// File change notification callback (Phase 4).
typedef void (*EngineVfsChangeCallback)(void* user, const char* vpath, uint32_t changeType);

struct EngineVfsApi
{
    // Service header: version must be >= ENGINE_VFS_API_VERSION, size >= sizeof(EngineVfsApi)
    struct EngineServiceHeader header;
    void* context;

    // --- Core Read & Inspection Operations (Phase 2) ---

    // Resolves a virtual path to a physical filesystem path.
    // Returns ENGINE_VFS_OK, ENGINE_VFS_NOT_FOUND, or ENGINE_VFS_BUFFER_TOO_SMALL.
    // If capacity is insufficient, *needed contains the required byte count including null terminator.
    EngineVfsStatus (*resolve)(void* ctx, const char* vpath, char* outPhysical, uint64_t capacity, uint64_t* needed);

    // Translates a physical path or relative path to a canonical virtual path.
    EngineVfsStatus (*toVirtual)(void* ctx, const char* physical, char* outVpath, uint64_t capacity, uint64_t* needed);

    // Checks if a virtual path exists. Returns 1 if exists, 0 if not or on invalid argument.
    int (*exists)(void* ctx, const char* vpath);

    // Queries file/directory metadata.
    EngineVfsStatus (*stat)(void* ctx, const char* vpath, struct EngineVfsStat* outStat);

    // Synchronously reads file bytes using the caller's allocator callback.
    // If outReadBytes is not null, receives the exact count of bytes read.
    EngineVfsStatus (*readFile)(void* ctx, const char* vpath, EngineVfsAllocFn allocFn, void* allocUser, uint64_t* outReadBytes);

    // Enumerates directory entries in vdir.
    EngineVfsStatus (*enumerate)(void* ctx, const char* vdir, int recursive, EngineVfsEnumerateCallback callback, void* user);

    // Searches directory entries matching glob/extension pattern.
    EngineVfsStatus (*find)(void* ctx, const char* vdir, uint32_t typeFilter, const char* pattern, int recursive, EngineVfsFindCallback callback, void* user);

    // Enumerates active mount points.
    EngineVfsStatus (*getMounts)(void* ctx, EngineVfsMountCallback callback, void* user);

    // --- Mutating Operations (Reserved for Phase 4; return ENGINE_VFS_NOT_SUPPORTED until implemented) ---

    EngineVfsStatus (*writeFile)(void* ctx, const char* vpath, const void* data, uint64_t size, int atomic);
    EngineVfsStatus (*createDir)(void* ctx, const char* vdir, int recursive);
    EngineVfsStatus (*moveFile)(void* ctx, const char* oldVpath, const char* newVpath);
    EngineVfsStatus (*copyFile)(void* ctx, const char* srcVpath, const char* dstVpath);
    EngineVfsStatus (*deleteFile)(void* ctx, const char* vpath);

    // --- Change Notification Subscriptions (Reserved for Phase 4; return ENGINE_VFS_NOT_SUPPORTED) ---

    EngineVfsStatus (*subscribe)(void* ctx, const char* vdirPrefix, EngineVfsChangeCallback callback, void* user, uint64_t* outToken);
    EngineVfsStatus (*unsubscribe)(void* ctx, uint64_t token);
};

#ifdef __cplusplus
}
#endif
