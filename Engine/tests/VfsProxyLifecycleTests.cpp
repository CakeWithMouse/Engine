#include "VfsProxyLifecycleTests.h"
#include "VfsServiceFixture.h"
#include "../Source/Resources/HostVfsProxy.h"
#include "../Source/Resources/LocalVfs.h"
#include "../Source/Resources/VfsRead.h"
#include "../Source/Runtime/VfsService.h"

#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Runtime/VfsApi.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

void VfsProxyLifecycle_SessionsAndRejection()
{
    // Ensure clean initial state
    VfsRead::Unbind();

    // 1. Initial access without host service falls back to LocalVfs and logs once
    std::stringstream capture;
    auto* oldCoutBuf = std::cout.rdbuf(capture.rdbuf());

    // We also redirect stdout via freopen/printf or check std::cout
    // Note: VfsRead uses std::printf for logging
    // Test that Active() returns LocalVfs
    IVfs& activeInitial = VfsRead::Active();
    CHECK(&activeInitial == &LocalVfs::Instance());

    // Active() called again must still be LocalVfs
    IVfs& activeSecond = VfsRead::Active();
    CHECK(&activeSecond == &LocalVfs::Instance());

    std::cout.rdbuf(oldCoutBuf);

    // 2. Bind with invalid arguments is rejected
    VirtualFileSystem testVfs;
    EngineVfsApi badApi = CreateVfsService(testVfs);

    // Null pointer rejection
    CHECK(!VfsRead::Bind(nullptr));
    CHECK(&VfsRead::Active() == &LocalVfs::Instance());

    // Incompatible version rejection
    badApi.header.version = 0;
    CHECK(!VfsRead::Bind(&badApi));
    CHECK(&VfsRead::Active() == &LocalVfs::Instance());

    // Invalid size rejection
    badApi.header.version = ENGINE_VFS_API_VERSION;
    badApi.header.size = sizeof(EngineServiceHeader);
    CHECK(!VfsRead::Bind(&badApi));
    CHECK(&VfsRead::Active() == &LocalVfs::Instance());

    // 3. Two sessions in a row
    // Session 1:
    EngineVfsApi api1 = CreateVfsService(testVfs);
    CHECK(VfsRead::Bind(&api1));
    CHECK(VfsRead::IsBound());
    CHECK(&VfsRead::Active() == &HostVfsProxy::Instance());

    VfsRead::Unbind();
    CHECK(!VfsRead::IsBound());
    CHECK(&VfsRead::Active() == &LocalVfs::Instance());

    // Session 2:
    EngineVfsApi api2 = CreateVfsService(testVfs);
    CHECK(VfsRead::Bind(&api2));
    CHECK(VfsRead::IsBound());
    CHECK(&VfsRead::Active() == &HostVfsProxy::Instance());

    VfsRead::Unbind();
    CHECK(!VfsRead::IsBound());
    CHECK(&VfsRead::Active() == &LocalVfs::Instance());
}

void VfsProxyLifecycle_ReadParityAndAllocator()
{
    // Ensure clean state
    VfsRead::Unbind();

    // Create temporary directory with test files
    const auto fixtureDir = std::filesystem::temp_directory_path() / "vfs_proxy_read_fixture";
    std::filesystem::create_directories(fixtureDir);

    struct Cleaner
    {
        std::filesystem::path dir;
        ~Cleaner()
        {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
    } cleaner{fixtureDir};

    // 1. Text file
    const std::string textContent = "Hello from VfsProxyLifecycle parity test!";
    {
        std::ofstream f(fixtureDir / "text.txt", std::ios::binary);
        f.write(textContent.data(), static_cast<std::streamsize>(textContent.size()));
    }

    // 2. Zero-length file
    {
        std::ofstream f(fixtureDir / "empty.bin", std::ios::binary);
    }

    VirtualFileSystem testVfs;
    testVfs.Mount("/ProxyParity", fixtureDir);

    EngineVfsApi api = CreateVfsService(testVfs);
    CHECK(VfsRead::Bind(&api));
    CHECK(VfsRead::IsBound());

    // Check 1: Normal file read parity (proxy vs direct)
    const std::vector<uint8_t> directText = testVfs.ReadBytes("/ProxyParity/text.txt");
    const std::vector<uint8_t> proxyText = VfsRead::Active().ReadBytes("/ProxyParity/text.txt");
    CHECK(!directText.empty());
    CHECK(directText == proxyText);

    // Check 2: Zero-length file read parity (proxy vs direct)
    const std::vector<uint8_t> directEmpty = testVfs.ReadBytes("/ProxyParity/empty.bin");
    const std::vector<uint8_t> proxyEmpty = VfsRead::Active().ReadBytes("/ProxyParity/empty.bin");
    CHECK(directEmpty.empty());
    CHECK(proxyEmpty.empty());
    CHECK(directEmpty == proxyEmpty);

    // Check 3: Non-existent file read
    bool directCaught = false;
    try
    {
        testVfs.ReadBytes("/ProxyParity/missing.bin");
    }
    catch (...)
    {
        directCaught = true;
    }
    CHECK(directCaught);

    const std::vector<uint8_t> proxyMissing = VfsRead::Active().ReadBytes("/ProxyParity/missing.bin");
    CHECK(proxyMissing.empty());

    // Check 4: Simulated allocator failure gives empty result without exception escaping boundary
    HostVfsProxy::Instance().SetSimulateAllocFailureForTesting(true);
    std::vector<uint8_t> failedBytes;
    bool exceptionEscaped = false;
    try
    {
        failedBytes = VfsRead::Active().ReadBytes("/ProxyParity/text.txt");
    }
    catch (...)
    {
        exceptionEscaped = true;
    }
    CHECK(!exceptionEscaped);
    CHECK(failedBytes.empty());

    // Restore allocator and verify read succeeds again
    HostVfsProxy::Instance().SetSimulateAllocFailureForTesting(false);
    const std::vector<uint8_t> restoredBytes = VfsRead::Active().ReadBytes("/ProxyParity/text.txt");
    CHECK(restoredBytes == directText);

    VfsRead::Unbind();
    CHECK(!VfsRead::IsBound());
    CHECK(&VfsRead::Active() == &LocalVfs::Instance());
}
