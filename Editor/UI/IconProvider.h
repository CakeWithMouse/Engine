#pragma once
#include <cstdint>

class IEditorHost;

class IconProvider
{
public:
    static void UploadIcons(IEditorHost* host, bool force = false);
};

