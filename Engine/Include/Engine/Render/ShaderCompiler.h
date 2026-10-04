#pragma once
#include <d3dcompiler.h>
#include <string>

/**
 * Single HLSL compilation policy for the whole application.
 * Debug builds keep shader debug info and skip optimization; Release builds compile optimized
 * shaders, so GPU timings taken in Release reflect optimized HLSL.
 */
namespace ShaderCompiler
{
    inline UINT GetCompileFlags()
    {
#ifdef _DEBUG
        return D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        return D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    }

    // Part of every shader cache key so blobs compiled under different policies never mix.
    inline const char* GetPolicyTag()
    {
#ifdef _DEBUG
        return "debug";
#else
        return "optimized";
#endif
    }

    /**
     * Compiles an entry point from a file. #include "..." is resolved relative to the file.
     * On failure returns the HRESULT and fills errorMessage (compiler output or a short reason).
     */
    inline HRESULT CompileFromFile(const std::wstring& fileName,
                                   const D3D_SHADER_MACRO* defines,
                                   const char* entryPoint,
                                   const char* target,
                                   ID3DBlob** blob,
                                   std::string* errorMessage = nullptr)
    {
        ID3DBlob* errors = nullptr;
        const HRESULT hr = D3DCompileFromFile(fileName.c_str(),
                                              defines,
                                              D3D_COMPILE_STANDARD_FILE_INCLUDE,
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
                *errorMessage = "shader file not found or unreadable";
            }
        }
        if (errors)
        {
            errors->Release();
        }
        return hr;
    }
}
