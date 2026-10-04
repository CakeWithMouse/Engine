#include <Engine/Base/Config/BaseResources.h>

#include <Windows.h>

std::filesystem::path BaseResources::FindProjectDirectory()
{
    return EnginePaths::ContentRoot();
}

std::string BaseResources::ResolveContentRoot()
{
    return EnginePaths::ContentRoot().generic_u8string();
}

std::wstring BaseResources::ReadEnvironmentVariable(const wchar_t* name)
{
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (length == 0)
    {
        return std::wstring();
    }
    std::wstring value(length, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), length);
    value.resize(written);
    return value;
}
