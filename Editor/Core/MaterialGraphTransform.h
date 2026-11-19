#pragma once

#include "MaterialGraphModel.h"

#include <QList>
#include <QJsonObject>

namespace EditorCore
{

// Превращение констант графа в параметры материала: значение сохраняется, нода становится
// ScalarParam и правится без перекомпиляции (ABI setMaterialParameter). Связи Value->Value
// остаются прежними. Чистая логика Core, без UI; результат — копия модели.
class MaterialGraphTransform
{
public:
    // Есть ли среди ids нода Constant, которую можно выставить параметром.
    static bool CanExpose(const GraphDocument& model, const QList<int>& ids);
    // Копия модели, где выбранные Constant стали ScalarParam с уникальным именем.
    static MaterialGraphModel ExposeConstants(const GraphDocument& model, const QList<int>& ids);
    // Сбор параметров графа по умолчанию (ScalarParam, ColorParam, Vector2/3Param) для сохранения в .mat
    static QJsonObject ExtractDefaultParams(const GraphDocument& model);
};

} // namespace EditorCore