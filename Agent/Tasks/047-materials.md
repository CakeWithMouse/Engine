# 047 — Материалы и редактор материалов

- Рендер: HLSL-контракт поверхности (`Shaders/Material`), общий шаблон с forward/deferred, тенями, инстансингом и скиннингом; режимы opaque/masked/translucent/additive, unlit, двусторонность, излучение; шахматка вместо материала с ошибкой компиляции.
- Ресурсы: файлы `.mat` (JSON) с параметрами и текстурами, кэш материалов игры, материал по умолчанию, F6 — перекомпиляция шейдеров и материалов с диска.
- Компоненты: `MeshComponent` и процедурные `MeshPrimitives`; материалы у статических, скелетных и инстансных моделей, `MeshInstance::CustomData`; запекание спрайтов LOD через материал. Примитивы SceneProject рисуются мешами с материалом по умолчанию.
- Scene: необязательное поле `assets.material` в JSON v1, восстановление при загрузке/Play/Undo/Redo/дублировании.
- Runtime: Edit API v6 (назначение, чтение и правка материала, сохранение/откат, перекомпиляция файла, превью) и Scene API v4 (`materialAsset` в снимке).
- Editor: поле **Материал** в свойствах объекта, окно материала с превью, текстом HLSL, параметрами, текстурами и ошибками; создание материала из Files и из материала по умолчанию.
- Примеры: сцена «Материалы» в BasicExamples, техника и постройки Battle.
- Тесты: EngineTests, `Engine/tests/assets/run.ps1 -EnableAssimp`, `Editor/tests/run.ps1 -EnableAssimp`, `Engine/tests/project-workflow/run.ps1`.

Пользовательский процесс, формат `.mat`, HLSL-контракт, API для кода игры и обновление старых проектов: [MATERIALS](../../Engine/docs/MATERIALS.md).

Проверено в Debug, MSVC v145, Qt 6.7:

- Engine: 43/43; каждый промежуточный коммит ветки собран с нуля и проходит EngineTests.
- Asset workflow с Assimp: 123 проверки.
- Qt UI: 1756 проверок.
- Project workflow: 129 проверок, затем `--check-project`, `--smoke-project` и `--smoke-project-editor`.

Release в рамках задачи не проверялся. Уже созданные проекты требуют обновления `Game.dll` из шаблона: новый редактор отклоняет DLL со Scene API v3.
