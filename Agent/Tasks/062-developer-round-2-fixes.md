# 062 — Повторный круг четырёх разработчиков и исправления .5

Запрос: повторить работу персон и починку найденных багов, потолок недельного
счётчика поднят пользователем до 50%. Навыки: try-engine и
fix-engine-after-developers. Используются 3 GPT-6-Luna medium агента;
CalmCity и AITrash идут последовательно на одном агенте.

Локальный SDK 2026.11.05.5 (промежуточные проверенные сборки .2/.3/.4 сохранены):

- Исправлен screen-size LOD ортографической камеры.
- SaveGame восстанавливает input scope отключённого CharacterController.
- Default InstancedMesh shader учитывает transforms экземпляров; F7/F8
  используют общий ввод в обоих host modes.
- Primitive CreateInstance без legacy shader использует штатный mesh/material
  путь и переносит копии как с материалом, так и без него.
- Mesh, InstancedMesh и FBX загружают текстуру, заданную до подключения Game,
  при создании GPU-ресурсов; восстановление сцены не требует повторного setter.
- EngineCommandlets расширен render.diagnostics, project.input,
  material.graph, project.scene; мышь принимает реальные Engine Keys 500+.
- Новые сценарии/регрессии проверяют реальные Engine/D3D/Qt операции;
  сообщения hot reload сохраняют исходную причину ошибки.

Дневники, баги, исходники, before/after и матрица покрытия:
[прогон r2](../Playtests/2026-11-05-2026.11.05.1-r2/report.md).

Изменения не опубликованы в EngineStore, коммит и PR этим запросом не запрошены.
