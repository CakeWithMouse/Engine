#include "HlslToGraph.h"
#include "HlslGraphCompiler.h"

#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <algorithm>

namespace EditorCore
{

namespace
{

struct Token
{
    QString text;
    bool isNumber() const
    {
        if (text.isEmpty()) return false;
        return text[0].isDigit() || (text[0] == QLatin1Char('.') && text.size() > 1 && text[1].isDigit());
    }
    bool isIdent() const
    {
        if (text.isEmpty()) return false;
        return text[0].isLetter() || text[0] == QLatin1Char('_');
    }
};

QVector<Token> Tokenize(const QString& src)
{
    QVector<Token> out;
    int i = 0;
    const int n = src.size();
    while (i < n)
    {
        const QChar c = src[i];
        if (c.isSpace()) { ++i; continue; }
        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && src[i + 1].isDigit()))
        {
            int j = i;
            while (j < n && (src[j].isDigit() || src[j] == QLatin1Char('.'))) ++j;
            if (j < n && (src[j] == QLatin1Char('f') || src[j] == QLatin1Char('F'))) ++j;
            out.append({src.mid(i, j - i)});
            i = j;
            continue;
        }
        if (c.isLetter() || c == QLatin1Char('_'))
        {
            int j = i;
            while (j < n && (src[j].isLetterOrNumber() || src[j] == QLatin1Char('_'))) ++j;
            out.append({src.mid(i, j - i)});
            i = j;
            continue;
        }
        out.append({QString(c)});
        ++i;
    }
    return out;
}

bool IsFloatType(const QString& s)
{
    return s == QStringLiteral("float") || s == QStringLiteral("float2") || s == QStringLiteral("float3") || s == QStringLiteral("float4");
}

double NumValue(const QString& tok)
{
    QString s = tok;
    if (s.endsWith(QLatin1Char('f')) || s.endsWith(QLatin1Char('F'))) s.chop(1);
    return s.toDouble();
}

struct Value
{
    int node = -1;
    QString pin;
    int size = 1;
};

// Рекурсивный разбор инлайнового выражения в ноды графа. Приведения Convert ((floatN)(x),
// (x).xyz, floatN(x, 0..)) снимаются: компилятор добавит их снова по размерам пинов.
class ExprParser
{
public:
    ExprParser(MaterialGraphModel& model, const QVector<Token>& tokens, QStringList& problems,
               QHash<QString, int>& params, QHash<QString, int>& textures)
        : model_(model), tok_(tokens), problems_(problems), params_(params), textures_(textures) {}

    Value Parse() { return AddSub(); }
    bool AtEnd() const { return pos_ >= tok_.size(); }

private:
    MaterialGraphModel& model_;
    const QVector<Token>& tok_;
    QStringList& problems_;
    QHash<QString, int>& params_;
    QHash<QString, int>& textures_;
    int pos_ = 0;
    int layout_ = 0;

    QString Peek(int off = 0) const { return pos_ + off < tok_.size() ? tok_[pos_ + off].text : QString(); }
    QString Take() { return pos_ < tok_.size() ? tok_[pos_++].text : QString(); }
    void Expect(const QString& s)
    {
        if (Peek() != s) problems_ << QStringLiteral("expected '%1' near '%2'").arg(s, Peek());
        else Take();
    }
    void Fail(const QString& msg) { problems_ << msg; }

    QPointF NextPos() { return QPointF(120 + (layout_ % 6) * 230, 120 + (layout_++ / 6) * 170); }

    int Make(const QString& type, const QString& title, const QVariantMap& props = {})
    {
        return model_.AddNode(type, title, NextPos(), props);
    }
    void Link(const Value& v, int to, const QString& pin)
    {
        if (v.node >= 0) model_.AddConnection(v.node, v.pin, to, pin);
    }
    QString TypeOf(int node) const
    {
        const GraphNodeData* n = model_.FindNode(node);
        return n ? n->type : QString();
    }

    Value Binary(const QString& type, const Value& a, const Value& b, const QString& pa, const QString& pb)
    {
        const int id = Make(type, type);
        Link(a, id, pa);
        Link(b, id, pb);
        return {id, QStringLiteral("Out"), qMax(a.size, b.size)};
    }
    Value Unary(const QString& type, const Value& a)
    {
        const int id = Make(type, type);
        Link(a, id, QStringLiteral("In"));
        return {id, QStringLiteral("Out"), a.size};
    }

    Value AddSub()
    {
        Value a = MulDiv();
        while (Peek() == QStringLiteral("+") || Peek() == QStringLiteral("-"))
        {
            const QString op = Take();
            const Value b = MulDiv();
            a = Binary(op == QStringLiteral("+") ? QStringLiteral("Add") : QStringLiteral("Subtract"), a, b,
                       QStringLiteral("A"), QStringLiteral("B"));
        }
        return a;
    }
    Value MulDiv()
    {
        Value a = Primary();
        while (Peek() == QStringLiteral("*") || Peek() == QStringLiteral("/"))
        {
            const QString op = Take();
            const Value b = Primary();
            a = Binary(op == QStringLiteral("*") ? QStringLiteral("Multiply") : QStringLiteral("Divide"), a, b,
                       QStringLiteral("A"), QStringLiteral("B"));
        }
        return a;
    }

    Value Primary()
    {
        const QString t = Peek();
        if (t.isEmpty()) { Fail(QStringLiteral("unexpected end of expression")); return {}; }
        if (tok_[pos_].isNumber())
        {
            Take();
            const int id = Make(QStringLiteral("Constant"), QStringLiteral("Constant"),
                                {{QStringLiteral("value"), NumValue(t)}});
            return {id, QStringLiteral("Value"), 1};
        }
        if (t == QStringLiteral("("))
        {
            Take();
            if (IsFloatType(Peek()) && Peek(1) == QStringLiteral(")") && Peek(2) == QStringLiteral("("))
            {
                Take(); Take(); Take(); // (floatN)(
                const Value v = AddSub();
                Expect(QStringLiteral(")"));
                return v; // приведение размера снято
            }
            if (Peek() == QStringLiteral("1.0f") && Peek(1) == QStringLiteral("-"))
            {
                Take(); Take();
                const Value v = AddSub();
                Expect(QStringLiteral(")"));
                return Unary(QStringLiteral("OneMinus"), v);
            }
            const Value v = AddSub();
            Expect(QStringLiteral(")"));
            return Postfix(v);
        }
        if (tok_[pos_].isIdent()) return Ident(Take());
        Fail(QStringLiteral("unexpected token '%1'").arg(t));
        Take();
        return {};
    }

    Value Postfix(Value v)
    {
        while (Peek() == QStringLiteral("."))
        {
            Take();
            const QString sw = Take();
            const QString nt = TypeOf(v.node);
            const bool color = nt == QStringLiteral("Texture2D") || nt == QStringLiteral("ColorParam") || nt == QStringLiteral("ObjectColor");
            if (color && sw == QStringLiteral("rgb")) { v.pin = QStringLiteral("RGB"); v.size = 3; continue; }
            if (color && sw == QStringLiteral("rgba")) { v.pin = QStringLiteral("RGBA"); v.size = 4; continue; }
            if (color && nt != QStringLiteral("Texture2D") && sw == QStringLiteral("a")) { v.pin = QStringLiteral("A"); v.size = 1; continue; }
            if (nt == QStringLiteral("Texture2D") && sw.size() == 1 && QStringLiteral("rgba").contains(sw))
            {
                v.pin = sw.toUpper(); v.size = 1; continue;
            }
            if (sw.size() == 1 && QStringLiteral("xyzw").contains(sw))
            {
                if (v.size > 1)
                {
                    const int idx = QStringLiteral("xyzw").indexOf(sw);
                    const int id = Make(QStringLiteral("Split"), QStringLiteral("Split"));
                    Link(v, id, QStringLiteral("In"));
                    v = {id, QString(QStringLiteral("RGBA")[idx]), 1};
                }
                continue;
            }
            bool trunc = !sw.isEmpty();
            for (const QChar c : sw) if (!QStringLiteral("xyzw").contains(c)) trunc = false;
            if (trunc) { v.size = sw.size(); continue; } // усечение Convert: компилятор повторит
            Fail(QStringLiteral("unsupported swizzle '.%1'").arg(sw));
        }
        return v;
    }

    Value Ident(const QString& name)
    {
        if (Peek() == QStringLiteral(".") && Peek(1) == QStringLiteral("Sample") && Peek(2) == QStringLiteral("("))
            return TextureSample(name);
        if (name == QStringLiteral("i"))
        {
            if (Peek() == QStringLiteral("."))
            {
                Take();
                const QString field = Take();
                return Postfix(InputNode(field));
            }
            return {-1, QStringLiteral("i"), 0};
        }
        if (name == QStringLiteral("defaults") && Peek() == QStringLiteral("."))
        {
            Take();
            const QString field = Take();
            const int id = Make(QStringLiteral("SurfaceDefaults"), QStringLiteral("Surface Defaults"));
            if (field == QStringLiteral("Alpha")) return {id, QStringLiteral("Alpha"), 1};
            if (field == QStringLiteral("Normal")) return {id, QStringLiteral("Normal"), 3};
            return {id, QStringLiteral("Albedo"), 3};
        }
        if (Peek() == QStringLiteral("(")) return Function(name);
        if (params_.contains(name))
        {
            const int id = params_.value(name);
            const QString nt = TypeOf(id);
            if (nt == QStringLiteral("ColorParam")) return Postfix({id, QStringLiteral("RGBA"), 4});
            if (nt == QStringLiteral("Vector2Param")) return {id, QStringLiteral("Value"), 2};
            if (nt == QStringLiteral("Vector3Param")) return {id, QStringLiteral("Value"), 3};
            return {id, QStringLiteral("Value"), 1};
        }
        Fail(QStringLiteral("unknown identifier '%1'").arg(name));
        return {};
    }

    Value InputNode(const QString& field)
    {
        if (field == QStringLiteral("Time")) return {Make(QStringLiteral("Time"), QStringLiteral("Time")), QStringLiteral("Time"), 1};
        if (field == QStringLiteral("Normal")) return {Make(QStringLiteral("SurfaceNormal"), QStringLiteral("Surface Normal")), QStringLiteral("Normal"), 3};
        if (field == QStringLiteral("ViewDir")) return {Make(QStringLiteral("ViewDirection"), QStringLiteral("View Direction")), QStringLiteral("Direction"), 3};
        if (field == QStringLiteral("WorldPos")) return {Make(QStringLiteral("WorldPosition"), QStringLiteral("World Position")), QStringLiteral("Position"), 3};
        if (field == QStringLiteral("Color")) return {Make(QStringLiteral("ObjectColor"), QStringLiteral("Object Color")), QStringLiteral("RGBA"), 4};
        if (field == QStringLiteral("UV")) return {Make(QStringLiteral("UVCoord"), QStringLiteral("UV Coord")), QStringLiteral("UV"), 2};
        Fail(QStringLiteral("unsupported input 'i.%1'").arg(field));
        return {};
    }

    Value TextureSample(const QString& name)
    {
        Take(); Take(); Take(); // . Sample (
        Expect(QStringLiteral("MaterialSampler"));
        Expect(QStringLiteral(","));
        int tex = textures_.contains(name) ? textures_.value(name)
                                           : Make(QStringLiteral("Texture2D"), name, {{QStringLiteral("textureName"), name}});
        textures_.insert(name, tex);
        const bool defaultUv = Peek() == QStringLiteral("i") && Peek(1) == QStringLiteral(".") && Peek(2) == QStringLiteral("UV") && Peek(3) == QStringLiteral(")");
        if (defaultUv) { Take(); Take(); Take(); }
        else
        {
            const Value uv = AddSub();
            Link(uv, tex, QStringLiteral("UV"));
        }
        Expect(QStringLiteral(")"));
        return Postfix({tex, QStringLiteral("RGBA"), 4});
    }

    Value Function(const QString& name)
    {
        Take(); // (
        QVector<Value> args;
        if (Peek() != QStringLiteral(")"))
        {
            args.append(AddSub());
            while (Peek() == QStringLiteral(",")) { Take(); args.append(AddSub()); }
        }
        Expect(QStringLiteral(")"));
        const int argc = args.size();
        const auto arg = [&](int i) { return i < argc ? args[i] : Value{}; };

        if (IsFloatType(name))
        {
            const int n = name == QStringLiteral("float") ? 1 : name.mid(5).toInt();
            if (argc == n && n >= 2)
            {
                const int id = Make(QStringLiteral("Combine"), QStringLiteral("Combine"));
                Link(arg(0), id, QStringLiteral("R"));
                Link(arg(1), id, QStringLiteral("G"));
                if (n >= 3) Link(arg(2), id, QStringLiteral("B"));
                if (n >= 4) Link(arg(3), id, QStringLiteral("A"));
                const QString pin = n == 2 ? QStringLiteral("RG") : n == 3 ? QStringLiteral("RGB") : QStringLiteral("RGBA");
                return {id, pin, n};
            }
            return arg(0); // добивка Convert floatN(x, 0..): снята
        }
        static const QHash<QString, QString> unary{
            {QStringLiteral("saturate"), QStringLiteral("Saturate")}, {QStringLiteral("abs"), QStringLiteral("Abs")},
            {QStringLiteral("frac"), QStringLiteral("Frac")}, {QStringLiteral("sin"), QStringLiteral("Sine")},
            {QStringLiteral("normalize"), QStringLiteral("Normalize")}, {QStringLiteral("sign"), QStringLiteral("Normalize")}};
        if (unary.contains(name) && argc >= 1) return Unary(unary.value(name), arg(0));
        if (argc >= 2)
        {
            if (name == QStringLiteral("pow")) return Binary(QStringLiteral("Power"), arg(0), arg(1), QStringLiteral("Base"), QStringLiteral("Exp"));
            if (name == QStringLiteral("min")) return Binary(QStringLiteral("Min"), arg(0), arg(1), QStringLiteral("A"), QStringLiteral("B"));
            if (name == QStringLiteral("max")) return Binary(QStringLiteral("Max"), arg(0), arg(1), QStringLiteral("A"), QStringLiteral("B"));
            if (name == QStringLiteral("step")) return Binary(QStringLiteral("Step"), arg(0), arg(1), QStringLiteral("Edge"), QStringLiteral("In"));
            if (name == QStringLiteral("dot"))
            {
                const int id = Make(QStringLiteral("Dot"), QStringLiteral("Dot"));
                Link(arg(0), id, QStringLiteral("A"));
                Link(arg(1), id, QStringLiteral("B"));
                return {id, QStringLiteral("Out"), 1};
            }
        }
        if (argc >= 3)
        {
            if (name == QStringLiteral("NormalFromMap"))
            {
                const int texNode = arg(1).node;
                if (GraphNodeData* n = model_.FindNode(texNode))
                {
                    n->type = QStringLiteral("NormalMap");
                    n->title = QStringLiteral("NormalMap");
                }
                if (arg(2).node >= 0 && TypeOf(arg(2).node) != QStringLiteral("Constant"))
                    Link(arg(2), texNode, QStringLiteral("Strength"));
                return {texNode, QStringLiteral("Normal"), 3};
            }
            if (name == QStringLiteral("lerp"))
            {
                const int id = Make(QStringLiteral("Lerp"), QStringLiteral("Lerp"));
                Link(arg(0), id, QStringLiteral("A"));
                Link(arg(1), id, QStringLiteral("B"));
                Link(arg(2), id, QStringLiteral("Alpha"));
                return {id, QStringLiteral("Out"), qMax(arg(0).size, arg(1).size)};
            }
            if (name == QStringLiteral("clamp") || name == QStringLiteral("smoothstep"))
            {
                const bool clamp = name == QStringLiteral("clamp");
                const int id = Make(clamp ? QStringLiteral("Clamp") : QStringLiteral("Smoothstep"), name);
                Link(arg(0), id, clamp ? QStringLiteral("In") : QStringLiteral("Min"));
                Link(arg(1), id, clamp ? QStringLiteral("Min") : QStringLiteral("Max"));
                Link(arg(2), id, clamp ? QStringLiteral("Max") : QStringLiteral("In"));
                return {id, QStringLiteral("Out"), std::max({arg(0).size, arg(1).size, arg(2).size})};
            }
        }
        Fail(QStringLiteral("unsupported function '%1'").arg(name));
        return {};
    }
};

int ParamNode(MaterialGraphModel& model, QHash<QString, int>& params, const QString& type, const QString& name, const QVariantMap& props)
{
    if (params.contains(name)) return params.value(name);
    const int id = model.AddNode(type, name, QPointF(120, 120 + params.size() * 150), props);
    params.insert(name, id);
    return id;
}

} // namespace

bool HlslToGraph::IsGenerated(const QString& hlsl)
{
    return hlsl.contains(QStringLiteral("// Generated by PainEngine Material Node Graph"));
}

HlslParseResult HlslToGraph::Parse(const QString& hlsl)
{
    HlslParseResult result;
    if (!IsGenerated(hlsl))
    {
        result.problems << QStringLiteral("HLSL is not a generated material graph shader");
        return result;
    }

    MaterialGraphModel& model = result.model;
    QHash<QString, int> params, textures;
    const int output = model.AddNode(QStringLiteral("MaterialOutput"), QStringLiteral("Material Output"), QPointF(900, 220));

    static const QRegularExpression colorRe(QStringLiteral(R"(^\s*float4\s+(\w+)\s*=\s*float4\(([^)]*)\);)"));
    static const QRegularExpression scalarRe(QStringLiteral(R"(^\s*float\s+(\w+)\s*=\s*([^;]+);)"));
    static const QRegularExpression vectorRe(QStringLiteral(R"(^\s*float([23])\s+(\w+)\s*=\s*float\1\(([^)]*)\);)"));
    static const QRegularExpression textureRe(QStringLiteral(R"(^Texture2D\s+(\w+)\s*:\s*register\(t(\d+)\);)"));
    static const QRegularExpression assignRe(QStringLiteral(R"(^\s*o\.(\w+)\s*=\s*(.+);\s*$)"));

    const QStringList lines = hlsl.split(QLatin1Char('\n'));
    for (const QString& line : lines)
    {
        auto m = colorRe.match(line);
        if (m.hasMatch())
        {
            const QStringList c = m.captured(2).split(QLatin1Char(','));
            QColor col = QColor::fromRgbF(NumValue(c.value(0).trimmed()), NumValue(c.value(1).trimmed()),
                                          NumValue(c.value(2).trimmed()), NumValue(c.value(3).trimmed()));
            ParamNode(model, params, QStringLiteral("ColorParam"), m.captured(1),
                      {{QStringLiteral("name"), m.captured(1)}, {QStringLiteral("color"), col}});
            continue;
        }
        m = vectorRe.match(line);
        if (m.hasMatch())
        {
            const int size = m.captured(1).toInt();
            QVariantList vals;
            for (const QString& s : m.captured(3).split(QLatin1Char(','))) vals << NumValue(s.trimmed());
            ParamNode(model, params, size == 2 ? QStringLiteral("Vector2Param") : QStringLiteral("Vector3Param"), m.captured(2),
                      {{QStringLiteral("name"), m.captured(2)}, {QStringLiteral("value"), vals}});
            continue;
        }
        m = scalarRe.match(line);
        if (m.hasMatch())
        {
            ParamNode(model, params, QStringLiteral("ScalarParam"), m.captured(1),
                      {{QStringLiteral("name"), m.captured(1)}, {QStringLiteral("value"), NumValue(m.captured(2).trimmed())}});
            continue;
        }
        m = textureRe.match(line);
        if (m.hasMatch())
        {
            const QString name = m.captured(1);
            if (!textures.contains(name))
                textures.insert(name, model.AddNode(QStringLiteral("Texture2D"), name, QPointF(120, 400 + textures.size() * 150),
                                                    {{QStringLiteral("textureName"), name}, {QStringLiteral("slot"), m.captured(2).toInt()}}));
            continue;
        }
    }

    static const QHash<QString, QString> fieldPin{
        {QStringLiteral("Albedo"), QStringLiteral("Albedo")}, {QStringLiteral("Alpha"), QStringLiteral("Alpha")},
        {QStringLiteral("SpecularWeight"), QStringLiteral("Specular")}, {QStringLiteral("AmbientModel"), QStringLiteral("AmbientModel")},
        {QStringLiteral("Emissive"), QStringLiteral("Emissive")}, {QStringLiteral("Normal"), QStringLiteral("Normal")}};

    for (const QString& line : lines)
    {
        const auto m = assignRe.match(line);
        if (!m.hasMatch()) continue;
        const QString field = m.captured(1);
        if (!fieldPin.contains(field)) continue;
        QString rhs = m.captured(2).trimmed();
        const bool saturate = field != QStringLiteral("Emissive") && field != QStringLiteral("Normal");
        if (saturate && rhs.startsWith(QStringLiteral("saturate(")) && rhs.endsWith(QLatin1Char(')')))
            rhs = rhs.mid(9, rhs.size() - 10).trimmed();
        const QVector<Token> tokens = Tokenize(rhs);
        ExprParser parser(model, tokens, result.problems, params, textures);
        const Value v = parser.Parse();
        if (!parser.AtEnd()) result.problems << QStringLiteral("trailing tokens in o.%1").arg(field);
        if (v.node >= 0) model.AddConnection(v.node, v.pin, output, fieldPin.value(field));
    }

    result.ok = result.problems.isEmpty();
    return result;
}

} // namespace EditorCore