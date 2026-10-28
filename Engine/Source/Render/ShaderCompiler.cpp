#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <Engine/Render/ShaderCompiler.h>
#include "../Resources/VfsRead.h"

#include <algorithm>
#include <cwctype>
#include <d3dcompiler.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path FromUtf8(const std::string& path) { return std::filesystem::u8path(path); }

    /** Physical path of a file read via VFS, for dependencies and messages; the virtual one if unknown. */
    std::string PhysicalPath(const std::string& virtualPath)
    {
        const std::string physical = VfsRead::Active().Resolve(virtualPath);
        return physical.empty() ? virtualPath : physical;
    }

    /** "Inc/Common.hlsli(3,5): error ..." -> "C:/.../Inc/Common.hlsli(3,5): error ...". */
    std::string ResolveMessagePaths(const std::string& messages, const std::map<std::string, std::string>& openedAs)
    {
        std::string result;
        size_t lineStart = 0;
        while (lineStart < messages.size())
        {
            size_t lineEnd = messages.find('\n', lineStart);
            lineEnd = lineEnd == std::string::npos ? messages.size() : lineEnd + 1;
            const std::string line = messages.substr(lineStart, lineEnd - lineStart);
            const size_t location = line.find('(');
            const auto file = location == std::string::npos ? openedAs.end() : openedAs.find(line.substr(0, location));
            result += file == openedAs.end() ? line : file->second + line.substr(location);
            lineStart = lineEnd;
        }
        return result;
    }
}

namespace ShaderCompiler
{
    unsigned int GetCompileFlags()
    {
#ifdef _DEBUG
        return D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        return D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    }

    const char* GetPolicyTag()
    {
#ifdef _DEBUG
        return "debug";
#else
        return "optimized";
#endif
    }

    /** Resolves includes via VFS, applies include overrides and records every opened file. */
    class VfsShaderInclude : public ID3DInclude
    {
    public:
        VfsShaderInclude(std::string baseVirtualDirectory, const std::map<std::string, std::string>& includeOverrides)
            : baseDir(std::move(baseVirtualDirectory)), overrides(includeOverrides)
        {
        }

        HRESULT __stdcall Open(D3D_INCLUDE_TYPE includeType,
                               LPCSTR pFileName,
                               LPCVOID pParentData,
                               LPCVOID* ppData,
                               UINT* pBytes) override
        {
            if (!pFileName || !ppData || !pBytes)
            {
                return E_INVALIDARG;
            }

            auto& vfs = VfsRead::Active();
            std::string targetVirtual;
            const auto overridden = overrides.find(pFileName);
            if (overridden != overrides.end())
            {
                targetVirtual = vfs.ToVirtualPath(overridden->second);
            }
            else
            {
                std::string parentDir = baseDir;
                if (pParentData != nullptr)
                {
                    auto it = parentDirs.find(pParentData);
                    if (it != parentDirs.end())
                    {
                        parentDir = it->second;
                    }
                }

                // Standard D3D include semantics:
                // 1. Search relative to the file containing the #include
                targetVirtual = VirtualFileSystem::NormalizeVirtualPath(parentDir + "/" + pFileName);
                if (!vfs.Exists(targetVirtual))
                {
                    // 2. If not found and parentDir != baseDir, search relative to the root shader directory
                    if (parentDir != baseDir)
                    {
                        const std::string rootVirtual = VirtualFileSystem::NormalizeVirtualPath(baseDir + "/" + pFileName);
                        if (vfs.Exists(rootVirtual))
                        {
                            targetVirtual = rootVirtual;
                        }
                        else
                        {
                            std::cout << "[ShaderCompiler] Include file not found in VFS: '" << pFileName
                                      << "' (searched in '" << parentDir << "' and '" << baseDir << "')" << std::endl;
                            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                        }
                    }
                    else
                    {
                        std::cout << "[ShaderCompiler] Include file not found in VFS: '" << pFileName
                                  << "' (searched in '" << parentDir << "')" << std::endl;
                        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                    }
                }
            }

            try
            {
                std::vector<uint8_t> bytes = vfs.ReadBytes(targetVirtual);
                if (bytes.empty() && !vfs.Exists(targetVirtual))
                {
                    std::cout << "[ShaderCompiler] Include file not found in VFS: '" << targetVirtual << "'" << std::endl;
                    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                }
                buffers.emplace_back(std::move(bytes));
                const auto& stored = buffers.back();
                *ppData = stored.data();
                *pBytes = static_cast<UINT>(stored.size());
                parentDirs[stored.data()] = std::filesystem::path(targetVirtual).parent_path().generic_u8string();
                const std::string physical = PhysicalPath(targetVirtual);
                Dependencies.push_back(NormalizePath(physical));
                OpenedAs[pFileName] = FromUtf8(physical).lexically_normal().u8string();
                return S_OK;
            }
            catch (const std::exception& ex)
            {
                std::cout << "[ShaderCompiler] Failed to read include '" << targetVirtual << "' via VFS: " << ex.what() << std::endl;
                return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            }
            catch (...)
            {
                std::cout << "[ShaderCompiler] Unknown exception while reading include '" << targetVirtual << "' via VFS" << std::endl;
                return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            }
        }

        HRESULT __stdcall Close(LPCVOID pData) override
        {
            return S_OK;
        }

        std::vector<std::string> Dependencies;
        // The compiler names an included file as written in #include: "name" -> real path.
        std::map<std::string, std::string> OpenedAs;

    private:
        std::string baseDir;
        const std::map<std::string, std::string>& overrides;
        std::vector<std::vector<uint8_t>> buffers;
        std::map<const void*, std::string> parentDirs;
    };

    std::string NormalizePath(const std::string& path)
    {
        std::error_code error;
        std::filesystem::path absolute = std::filesystem::absolute(FromUtf8(path), error);
        if (error)
        {
            absolute = FromUtf8(path);
        }
        std::wstring normalized = absolute.lexically_normal().generic_wstring();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return std::filesystem::path(normalized).generic_u8string();
    }

    CompileResult Compile(const std::string& fileName, const char* entryPoint, const char* target,
                          const CompileOptions& options)
    {
        CompileResult result;
        std::string virtualPath;
        const std::vector<uint8_t> source = fileName.empty() ? std::vector<uint8_t>{} : VfsRead::Active().ReadBytes(fileName, &virtualPath);
        result.Dependencies.push_back(NormalizePath(virtualPath.empty() ? fileName : PhysicalPath(virtualPath)));
        if (source.empty())
        {
            result.Result = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            result.Errors = "shader file not found or unreadable: " + fileName;
            return result;
        }

        std::vector<D3D_SHADER_MACRO> macros;
        macros.reserve(options.Defines.size() + 1);
        for (const auto& define : options.Defines)
        {
            macros.push_back({define.first.c_str(), define.second.c_str()});
        }
        macros.push_back({nullptr, nullptr});

        VfsShaderInclude includes(std::filesystem::path(virtualPath).parent_path().generic_u8string(), options.IncludeOverrides);
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        // The source name is the path the caller gave, so compiler messages point at the file the author edits.
        result.Result = D3DCompile(source.data(), source.size(), fileName.c_str(), macros.data(), &includes,
                                   entryPoint, target, GetCompileFlags(), 0,
                                   result.Blob.GetAddressOf(), errors.GetAddressOf());
        if (errors)
        {
            result.Errors.assign(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
            while (!result.Errors.empty() && result.Errors.back() == '\0')
            {
                result.Errors.pop_back();
            }
            result.Errors = ResolveMessagePaths(result.Errors, includes.OpenedAs);
        }
        else if (FAILED(result.Result))
        {
            result.Errors = "shader compilation failed: " + fileName;
        }
        if (FAILED(result.Result))
        {
            result.Blob.Reset();
        }

        result.Dependencies.insert(result.Dependencies.end(), includes.Dependencies.begin(), includes.Dependencies.end());
        return result;
    }

    HRESULT CompileFromFile(const std::wstring& fileName,
                           const D3D_SHADER_MACRO* defines,
                           const char* entryPoint,
                           const char* target,
                           ID3DBlob** blob,
                           std::string* errorMessage)
    {
        if (fileName.empty())
        {
            if (errorMessage) *errorMessage = "shader file not found or unreadable";
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        std::string utf8Path;
        const int pathLen = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), static_cast<int>(fileName.size()), nullptr, 0, nullptr, nullptr);
        if (pathLen <= 0)
        {
            const DWORD err = GetLastError();
            std::cerr << "[ShaderCompiler] WideCharToMultiByte failed to calculate buffer size (error " << err << ")\n";
            if (errorMessage) *errorMessage = "failed to convert shader file path to UTF-8";
            return E_FAIL;
        }

        utf8Path.resize(static_cast<size_t>(pathLen));
        const int converted = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), static_cast<int>(fileName.size()), utf8Path.data(), pathLen, nullptr, nullptr);
        if (converted <= 0)
        {
            const DWORD err = GetLastError();
            std::cerr << "[ShaderCompiler] WideCharToMultiByte failed to convert path to UTF-8 (error " << err << ")\n";
            if (errorMessage) *errorMessage = "failed to convert shader file path to UTF-8";
            return E_FAIL;
        }

        CompileOptions options;
        for (const D3D_SHADER_MACRO* define = defines; define && define->Name; ++define)
        {
            options.Defines.emplace_back(define->Name, define->Definition ? define->Definition : "");
        }

        CompileResult result = Compile(utf8Path, entryPoint, target, options);
        if (errorMessage && FAILED(result.Result))
        {
            *errorMessage = result.Errors;
        }
        if (blob)
        {
            *blob = result.Blob.Detach();
        }
        return result.Result;
    }
}
