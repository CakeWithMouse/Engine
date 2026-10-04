# P1. Transform cache может вернуть старую позицию

## Где

`GameComponents.h`, `SetPosition`, `SetParent`; `GetWorldMatrix`.

## Проблема

У корня кеш возвращается при `!bTransformDirty`. `SetPosition` не инвалидирует его. После изменения позиции статичный объект может продолжать рисоваться со старой матрицей. У дочерних объектов кеш фактически не экономит вычисления. Цикл parent-ссылок приводит к неограниченной рекурсии. `GetCenter` возвращает local position, в частности light registry не учитывает parent transform.

## Исправление

Единый набор transform setters с инвалидацией; version-based world transform или вычисление иерархии один раз в топологическом порядке; запрет циклов; отдельные `GetLocalPosition`/`GetWorldPosition`.

## Критерий приемки

Любое изменение transform отражается в world matrix текущего кадра; parent cycles невозможны; системы света и collision используют нужное пространство координат.

