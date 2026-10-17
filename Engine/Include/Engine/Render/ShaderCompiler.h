#pragma once

#include <string>

struct _D3D_SHADER_MACRO;
typedef struct _D3D_SHADER_MACRO D3D_SHADER_MACRO;
struct ID3D10Blob;
typedef ID3D10Blob ID3DBlob;
typedef long HRESULT;

/**
 * Single HLSL compilation policy for the whole application.
 * Debug builds keep shader debug info and skip optimization; Release builds compile optimized
 * shaders, so GPU timings taken in Release reflect optimized HLSL.
 */
namespace ShaderCompiler
{
    unsigned int GetCompileFlags();
    const char* GetPolicyTag();

    /**
     * Compiles an entry point from a file via VFS. #include "..." is resolved via VFS relative to the file.
     * On failure returns the HRESULT and fills errorMessage (compiler output or a short reason).
     */
    HRESULT CompileFromFile(const std::wstring& fileName,
                            const D3D_SHADER_MACRO* defines,
                            const char* entryPoint,
                            const char* target,
                            ID3DBlob** blob,
                            std::string* errorMessage = nullptr);
}
