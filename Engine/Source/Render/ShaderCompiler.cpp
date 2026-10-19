#include <Engine/Render/ShaderCompiler.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

namespace
{
    std::filesystem::path FromUtf8(const std::string& path) { return std::filesystem::u8path(path); }

    bool ReadFile(const std::filesystem::path& path, std::string& contents)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return false;
        }
        contents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        return true;
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

    /**
     * Resolves #include "..." relative to the including file (as D3D_COMPILE_STANDARD_FILE_INCLUDE
     * does), applies overrides and records every opened file.
     */
    class IncludeHandler final : public ID3DInclude
    {
    public:
        IncludeHandler(std::filesystem::path rootDirectory, const std::map<std::string, std::string>& overrides)
            : RootDirectory(std::move(rootDirectory)), Overrides(overrides) {}

        HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID parentData,
                               LPCVOID* data, UINT* bytes) override
        {
            std::filesystem::path path;
            const auto overridden = Overrides.find(fileName);
            if (overridden != Overrides.end())
            {
                path = FromUtf8(overridden->second);
            }
            else
            {
                const auto parent = Directories.find(parentData);
                const std::filesystem::path& directory = parent != Directories.end() ? parent->second : RootDirectory;
                path = directory / FromUtf8(fileName);
            }

            auto file = std::make_unique<std::string>();
            if (!ReadFile(path, *file))
            {
                return E_FAIL;
            }
            *data = file->data();
            *bytes = static_cast<UINT>(file->size());
            Directories[file->data()] = path.parent_path();
            Dependencies.push_back(ShaderCompiler::NormalizePath(path.u8string()));
            OpenedAs[fileName] = path.lexically_normal().u8string();
            Files.push_back(std::move(file));
            return S_OK;
        }

        HRESULT __stdcall Close(LPCVOID) override { return S_OK; } // buffers live until the handler dies

        std::vector<std::string> Dependencies;
        // The compiler names an included file as written in #include: "name" -> real path.
        std::map<std::string, std::string> OpenedAs;

    private:
        std::filesystem::path RootDirectory;
        const std::map<std::string, std::string>& Overrides;
        std::map<LPCVOID, std::filesystem::path> Directories;
        std::vector<std::unique_ptr<std::string>> Files;
    };
}

namespace ShaderCompiler
{
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
        const std::filesystem::path path = FromUtf8(fileName);
        result.Dependencies.push_back(NormalizePath(fileName));

        std::string source;
        if (!ReadFile(path, source))
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

        IncludeHandler includes(path.parent_path(), options.IncludeOverrides);
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        // The source name is the real path, so compiler messages point at the file the author edits.
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
        CompileOptions options;
        for (const D3D_SHADER_MACRO* define = defines; define && define->Name; ++define)
        {
            options.Defines.emplace_back(define->Name, define->Definition ? define->Definition : "");
        }

        CompileResult result = Compile(std::filesystem::path(fileName).u8string(), entryPoint, target, options);
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
