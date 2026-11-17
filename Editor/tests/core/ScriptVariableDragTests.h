#pragma once

#include "../../Core/ScriptVariableDrag.h"

#include <QMimeData>
#include <QString>
#include <cstdio>
#include <cstdlib>

namespace
{

int gScriptVariableDragChecks = 0;

void CheckVariableDrag(bool cond, const char* msg)
{
    ++gScriptVariableDragChecks;
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (ScriptVariableDrag): %s\n", msg);
        std::exit(1);
    }
}

void TestVariableDragRoundTrip()
{
    EditorCore::ScriptVariableDrag::Payload in;
    in.name = QStringLiteral("Health");
    in.type = QStringLiteral("float");
    EditorCore::ScriptVariableDrag::Payload out;
    CheckVariableDrag(EditorCore::ScriptVariableDrag::Decode(EditorCore::ScriptVariableDrag::Encode(in), out),
                      "variable payload round-trip decodes");
    CheckVariableDrag(out.name == QStringLiteral("Health"), "variable name preserved");
    CheckVariableDrag(out.type == QStringLiteral("float"), "variable type preserved");
}

void TestVariableDragUnicodeAndTypes()
{
    for (const QString& type : {QStringLiteral("bool"), QStringLiteral("vec3"), QStringLiteral("string"), QString()})
    {
        EditorCore::ScriptVariableDrag::Payload in;
        in.name = QStringLiteral("Скорость");
        in.type = type;
        EditorCore::ScriptVariableDrag::Payload out;
        CheckVariableDrag(EditorCore::ScriptVariableDrag::Decode(EditorCore::ScriptVariableDrag::Encode(in), out),
                          "unicode variable payload decodes");
        CheckVariableDrag(out.name == QStringLiteral("Скорость"), "unicode variable name preserved");
        CheckVariableDrag(out.type == type, "variable type preserved across kinds");
    }
}

void TestVariableDragRejectsBadData()
{
    EditorCore::ScriptVariableDrag::Payload out;
    CheckVariableDrag(!EditorCore::ScriptVariableDrag::Decode(QByteArray("not json"), out), "garbage rejected");
    CheckVariableDrag(!EditorCore::ScriptVariableDrag::Decode(QByteArray("[1,2]"), out), "json array rejected");
    CheckVariableDrag(!EditorCore::ScriptVariableDrag::Decode(QByteArray("{}"), out), "missing name rejected");
    CheckVariableDrag(!EditorCore::ScriptVariableDrag::Decode(QByteArray("{\"name\":\"\"}"), out), "empty name rejected");
    CheckVariableDrag(!EditorCore::ScriptVariableDrag::Decode(QByteArray("{\"name\":5}"), out), "non-string name rejected");
}

void TestVariableDragMime()
{
    QMimeData mime;
    CheckVariableDrag(!EditorCore::ScriptVariableDrag::HasMime(mime), "empty mime has no variable format");
    CheckVariableDrag(EditorCore::ScriptVariableDrag::FromMime(mime).name.isEmpty(), "absent mime gives empty payload");

    EditorCore::ScriptVariableDrag::Payload in;
    in.name = QStringLiteral("Ready");
    in.type = QStringLiteral("bool");
    mime.setData(QLatin1String(EditorCore::ScriptVariableDrag::kMimeType), EditorCore::ScriptVariableDrag::Encode(in));
    CheckVariableDrag(EditorCore::ScriptVariableDrag::HasMime(mime), "variable mime detected");
    const EditorCore::ScriptVariableDrag::Payload out = EditorCore::ScriptVariableDrag::FromMime(mime);
    CheckVariableDrag(out.name == QStringLiteral("Ready") && out.type == QStringLiteral("bool"), "FromMime decodes payload");
}

inline int RunScriptVariableDragTests()
{
    TestVariableDragRoundTrip();
    TestVariableDragUnicodeAndTypes();
    TestVariableDragRejectsBadData();
    TestVariableDragMime();
    return gScriptVariableDragChecks;
}

} // namespace