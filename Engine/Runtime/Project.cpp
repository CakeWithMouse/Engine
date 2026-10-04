#define NOMINMAX
#include "Project.h"
#include <Windows.h>
#include <fstream>
#include <map>
#include <stdexcept>

namespace
{
class ProjectJson
{
    const std::string& text;
    size_t pos = 0;
    [[noreturn]] void Invalid() const { throw std::runtime_error("Invalid Project.json near byte " + std::to_string(pos)); }
    void Space() { while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r' || text[pos] == '\t')) ++pos; }
    bool Take(char c) { Space(); if (pos < text.size() && text[pos] == c) { ++pos; return true; } return false; }
    unsigned Hex()
    {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (pos == text.size()) Invalid();
            const char c = text[pos++];
            const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) Invalid();
            value = value * 16 + digit;
        }
        return value;
    }
    void Unicode(std::string& value)
    {
        wchar_t chars[2] = {static_cast<wchar_t>(Hex()), 0};
        int count = 1;
        if (chars[0] >= 0xd800 && chars[0] <= 0xdbff)
        {
            if (pos + 2 > text.size() || text[pos++] != '\\' || text[pos++] != 'u') Invalid();
            chars[1] = static_cast<wchar_t>(Hex());
            if (chars[1] < 0xdc00 || chars[1] > 0xdfff) Invalid();
            count = 2;
        }
        else if (chars[0] >= 0xdc00 && chars[0] <= 0xdfff) Invalid();
        char buffer[8];
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, chars, count, buffer, 8, nullptr, nullptr);
        if (!size) Invalid();
        value.append(buffer, size);
    }
    std::string String()
    {
        if (!Take('"')) Invalid();
        std::string value;
        while (pos < text.size())
        {
            const unsigned char c = text[pos++];
            if (c == '"') return value;
            if (c < 0x20) Invalid();
            if (c != '\\') { value += static_cast<char>(c); continue; }
            if (pos == text.size()) Invalid();
            switch (text[pos++])
            {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case '/': value += '/'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            case 'u': Unicode(value); break;
            default: Invalid();
            }
        }
        Invalid();
    }
public:
    explicit ProjectJson(const std::string& source) : text(source) {}
    std::map<std::string, std::string> Read()
    {
        std::map<std::string, std::string> fields;
        if (!Take('{')) Invalid();
        if (!Take('}'))
        {
            do
            {
                const auto key = String();
                if (!Take(':')) Invalid();
                if (!fields.emplace(key, String()).second) Invalid();
                if (Take('}')) break;
                if (!Take(',')) Invalid();
            } while (true);
        }
        Space();
        if (pos != text.size()) Invalid();
        return fields;
    }
};
}

RuntimeProject ReadRuntimeProject(const std::filesystem::path& path)
{
    const auto file = std::filesystem::absolute(path).lexically_normal();
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open project: " + file.u8string());
    if (std::filesystem::file_size(file) > 65536) throw std::runtime_error("Project.json exceeds 64 KiB");
    std::string json((std::istreambuf_iterator<char>(input)), {});
    if (json.compare(0, 3, "\xef\xbb\xbf") == 0) json.erase(0, 3);
    if (json.empty() || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, json.data(), static_cast<int>(json.size()), nullptr, 0))
        throw std::runtime_error("Project.json must be valid UTF-8");
    const auto fields = ProjectJson(json).Read();
    if (fields.size() != 3 || !fields.count("name") || !fields.count("module") || !fields.count("content"))
        throw std::runtime_error("Project.json requires exactly name, module, content string fields");
    for (const auto& field : fields)
        if (field.second.empty() || field.second.find_first_of(std::string("\0\r\n", 3)) != std::string::npos)
            throw std::runtime_error("Project fields must be nonempty single-line strings without NUL");
    RuntimeProject project{fields.at("name"), file,
        (file.parent_path() / std::filesystem::u8path(fields.at("module"))).lexically_normal(),
        (file.parent_path() / std::filesystem::u8path(fields.at("content"))).lexically_normal()};
    if (!std::filesystem::is_directory(project.content)) throw std::runtime_error("Project content directory is missing");
    return project;
}
