#pragma once

#include <d3dcommon.h>
#include <map>
#include <string>
#include <vector>
#include <wrl/client.h>

/**
 * Single HLSL compilation policy for the whole application.
 * Debug builds keep shader debug info and skip optimization; Release builds compile optimized
 * shaders, so GPU timings taken in Release reflect optimized HLSL.
 */
namespace ShaderCompiler
{
    unsigned int GetCompileFlags();
    const char* GetPolicyTag();

    /** Absolute, lexically normal, generic-separator and lower-case: one spelling per file. */
    std::string NormalizePath(const std::string& path);

    /** Everything that selects one compiled blob besides the entry point and the target. */
    struct CompileOptions
    {
        std::vector<std::pair<std::string, std::string>> Defines;
        /**
         * #include "name" -> file on disk. Lets a shared template include a file chosen at compile
         * time (e.g. the surface of a material). Names are matched exactly as written in HLSL.
         */
        std::map<std::string, std::string> IncludeOverrides;
    };

    struct CompileResult
    {
        HRESULT Result = E_FAIL;
        Microsoft::WRL::ComPtr<ID3DBlob> Blob;
        std::string Errors;                    // compiler output
        std::vector<std::string> Dependencies; // normalized: the main file and every included file
        bool Succeeded() const { return SUCCEEDED(Result) && Blob; }
    };

    /**
     * Compiles an entry point from a file (UTF-8 path) via VFS. #include "..." is resolved via VFS
     * relative to the including file unless the name is listed in options.IncludeOverrides.
     */
    CompileResult Compile(const std::string& fileName, const char* entryPoint, const char* target,
                          const CompileOptions& options = {});

    /** Legacy form: wide path, raw macro list. Fills errorMessage on failure. */
    HRESULT CompileFromFile(const std::wstring& fileName,
                            const D3D_SHADER_MACRO* defines,
                            const char* entryPoint,
                            const char* target,
                            ID3DBlob** blob,
                            std::string* errorMessage = nullptr);
}
