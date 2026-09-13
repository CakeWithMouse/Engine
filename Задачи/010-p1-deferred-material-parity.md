# P1. Deferred не сохраняет свойства forward-материала

## Где

`Shaders/ReflectiveSphere.hlsl`, `ReflectiveFBX.hlsl`, `RotatedFigure.hlsl`; `Game::UpdateDeferredLightingBuffer`.

## Проблема

Reflective G-buffer branches пишут только albedo, normal и world position. Общий deferred lighting не получает cubemap/reflection strength/Fresnel parameters и не вычисляет соответствующие отражения. Ambient задан иначе, чем в component forward buffers. Поэтому переключение пути меняет модель освещения, а не только способ ее вычисления.

## Исправление

Определить общий lighting/material contract. Выбрать, какие свойства кодируются в G-buffer, а какие рисуются отдельным forward-проходом. Вынести общие lighting functions и параметры. Если reflective остается forward-only, исключать его из G-buffer и правильно композить с depth.

## Критерий приемки

Для заявленной общей модели материала оба пути дают сопоставимое изображение; исключения названы явно.

