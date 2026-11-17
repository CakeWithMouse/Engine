#pragma once

#include "MaterialGraphModel.h"

#include <QString>

namespace EditorCore
{

// Готовые шаблоны материала, собираемые в Core без UI. «Simple» — плоский цвет без текстуры,
// «ORM» — упакованная текстура (R=AO, G=Roughness, B=Metallic), распакованная Split-нодой.
// Движок освещения Блинн-Фонг: AO идёт в AmbientModel, roughness — в SpecularWeight как (1-roughness)
// (соглашение пресета), metallic распаковывается, но своего входа в движке не имеет (см. «Нужно в ABI»).
class MaterialGraphPresets
{
public:
    static MaterialGraphModel BuildSimple();
    static MaterialGraphModel BuildOrm();

    // Имя упакованной текстуры ORM-пресета (для назначения файла и автотеста распаковки).
    static QString OrmTextureName();
};

} // namespace EditorCore