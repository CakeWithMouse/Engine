# P1. Импорт теряет transforms узлов и часть данных материалов

## Где

`FBXComponent::ProcessNode/ProcessMesh/ProcessMaterial/Render`, `BaseFBX.hlsl`.

## Проблема

`ProcessNode` рекурсивно проходит узлы, но не накапливает и не применяет `node->mTransformation`. Для файлов с нетривиальной иерархией геометрия будет размещена неверно. Цвет материала извлекается и хранится в mesh records, но draw не загружает его отдельно перед каждым submesh. `BaseFBX.hlsl` безусловно семплирует текстуру, не используя `HasTexture` как fallback для модели без изображения.

## Исправление

Определить import contract: bake node transforms в вершины/нормали либо сохранить render nodes с transform. Материалы оформлять по submesh с валидными default textures/constants. Отдельно определить неподдерживаемые свойства вместо молчаливой потери.

## Критерий приемки

Модели с иерархией узлов отображаются с ожидаемыми transforms; submesh material data не теряется; модель без текстуры имеет корректный fallback.

