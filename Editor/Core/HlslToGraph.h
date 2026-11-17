#pragma once

#include "MaterialGraphModel.h"

#include <QString>
#include <QStringList>

namespace EditorCore
{

struct HlslParseResult
{
    MaterialGraphModel model;
    QStringList problems; // нераспознанные конструкции; пустой список — успех
    bool ok = false;
};

// Обратный разбор HLSL, собранного HlslGraphCompiler, в нодовый граф материала.
// Понимает объявления параметров/текстур и присваивания выходам o.Albedo/Alpha/...
// вместе с инлайновыми выражениями (математика, сэмплы, Split, OneMinus, приведения).
// Чистая логика Core: результат эквивалентен исходному графу по HLSL, не по раскладке нод.
class HlslToGraph
{
public:
    static HlslParseResult Parse(const QString& hlsl);
    static bool IsGenerated(const QString& hlsl);
};

} // namespace EditorCore