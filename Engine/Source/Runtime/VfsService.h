#pragma once
#include <Engine/Runtime/VfsApi.h>

class VirtualFileSystem;

EngineVfsApi CreateVfsService(VirtualFileSystem& vfs);
