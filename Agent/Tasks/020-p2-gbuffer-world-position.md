# P2. G-buffer тратит bandwidth на world position

## Где

`Game::InitDeferredResources`, `RotatedFigure.hlsl`, G-buffer branches материалов.

## Проблема

Три MRT занимают 4+8+8 = 20 байт на пиксель, depth - еще 4 байта. При 1920x1080 это примерно 39.6 MiB цветовых буферов и 7.9 MiB depth. Третий target называется `Material`, но хранит half-float world position. В координатах большой величины half precision ухудшает точность; при этом depth и inverse matrices уже есть.

## Исправление

Сначала переименовать ресурс по реальному назначению или явно определить новый G-buffer contract. Затем сравнить восстановление позиции из depth с текущим хранением на нужном диапазоне камеры; при подходящей точности освободить target либо использовать его для material properties.

## Метрики

GPU time geometry/lighting при нескольких разрешениях и визуальная ошибка восстановленной позиции.

