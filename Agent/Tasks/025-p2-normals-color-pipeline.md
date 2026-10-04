# P2. Нормали и цветовой пайплайн ограничивают качество

## Где

`BaseFBX.hlsl` и другие material VS/PS, texture creation, G-buffer formats.

## Проблема

Нормаль FBX преобразуется через `(float3x3)worldMatrix`; при неравномерном scale это не эквивалентно inverse-transpose normal matrix. Для примитивов normal generation зависит от конкретного shader. Изображения и back buffer используют UNORM; финальный цвет часто ограничивается `saturate`, mip chain для текстур не строится.

## Исправление

Сделать общий normal matrix contract и политику normal orientation; явно отделить цветовые текстуры от линейных data textures. При необходимости ввести linear HDR intermediate/tone mapping. Mipmaps добавлять с определенным ownership и загрузочным этапом.

## Критерий приемки

Нормали корректны при неравномерном scale; текстуры имеют предсказуемую color-space policy; дальние текстуры не деградируют из-за отсутствия mipmaps.

