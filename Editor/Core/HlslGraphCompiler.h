#pragma once

#include "MaterialGraphModel.h"
#include <QHash>
#include <QList>
#include <QString>

namespace EditorCore
{

// HLSL графа и откуда взялась каждая строка: ошибку компилятора можно показать на ноде.
struct GraphCompileResult
{
    QString hlsl;
    QHash<int, QList<int>> lineNodes; // строка HLSL (с 1) -> ноды, из которых она собрана; первая — главная
    QHash<int, QString> lineOutputs;  // строка присваивания -> вход MaterialOutput
    QHash<int, QString> problems;     // нода -> ошибка графа, найденная до компиляции HLSL
};

class HlslGraphCompiler
{
public:
    static GraphCompileResult Compile(const MaterialGraphModel& model);
    static QString CompileGraphToHlsl(const MaterialGraphModel& model) { return Compile(model).hlsl; }

    // Пины ноды, общие для редактора и компилятора. Тип пина: float, float2, float3, float4 или any
    // (размер берётся по подключённым входам). Связь допустима всегда: компилятор приводит размеры сам.
    static QVector<GraphPin> NodePins(const QString& type);
    static bool IsKnownNode(const QString& type);

    // Почему имя параметра или текстуры не годится для HLSL; пустая строка — годится.
    // Проверяет синтаксис, ключевые слова и имена, занятые шаблоном материала движка.
    static QString NameProblem(const QString& name);
};

} // namespace EditorCore
