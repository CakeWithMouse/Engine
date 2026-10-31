#include <Engine/UI/UIMarkup.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <locale>

namespace
{
    struct Attribute
    {
        std::string Name;
        std::string Value;
        int Line = 0;
        mutable bool bUsed = false;
    };

    struct Node
    {
        std::string Tag;
        std::vector<Attribute> Attributes;
        std::vector<Node> Children;
        int Line = 0;
    };

    bool IsNameChar(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == ':' || c == '.';
    }

    bool IsSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    /**
     * The subset of XML the interface needs: elements, attributes, comments and the declaration.
     * Text content is not part of the format - everything an element says is in its attributes -
     * so a stray word is reported instead of being silently dropped.
     */
    class XmlReader
    {
    public:
        XmlReader(const std::string& text, const std::string& sourceName)
            : Text(text), Source(sourceName) {}

        bool ReadDocument(Node& outRoot, std::string& error)
        {
            if (Text.compare(0, 3, "\xEF\xBB\xBF") == 0) Skip(3);
            SkipIgnorable();
            if (Peek() != '<')
            {
                return Fail(error, "expected an element at the start of the document");
            }
            if (!ReadElement(outRoot, error))
            {
                return false;
            }
            SkipIgnorable();
            if (Position < Text.size())
            {
                return Fail(error, "unexpected content after the root element");
            }
            return true;
        }

    private:
        char Peek(size_t offset = 0) const
        {
            const size_t index = Position + offset;
            return index < Text.size() ? Text[index] : '\0';
        }

        char Get()
        {
            const char c = Peek();
            ++Position;
            if (c == '\n')
            {
                ++Line;
            }
            return c;
        }

        bool StartsWith(const char* prefix) const
        {
            return Text.compare(Position, std::strlen(prefix), prefix) == 0;
        }

        void Skip(size_t count)
        {
            for (size_t i = 0; i < count && Position < Text.size(); ++i)
            {
                Get();
            }
        }

        bool Fail(std::string& error, const std::string& message) const
        {
            error = Source + ":" + std::to_string(Line) + ": " + message;
            return false;
        }

        void SkipSpace()
        {
            while (Position < Text.size() && IsSpace(Peek()))
            {
                Get();
            }
        }

        /** Whitespace, comments, the XML declaration and a doctype: everything with no meaning. */
        void SkipIgnorable()
        {
            for (;;)
            {
                SkipSpace();
                if (StartsWith("<!--"))
                {
                    Skip(4);
                    while (Position < Text.size() && !StartsWith("-->"))
                    {
                        Get();
                    }
                    Skip(3);
                    continue;
                }
                if (StartsWith("<?"))
                {
                    Skip(2);
                    while (Position < Text.size() && !StartsWith("?>"))
                    {
                        Get();
                    }
                    Skip(2);
                    continue;
                }
                if (StartsWith("<!"))
                {
                    while (Position < Text.size() && Peek() != '>')
                    {
                        Get();
                    }
                    Skip(1);
                    continue;
                }
                return;
            }
        }

        std::string ReadName()
        {
            std::string name;
            while (Position < Text.size() && IsNameChar(Peek()))
            {
                name.push_back(Get());
            }
            return name;
        }

        /** Decodes the five XML entities and numeric references; anything else stays as written. */
        bool ReadAttributeValue(std::string& outValue, std::string& error)
        {
            const char quote = Get();
            if (quote != '"' && quote != '\'')
            {
                return Fail(error, "an attribute value must be quoted");
            }
            outValue.clear();
            while (Position < Text.size() && Peek() != quote)
            {
                if (Peek() != '&')
                {
                    outValue.push_back(Get());
                    continue;
                }
                const size_t end = Text.find(';', Position);
                if (end == std::string::npos || end - Position > 10)
                {
                    outValue.push_back(Get());
                    continue;
                }
                const std::string entity = Text.substr(Position + 1, end - Position - 1);
                Skip(end - Position + 1);
                if (entity == "lt") outValue.push_back('<');
                else if (entity == "gt") outValue.push_back('>');
                else if (entity == "amp") outValue.push_back('&');
                else if (entity == "quot") outValue.push_back('"');
                else if (entity == "apos") outValue.push_back('\'');
                else if (entity.size() > 1 && entity[0] == '#')
                {
                    const bool bHex = entity[1] == 'x' || entity[1] == 'X';
                    const long code = std::strtol(entity.c_str() + (bHex ? 2 : 1), nullptr, bHex ? 16 : 10);
                    if (code > 0 && code < 128)
                    {
                        outValue.push_back(static_cast<char>(code));
                    }
                }
                else
                {
                    return Fail(error, "unknown entity &" + entity + ";");
                }
            }
            if (Position >= Text.size())
            {
                return Fail(error, "an attribute value is not closed");
            }
            Get();
            return true;
        }

        bool ReadElement(Node& outNode, std::string& error)
        {
            outNode.Line = Line;
            Get(); // '<'
            outNode.Tag = ReadName();
            if (outNode.Tag.empty())
            {
                return Fail(error, "an element has no name");
            }

            for (;;)
            {
                SkipSpace();
                if (Peek() == '/' && Peek(1) == '>')
                {
                    Skip(2);
                    return true;
                }
                if (Peek() == '>')
                {
                    Skip(1);
                    break;
                }
                if (Position >= Text.size())
                {
                    return Fail(error, "<" + outNode.Tag + "> is not closed");
                }

                Attribute attribute;
                attribute.Line = Line;
                attribute.Name = ReadName();
                if (attribute.Name.empty())
                {
                    return Fail(error, std::string("unexpected character '") + Peek() + "' in <" + outNode.Tag + ">");
                }
                SkipSpace();
                if (Peek() != '=')
                {
                    return Fail(error, "attribute " + attribute.Name + " has no value");
                }
                Get();
                SkipSpace();
                if (!ReadAttributeValue(attribute.Value, error))
                {
                    return false;
                }
                outNode.Attributes.push_back(std::move(attribute));
            }

            for (;;)
            {
                SkipIgnorable();
                if (Position >= Text.size())
                {
                    return Fail(error, "<" + outNode.Tag + "> is not closed");
                }
                if (Peek() == '<' && Peek(1) == '/')
                {
                    Skip(2);
                    const std::string closing = ReadName();
                    SkipSpace();
                    if (Peek() != '>')
                    {
                        return Fail(error, "the closing tag of " + closing + " is malformed");
                    }
                    Get();
                    if (closing != outNode.Tag)
                    {
                        return Fail(error, "</" + closing + "> closes <" + outNode.Tag + ">");
                    }
                    return true;
                }
                if (Peek() == '<')
                {
                    Node child;
                    if (!ReadElement(child, error))
                    {
                        return false;
                    }
                    outNode.Children.push_back(std::move(child));
                    continue;
                }
                return Fail(error, "text is not part of the markup; everything belongs in attributes");
            }
        }

        const std::string& Text;
        const std::string& Source;
        size_t Position = 0;
        int Line = 1;
    };

    /** Builds elements out of parsed nodes and reports what it could not make sense of. */
    class MarkupBuilder
    {
    public:
        MarkupBuilder(const std::string& sourceName, std::vector<std::string>* warnings)
            : Source(sourceName), Warnings(warnings) {}

        std::unique_ptr<UIElement> Build(const Node& node, std::string& error)
        {
            std::unique_ptr<UIElement> element = Create(node, error);
            if (element == nullptr)
            {
                return nullptr;
            }
            if (!ApplyCommon(node, *element, error) || !ApplyTyped(node, *element, error))
            {
                return nullptr;
            }
            for (const Node& childNode : node.Children)
            {
                std::unique_ptr<UIElement> child = Build(childNode, error);
                if (child == nullptr)
                {
                    return nullptr;
                }
                element->Add(std::move(child));
            }
            ReportUnused(node);
            return element;
        }

    private:
        void Warn(int line, const std::string& message) const
        {
            if (Warnings != nullptr)
            {
                Warnings->push_back(Source + ":" + std::to_string(line) + ": " + message);
            }
        }

        bool Fail(std::string& error, int line, const std::string& message) const
        {
            error = Source + ":" + std::to_string(line) + ": " + message;
            return false;
        }

        const Attribute* Lookup(const Node& node, const char* name) const
        {
            for (const Attribute& attribute : node.Attributes)
            {
                if (ToLower(attribute.Name) == name)
                {
                    attribute.bUsed = true;
                    return &attribute;
                }
            }
            return nullptr;
        }

        void ReportUnused(const Node& node) const
        {
            for (const Attribute& attribute : node.Attributes)
            {
                if (!attribute.bUsed)
                {
                    Warn(attribute.Line, "<" + node.Tag + "> has no attribute " + attribute.Name + ", ignored");
                }
            }
        }

        std::unique_ptr<UIElement> Create(const Node& node, std::string& error) const
        {
            const std::string tag = ToLower(node.Tag);
            if (tag == "ui" || tag == "group") return std::make_unique<UIElement>();
            if (tag == "panel") return std::make_unique<UIPanel>();
            if (tag == "label") return std::make_unique<UILabel>();
            if (tag == "bar") return std::make_unique<UIBar>();
            if (tag == "button") return std::make_unique<UIButton>();
            Fail(error, node.Line, "unknown element <" + node.Tag + ">");
            return nullptr;
        }

        bool ReadFloats(const Attribute& attribute, std::vector<float>& out, size_t maxCount,
                        std::string& error) const
        {
            out.clear();
            std::istringstream stream(attribute.Value);
            float value = 0.0f;
            while (stream >> value)
            {
                out.push_back(value);
            }
            if (out.empty() || out.size() > maxCount || !stream.eof())
            {
                return Fail(error, attribute.Line, attribute.Name + ": expected up to " +
                                                       std::to_string(maxCount) + " numbers, got \"" +
                                                       attribute.Value + "\"");
            }
            return true;
        }

        bool ReadFloat(const Attribute& attribute, float& out, std::string& error) const
        {
            std::vector<float> values;
            if (!ReadFloats(attribute, values, 1, error))
            {
                return false;
            }
            out = values[0];
            return true;
        }

        bool ReadBool(const Attribute& attribute, bool& out, std::string& error) const
        {
            const std::string value = ToLower(attribute.Value);
            if (value == "true" || value == "1") { out = true; return true; }
            if (value == "false" || value == "0") { out = false; return true; }
            return Fail(error, attribute.Line, attribute.Name + ": expected true or false");
        }

        bool ReadColor(const Attribute& attribute, glm::vec4& out, std::string& error) const
        {
            if (UIMarkup::ParseColor(attribute.Value, out))
            {
                return true;
            }
            return Fail(error, attribute.Line, attribute.Name + ": expected #RRGGBB or #RRGGBBAA, got \"" +
                                                   attribute.Value + "\"");
        }

        bool ApplyCommon(const Node& node, UIElement& element, std::string& error) const
        {
            if (const Attribute* name = Lookup(node, "name"))
            {
                element.Name = name->Value;
            }
            if (const Attribute* anchor = Lookup(node, "anchor"))
            {
                if (!UIMarkup::ParseAnchor(anchor->Value, element.Rect.Anchor))
                {
                    return Fail(error, anchor->Line, "anchor: \"" + anchor->Value + "\" is not an anchor name");
                }
            }
            if (const Attribute* rect = Lookup(node, "rect"))
            {
                std::vector<float> values;
                if (!ReadFloats(*rect, values, 4, error))
                {
                    return false;
                }
                if (values.size() < 2)
                {
                    return Fail(error, rect->Line, "rect: expected at least \"x y\"");
                }
                element.Rect.X = values[0];
                element.Rect.Y = values[1];
                element.Rect.Width = values.size() > 2 ? values[2] : 0.0f;
                element.Rect.Height = values.size() > 3 ? values[3] : 0.0f;
            }
            if (const Attribute* x = Lookup(node, "x"))
            {
                if (!ReadFloat(*x, element.Rect.X, error)) return false;
            }
            if (const Attribute* y = Lookup(node, "y"))
            {
                if (!ReadFloat(*y, element.Rect.Y, error)) return false;
            }
            if (const Attribute* width = Lookup(node, "width"))
            {
                if (!ReadFloat(*width, element.Rect.Width, error)) return false;
            }
            if (const Attribute* height = Lookup(node, "height"))
            {
                if (!ReadFloat(*height, element.Rect.Height, error)) return false;
            }
            if (const Attribute* padding = Lookup(node, "padding"))
            {
                std::vector<float> values;
                if (!ReadFloats(*padding, values, 4, error))
                {
                    return false;
                }
                if (values.size() == 1)
                {
                    element.Padding = UIEdges{values[0], values[0], values[0], values[0]};
                }
                else if (values.size() == 4)
                {
                    element.Padding = UIEdges{values[0], values[1], values[2], values[3]};
                }
                else
                {
                    return Fail(error, padding->Line, "padding: expected one number or four");
                }
            }
            if (const Attribute* visible = Lookup(node, "visible"))
            {
                if (!ReadBool(*visible, element.bVisible, error)) return false;
            }
            if (const Attribute* interactive = Lookup(node, "interactive"))
            {
                if (!ReadBool(*interactive, element.bInteractive, error)) return false;
            }
            return true;
        }

        bool ApplyTyped(const Node& node, UIElement& element, std::string& error) const
        {
            if (auto* panel = dynamic_cast<UIPanel*>(&element))
            {
                if (const Attribute* color = Lookup(node, "color"))
                {
                    if (!ReadColor(*color, panel->Color, error)) return false;
                }
                if (const Attribute* border = Lookup(node, "border"))
                {
                    if (!ReadFloat(*border, panel->BorderWidth, error)) return false;
                }
                if (const Attribute* borderColor = Lookup(node, "bordercolor"))
                {
                    if (!ReadColor(*borderColor, panel->BorderColor, error)) return false;
                }
                return true;
            }
            if (auto* label = dynamic_cast<UILabel*>(&element))
            {
                if (const Attribute* text = Lookup(node, "text"))
                {
                    label->SetText(text->Value);
                }
                if (const Attribute* color = Lookup(node, "color"))
                {
                    if (!ReadColor(*color, label->Color, error)) return false;
                }
                if (const Attribute* align = Lookup(node, "align"))
                {
                    const std::string value = ToLower(align->Value);
                    if (value == "right") label->bAlignRight = true;
                    else if (value == "left") label->bAlignRight = false;
                    else return Fail(error, align->Line, "align: expected left or right");
                }
                return true;
            }
            if (auto* bar = dynamic_cast<UIBar*>(&element))
            {
                if (const Attribute* fill = Lookup(node, "fill"))
                {
                    if (!ReadFloat(*fill, bar->Fill, error)) return false;
                }
                if (const Attribute* trackColor = Lookup(node, "trackcolor"))
                {
                    if (!ReadColor(*trackColor, bar->TrackColor, error)) return false;
                }
                if (const Attribute* fillColor = Lookup(node, "fillcolor"))
                {
                    if (!ReadColor(*fillColor, bar->FillColor, error)) return false;
                }
                return true;
            }
            if (auto* button = dynamic_cast<UIButton*>(&element))
            {
                if (const Attribute* text = Lookup(node, "text"))
                {
                    button->SetText(text->Value);
                }
                if (const Attribute* color = Lookup(node, "color"))
                {
                    if (!ReadColor(*color, button->Color, error)) return false;
                }
                if (const Attribute* hoverColor = Lookup(node, "hovercolor"))
                {
                    if (!ReadColor(*hoverColor, button->HoverColor, error)) return false;
                }
                if (const Attribute* pressedColor = Lookup(node, "pressedcolor"))
                {
                    if (!ReadColor(*pressedColor, button->PressedColor, error)) return false;
                }
                if (const Attribute* textColor = Lookup(node, "textcolor"))
                {
                    if (!ReadColor(*textColor, button->TextColor, error)) return false;
                }
                if (const Attribute* border = Lookup(node, "border"))
                {
                    if (!ReadFloat(*border, button->BorderWidth, error)) return false;
                }
                if (const Attribute* borderColor = Lookup(node, "bordercolor"))
                {
                    if (!ReadColor(*borderColor, button->BorderColor, error)) return false;
                }
                return true;
            }
            return true;
        }

        const std::string& Source;
        std::vector<std::string>* Warnings;
    };
}

bool UIMarkup::ParseColor(const std::string& text, glm::vec4& outColor)
{
    if (text.size() != 7 && text.size() != 9)
    {
        return false;
    }
    if (text[0] != '#')
    {
        return false;
    }
    uint32_t channels[4] = {0, 0, 0, 255};
    const size_t count = (text.size() - 1) / 2;
    for (size_t i = 0; i < count; ++i)
    {
        const std::string byte = text.substr(1 + i * 2, 2);
        if (!std::isxdigit(static_cast<unsigned char>(byte[0])) ||
            !std::isxdigit(static_cast<unsigned char>(byte[1])))
        {
            return false;
        }
        channels[i] = static_cast<uint32_t>(std::strtoul(byte.c_str(), nullptr, 16));
    }
    outColor = glm::vec4(static_cast<float>(channels[0]) / 255.0f,
                         static_cast<float>(channels[1]) / 255.0f,
                         static_cast<float>(channels[2]) / 255.0f,
                         static_cast<float>(channels[3]) / 255.0f);
    return true;
}

bool UIMarkup::ParseAnchor(const std::string& text, UIAnchor& outAnchor)
{
    struct Named
    {
        const char* Name;
        UIAnchor Anchor;
    };
    static const Named names[] = {
        {"topleft", UIAnchor::TopLeft},         {"topcenter", UIAnchor::TopCenter},
        {"topright", UIAnchor::TopRight},       {"centerleft", UIAnchor::CenterLeft},
        {"center", UIAnchor::Center},           {"centerright", UIAnchor::CenterRight},
        {"bottomleft", UIAnchor::BottomLeft},   {"bottomcenter", UIAnchor::BottomCenter},
        {"bottomright", UIAnchor::BottomRight},
    };
    const std::string lowered = ToLower(text);
    for (const Named& named : names)
    {
        if (lowered == named.Name)
        {
            outAnchor = named.Anchor;
            return true;
        }
    }
    return false;
}

bool UIMarkup::Parse(const std::string& text, const std::string& sourceName,
                     std::unique_ptr<UIElement>& outRoot, std::string& error,
                     std::vector<std::string>* warnings)
{
    Node root;
    XmlReader reader(text, sourceName);
    if (!reader.ReadDocument(root, error))
    {
        return false;
    }

    MarkupBuilder builder(sourceName, warnings);
    std::unique_ptr<UIElement> element = builder.Build(root, error);
    if (element == nullptr)
    {
        return false;
    }
    outRoot = std::move(element);
    return true;
}

bool UIMarkup::Load(const std::string& path, std::unique_ptr<UIElement>& outRoot,
                    std::string& error, std::vector<std::string>* warnings)
{
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file)
    {
        error = path + ": cannot read the file";
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return Parse(text, path, outRoot, error, warnings);
}

std::string UIMarkup::Serialize(const UIElement& root)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    auto escaped = [](const std::string& value) {
        std::string result;
        for (char c : value) switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            default: result += c;
        }
        return result;
    };
    auto color = [](const glm::vec4& c) {
        std::ostringstream s; s << '#' << std::hex << std::setfill('0');
        for (int i=0;i<4;++i) s << std::setw(2) << static_cast<int>(std::clamp(c[i],0.0f,1.0f)*255.0f+0.5f);
        return s.str();
    };
    const char* anchors[] = {"TopLeft","TopCenter","TopRight","CenterLeft","Center","CenterRight","BottomLeft","BottomCenter","BottomRight"};
    std::function<void(const UIElement&,int)> write = [&](const UIElement& e, int depth) {
        auto panel=dynamic_cast<const UIPanel*>(&e); auto label=dynamic_cast<const UILabel*>(&e);
        auto bar=dynamic_cast<const UIBar*>(&e); auto button=dynamic_cast<const UIButton*>(&e);
        const char* tag = panel?"Panel":label?"Label":bar?"Bar":button?"Button":depth?"Group":"UI";
        out << std::string(depth*2,' ') << '<' << tag;
        auto attr = [&](const char* key, const auto& value) { out << ' ' << key << "=\"" << value << '"'; };
        attr("name",escaped(e.Name)); attr("anchor",anchors[static_cast<int>(e.Rect.Anchor)]);
        out << " rect=\"" << e.Rect.X << ' ' << e.Rect.Y << ' ' << e.Rect.Width << ' ' << e.Rect.Height << '"';
        out << " padding=\"" << e.Padding.Left << ' ' << e.Padding.Top << ' ' << e.Padding.Right << ' ' << e.Padding.Bottom << '"';
        attr("visible",e.bVisible?"true":"false"); attr("interactive",e.bInteractive?"true":"false");
        if(panel) { attr("color",color(panel->Color)); attr("border",panel->BorderWidth); attr("borderColor",color(panel->BorderColor)); }
        if(label) { attr("text",escaped(label->GetText())); attr("color",color(label->Color)); attr("align",label->bAlignRight?"right":"left"); }
        if(bar) { attr("fill",bar->Fill); attr("trackColor",color(bar->TrackColor)); attr("fillColor",color(bar->FillColor)); }
        if(button) {
            attr("text",escaped(button->GetText())); attr("color",color(button->Color));
            attr("hoverColor",color(button->HoverColor)); attr("pressedColor",color(button->PressedColor));
            attr("textColor",color(button->TextColor)); attr("border",button->BorderWidth); attr("borderColor",color(button->BorderColor));
        }
        if(e.GetChildren().empty()) out << "/>\n";
        else { out << ">\n"; for(const auto& child:e.GetChildren()) write(*child,depth+1); out << std::string(depth*2,' ') << "</" << tag << ">\n"; }
    };
    write(root,0);
    return out.str();
}
