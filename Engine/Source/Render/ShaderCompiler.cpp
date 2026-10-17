#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <Engine/Render/ShaderCompiler.h>
#include "../Resources/VfsRead.h"

#include <d3dcompiler.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

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

    class VfsShaderInclude : public ID3DInclude
    {
    public:
        explicit VfsShaderInclude(std::string baseVirtualDirectory)
            : baseDir(std::move(baseVirtualDirectory))
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

            std::string parentDir = baseDir;
            if (pParentData != nullptr)
            {
                auto it = parentDirs.find(pParentData);
                if (it != parentDirs.end())
                {
                    parentDir = it->second;
                }
            }

            auto& vfs = VfsRead::GetVFS();
            // Standard D3D include semantics:
            // 1. Search relative to the file containing the #include
            std::string targetVirtual = VirtualFileSystem::NormalizeVirtualPath(parentDir + "/" + pFileName);
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

            try
            {
                std::vector<uint8_t> bytes = vfs.ReadBytes(targetVirtual);
                buffers.emplace_back(std::move(bytes));
                const auto& stored = buffers.back();
                *ppData = stored.data();
                *pBytes = static_cast<UINT>(stored.size());
                parentDirs[stored.data()] = std::filesystem::path(targetVirtual).parent_path().generic_u8string();
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

    private:
        std::string baseDir;
        std::vector<std::vector<uint8_t>> buffers;
        std::map<const void*, std::string> parentDirs;
    };

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
        if (pathLen > 0)
        {
            utf8Path.resize(static_cast<size_t>(pathLen));
            WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), static_cast<int>(fileName.size()), utf8Path.data(), pathLen, nullptr, nullptr);
        }
        else
        {
            for (wchar_t wc : fileName)
            {
                utf8Path.push_back(static_cast<char>(wc));
            }
        }

        std::string virtualPath;
        const std::vector<uint8_t> shaderSource = VfsRead::ReadBytes(utf8Path, &virtualPath);
        if (shaderSource.empty())
        {
            if (errorMessage)
            {
                *errorMessage = "shader file not found or unreadable";
            }
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        const std::string baseVirtualDir = std::filesystem::path(virtualPath).parent_path().generic_u8string();
        VfsShaderInclude includeHandler(baseVirtualDir);

        ID3DBlob* errors = nullptr;
        const HRESULT hr = D3DCompile(shaderSource.data(),
                                      shaderSource.size(),
                                      utf8Path.c_str(),
                                      defines,
                                      &includeHandler,
                                      entryPoint,
                                      target,
                                      GetCompileFlags(),
                                      0,
                                      blob,
                                      &errors);
        if (errorMessage)
        {
            if (errors)
            {
                *errorMessage = static_cast<const char*>(errors->GetBufferPointer());
            }
            else if (FAILED(hr))
            {
                *errorMessage = "shader compilation failed";
            }
        }
        if (errors)
        {
            errors->Release();
        }
        return hr;
    }
}
