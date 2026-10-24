// Deliberately a small annotation scanner, not a C++ parser/preprocessor.
// Supported declaration grammar and rejection rules: docs/COMPONENT-GENERATION.md.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
struct Token { std::string text; int line; bool literal = false; };
struct Type { std::string name, label; fs::path header; int line; };

static std::string Read(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read " + path.u8string());
    std::ostringstream out; out << file.rdbuf();
    if (file.bad()) throw std::runtime_error("Read failed: " + path.u8string());
    return out.str();
}
[[noreturn]] static void Fail(const fs::path& path, int line, const std::string& message)
{
    throw std::runtime_error(path.u8string() + "(" + std::to_string(line) + "): error ECG001: " + message);
}
static bool Identifier(const std::string& s)
{
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}
static std::vector<Token> Lex(const fs::path& path, const std::string& s)
{
    std::vector<Token> tokens;
    size_t i = s.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    int line = 1;
    auto advance = [&]() { if (s[i++] == '\n') ++line; };
    while (i < s.size())
    {
        if (std::isspace(static_cast<unsigned char>(s[i]))) { advance(); continue; }
        if (s.compare(i, 2, "//") == 0)
        {
            while (i < s.size() && s[i] != '\n') advance();
            continue;
        }
        if (s.compare(i, 2, "/*") == 0)
        {
            int start = line; i += 2;
            while (i < s.size() && s.compare(i, 2, "*/") != 0) advance();
            if (i == s.size()) Fail(path, start, "Unterminated comment");
            i += 2; continue;
        }
        size_t begin = i; int start = line;
        if (s[i] == '#')
        {
            do
            {
                while (i < s.size() && s[i] != '\n') advance();
                size_t end = i;
                if (end > begin && s[end - 1] == '\r') --end;
                bool continuation = end > begin && s[end - 1] == '\\';
                if (i < s.size()) advance();
                if (!continuation) break;
            } while (i < s.size());
            tokens.push_back({s.substr(begin, i - begin), start}); continue;
        }
        // Consume complete string/character tokens, including raw string prefixes.
        size_t quote = i;
        for (const std::string prefix : {"u8R", "uR", "UR", "LR", "R", "u8", "u", "U", "L"})
            if (s.compare(i, prefix.size(), prefix) == 0 && i + prefix.size() < s.size() &&
                (s[i + prefix.size()] == '"' || s[i + prefix.size()] == '\''))
            { quote = i + prefix.size(); break; }
        if (s[quote] == '"' || s[quote] == '\'')
        {
            const char delimiter = s[quote];
            const bool raw = quote > i && s[quote - 1] == 'R';
            i = quote + 1;
            if (raw)
            {
                const size_t open = s.find('(', i);
                if (open == std::string::npos || open - i > 16) Fail(path, start, "Invalid raw string");
                const std::string close = ")" + s.substr(i, open - i) + "\"";
                const size_t end = s.find(close, open + 1);
                if (end == std::string::npos) Fail(path, start, "Unterminated raw string");
                while (i < end + close.size()) advance();
            }
            else
            {
                while (i < s.size() && s[i] != delimiter)
                {
                    if (s[i] == '\\') { advance(); if (i < s.size()) advance(); }
                    else advance();
                }
                if (i == s.size()) Fail(path, start, "Unterminated string");
                advance();
            }
            tokens.push_back({s.substr(begin, i - begin), start, true}); continue;
        }
        if (std::isalpha(static_cast<unsigned char>(s[i])) || s[i] == '_')
        {
            do { ++i; } while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_'));
        }
        else ++i;
        tokens.push_back({s.substr(begin, i - begin), start});
    }
    return tokens;
}
static std::pair<std::string, std::string> Directive(const Token& t)
{
    std::istringstream in(t.text.substr(1)); std::string command, argument;
    in >> command >> argument; return {command, argument};
}
static void Scan(const fs::path& path, bool header, std::vector<Type>& types)
{
    const auto ts = Lex(path, Read(path));
    std::vector<std::string> scopes;
    int conditional = 0;
    bool pendingTemplate = false;
    // A conventional leading include guard is the only allowed conditional wrapper.
    bool guard = ts.size() >= 2 && ts[0].text[0] == '#' && ts[1].text[0] == '#' &&
        Directive(ts[0]).first == "ifndef" && Directive(ts[1]).first == "define" &&
        !Directive(ts[0]).second.empty() && Directive(ts[0]).second == Directive(ts[1]).second;
    for (size_t i = 0; i < ts.size(); ++i)
    {
        const auto& t = ts[i];
        if (t.literal) continue;
        if (t.text[0] == '#')
        {
            const auto d = Directive(t);
            if (d.first == "if" || d.first == "ifdef" || d.first == "ifndef") ++conditional;
            if (d.first == "endif") { --conditional; if (conditional == 0) guard = false; }
            if ((d.first == "else" || d.first == "elif") && conditional == 1) guard = false;
            if (conditional < 0) Fail(path, t.line, "Unmatched #endif");
            continue;
        }
        if (t.text == "ENGINE_PROPERTY") Fail(path, t.line, "ENGINE_PROPERTY is not supported yet");
        if (t.text == "template") pendingTemplate = true;
        if (t.text == "namespace")
        {
            std::string name; size_t j = i + 1;
            while (j < ts.size() && ts[j].text != "{" && ts[j].text != ";" && ts[j].text != "=")
            {
                if (ts[j].literal || (!Identifier(ts[j].text) && ts[j].text != ":"))
                    Fail(path, ts[j].line, "Unsupported namespace declaration");
                name += ts[j++].text;
            }
            if (j < ts.size() && ts[j].text == "{")
            { scopes.push_back(name.empty() ? "#" : name); i = j; continue; }
        }
        if (t.text == "ENGINE_COMPONENT")
        {
            if (!header) Fail(path, t.line, "ENGINE_COMPONENT must be in a header under a component source root");
            if (conditional > (guard ? 1 : 0)) Fail(path, t.line, "Conditional component declarations are unsupported; move the class outside #if");
            if (pendingTemplate || std::find(scopes.begin(), scopes.end(), "#") != scopes.end())
                Fail(path, t.line, "Component must be a non-template class in a named namespace or global scope");
            size_t j = i + 1;
            auto expect = [&](const char* value)
            {
                if (j >= ts.size() || ts[j].text != value) Fail(path, t.line, std::string("Expected '") + value + "' after ENGINE_COMPONENT");
                ++j;
            };
            expect("("); std::string label;
            if (j < ts.size() && ts[j].literal)
            {
                label = ts[j++].text;
                if (label.empty() || label[0] != '"' || label.find("\\0") != std::string::npos || label.find("\\x") != std::string::npos)
                    Fail(path, t.line, "Display name must be one ordinary UTF-8 string literal without octal/hex escapes");
                for (size_t k = 0; k + 1 < label.size(); ++k)
                    if (label[k] == '\\' && std::isdigit(static_cast<unsigned char>(label[k + 1])))
                        Fail(path, t.line, "Numeric escapes are unsupported in display names");
            }
            expect(")"); expect("class");
            if (j >= ts.size() || !Identifier(ts[j].text)) Fail(path, t.line, "Expected class name");
            std::string name;
            for (const auto& scope : scopes) name += scope + "::";
            name += ts[j++].text;
            if (j < ts.size() && ts[j].text == "final") ++j;
            expect(":");
            while (j < ts.size() && ts[j].text != "{" && ts[j].text != ";")
            {
                if (ts[j].text[0] == '#') Fail(path, ts[j].line, "Conditional base lists are unsupported");
                ++j;
            }
            expect("{");
            types.push_back({name, label.empty() ? "\"" + name + "\"" : label, path, t.line});
            // Process the body normally: nested annotations must produce a diagnostic.
            scopes.push_back("#"); pendingTemplate = false; i = j - 1; continue;
        }
        if (t.text == "{") { scopes.push_back("#"); pendingTemplate = false; }
        if (t.text == "}") { if (!scopes.empty()) scopes.pop_back(); pendingTemplate = false; }
        if (t.text == ";") pendingTemplate = false;
    }
    if (conditional != 0) Fail(path, ts.empty() ? 1 : ts.back().line, "Unclosed preprocessor conditional");
}
static void WriteChanged(const fs::path& path, const std::string& contents)
{
    if (fs::exists(path) && Read(path) == contents) return;
    const fs::path temporary = path.wstring() + L".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << contents; out.close();
        if (!out) throw std::runtime_error("Cannot write " + temporary.u8string());
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace " + path.u8string());
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 4) throw std::runtime_error("Usage: ComponentGenerator <UTF-8 roots manifest> <output directory> <catalog identifier>");
        const std::string catalog = fs::path(argv[3]).u8string();
        if (!Identifier(catalog)) throw std::runtime_error("Catalog must be a C++ identifier");
        std::vector<fs::path> files;
        std::string rootsText = Read(argv[1]);
        if (rootsText.compare(0, 3, "\xef\xbb\xbf") == 0) rootsText.erase(0, 3);
        std::istringstream roots(rootsText); std::string root;
        bool haveRoot = false;
        while (std::getline(roots, root))
        {
            if (!root.empty() && root.back() == '\r') root.pop_back();
            if (root.empty()) continue;
            haveRoot = true;
            for (const auto& entry : fs::recursive_directory_iterator(fs::u8path(root)))
            {
                if (!entry.is_regular_file()) continue;
                auto extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (extension == ".h" || extension == ".hpp" || extension == ".hh" || extension == ".cpp")
                    files.push_back(fs::weakly_canonical(entry.path()));
            }
        }
        if (!haveRoot) throw std::runtime_error("No component source roots configured");
        std::sort(files.begin(), files.end()); files.erase(std::unique(files.begin(), files.end()), files.end());
        std::vector<Type> types;
        for (const auto& file : files)
        {
            auto ext = file.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            Scan(file, ext != ".cpp", types);
        }
        std::sort(types.begin(), types.end(), [](const Type& a, const Type& b) { return a.name < b.name; });
        for (size_t i = 1; i < types.size(); ++i)
            if (types[i - 1].name == types[i].name)
                Fail(types[i].header, types[i].line, "Duplicate component " + types[i].name + " (also in " + types[i - 1].header.u8string() + ")");
        const std::string banner = "// Generated by ComponentGenerator. Do not edit.\n";
        const std::string header = banner + "#pragma once\n#include <Engine/Components/ComponentType.h>\nnamespace EngineGenerated::" + catalog + " { ComponentTypeCatalog GetComponentTypes() noexcept; }\n";
        std::string source = banner + "#include \"GeneratedComponents.h\"\n";
        std::vector<fs::path> includes;
        for (const auto& type : types) includes.push_back(type.header);
        std::sort(includes.begin(), includes.end()); includes.erase(std::unique(includes.begin(), includes.end()), includes.end());
        for (const auto& include : includes) source += "#include \"" + include.generic_u8string() + "\"\n";
        source += "namespace EngineGenerated::" + catalog + " {\nComponentTypeCatalog GetComponentTypes() noexcept\n{\n";
        if (types.empty()) source += "    return {};\n";
        else
        {
            source += "    static const ComponentType types[] = {\n";
            for (const auto& type : types)
                source += "        {\"" + type.name + "\", " + type.label + ", &EngineComponentDetail::Create<::" + type.name + ">, {}},\n";
            source += "    };\n    return {types, sizeof(types) / sizeof(types[0])};\n";
        }
        source += "}\n}\n";
        fs::create_directories(argv[2]);
        WriteChanged(fs::path(argv[2]) / "GeneratedComponents.h", header);
        WriteChanged(fs::path(argv[2]) / "GeneratedComponents.cpp", source);
        std::cout << "ComponentGenerator: " << types.size() << " component(s), " << catalog << '\n';
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
