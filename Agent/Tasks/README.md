# Задачи по материалам `../Reviews/Правки.md`

Этот каталог раскладывает большой архитектурный разбор из `../Reviews/Правки.md` на отдельные рабочие карточки. Исходный документ остается первоисточником; здесь задачи сгруппированы так, чтобы их можно было брать в работу по одной.

## Рекомендуемый порядок

1. `P0` lifecycle: штатное завершение, отказ при ошибках инициализации, безопасные базовые указатели.
2. `P1` воспроизводимость и контракты кадра: конфигурации, resize, delta time, shader variants, input layout, прозрачность, свет/тени.
3. `P1` владение и импорт: владелец объектов/ресурсов, регистрация, transforms, FBX/import cache, input snapshot.
4. `P2` измерения и оптимизация: frame constants, instance capacity, particle sort, visibility/batching, G-buffer, HLSL policy, telemetry.

## Список задач

| ID | Приоритет | Задача |
|---|---|---|
| 074 | P1 | [Новый AI-редактор и закрытие 48 partial](074-developer-round-7-ai-editor.md) |
| 001 | P0 | [WM_QUIT не останавливает внешний цикл](001-p0-wm-quit-loop.md) |
| 002 | P0 | [Ошибки инициализации не останавливают запуск](002-p0-initialization-failures.md) |
| 003 | P1 | [Окружение не воспроизводится из одного репозитория](003-p1-reproducible-environment.md) |
| 004 | P1 | [Resize объявлен, но не реализован](004-p1-resize-contract.md) |
| 005 | P1 | [Время симуляции зависит от FPS](005-p1-frame-delta-time.md) |
| 006 | P1 | [RenderType и shader variants расходятся](006-p1-render-type-shader-variants.md) |
| 007 | P1 | [Input layout зависит от предыдущего draw](007-p1-input-layout-state.md) |
| 008 | P1 | [Прозрачность не имеет полноценного прохода](008-p1-transparency-pass.md) |
| 009 | P1 | [Directional light и shadows обновляются несогласованно](009-p1-directional-light-shadow-snapshot.md) |
| 010 | P1 | [Deferred теряет свойства forward-материала](010-p1-deferred-material-parity.md) |
| 011 | P0 | [CreateInstance оставляет GamePtr неинициализированным](011-p0-create-instance-gameptr.md) |
| 012 | P1 | [Ресурсы и объекты не имеют единого владельца](012-p1-resource-ownership.md) |
| 013 | P1 | [Повторная регистрация сообщает ложный успех](013-p1-register-component-duplicates.md) |
| 014 | P1 | [Transform cache может вернуть старую позицию](014-p1-transform-cache-invalidation.md) |
| 015 | P1 | [Инстансинг и тени рисуют разную геометрию](015-p1-instancing-shadow-parity.md) |
| 016 | P2 | [Общие данные кадра пересчитываются для каждого объекта](016-p2-frame-constants.md) |
| 017 | P2 | [Инстансы постоянно пересоздают GPU buffer](017-p2-instance-buffer-capacity.md) |
| 018 | P2 | [Bitonic-сортировка частиц дорога по dispatch](018-p2-particle-sort-dispatch.md) |
| 019 | P2 | [Нет visibility culling и группировки draw packets](019-p2-visibility-and-batching.md) |
| 020 | P2 | [G-buffer хранит world position отдельным target](020-p2-gbuffer-world-position.md) |
| 021 | P2 | [HLSL всегда компилируется без оптимизации](021-p2-hlsl-optimization-policy.md) |
| 022 | P2 | [Постоянные расходы и неполная телеметрия](022-p2-telemetry-and-lazy-resources.md) |
| 023 | P1 | [Импорт теряет node transforms и часть материалов](023-p1-fbx-import-transforms-materials.md) |
| 024 | P1/P2 | [FBX cache имеет скрытые предусловия](024-p1p2-fbx-cache-contract.md) |
| 025 | P2 | [Нормали и цветовой пайплайн ограничивают качество](025-p2-normals-color-pipeline.md) |
| 026 | P1/P2 | [Mouse offset теряет события](026-p1p2-input-snapshot.md) |
| 027 | P1 | [Разделить Engine и GameExample, перенести SunGame](027-engine-game-separation-roadmap.md) |
| 028 | P2 | [Улучшения после разделения: legacy, ресурсы, стратегия, DLL](028-separation-followups.md) |
| 029 | Развитие | [Самостоятельные игры и графический лаунчер](029-games-and-launcher.md) |
| 030 | Организация | [Корень Engine / Game / Agent](030-repository-layout.md) |
| 031 | Архитектура | [Перенести управление runtime в Engine](031-engine-runtime.md) |
| 032 | Архитектура | [Один EngineRuntime.exe: игра и режим редактирования](032-runtime-game-editor-modes.md) — [промпт выполнения](032-execution-prompt.md) |
| 033 | Рендер | [Массовая техника: инстансинг мешей и LOD](033-mass-unit-rendering.md) |
| 034 | Рендер | [Стоимость теней: каскады для техники и запечённая карта](034-shadow-cost.md) |
| 035 | Редактор | [Сцены как данные, JSON и встроенный Play: этапы](035-scene-editor-roadmap.md) |
| 036 | Редактор | [Статичный preview, Space Play/Stop, P Pause](036-editor-preview-play.md) |
| 037 | Ядро | [Scene, ID и API для редактора; без SceneObject/JSON](037-scene-core.md) |
| 038 | Ядро | [Обнаружение классов, генератор и каталог компонентов](038-component-generation.md) |
| 039 | Ядро/runtime | [Каталог игрового модуля и команды Scene](039-scene-module-commands.md) |
| 040 | Ядро/runtime | [Edit-сессия, viewport и Play API без изменений Editor](040-edit-session-api.md) |
| 041 | P1 | [JSON сцен и файловый API — выполнено](041-scene-json.md) |
| 042 | Ядро/runtime | [startupScene и загрузка при --scene-api — выполнено](042-startup-scene.md) |
| 043 | Ядро/runtime | [startupScene в game mode — выполнено](043-game-startup-scene.md) |
| 044 | Ядро/runtime | [Отображение startupScene в Game — выполнено](044-game-scene-rendering.md) |

- [045 — Пользовательские проекты и инструменты сцены](045-user-projects.md) — реализовано, Debug проверен.
- [046 — Импорт моделей и текстур в редактор](046-project-assets.md).
- [047 — Материалы и редактор материалов](047-materials.md).
- [047 — Проекты с C++ и пользовательские типы в редакторе](047-code-projects-and-editor-types.md) — реализованы два режима, переход к C++, SDK, Game/Component/Object и таблица видимости; результаты тестов в задаче.
- [048 — Частицы, скайбокс, категории и видимое выделение](048-editor-effects-and-selection.md) — реализовано; Debug/Release, D3D, Qt и совместимость проверены.

- [049 — Сохранение, старые проекты на новом runtime, экспорт и группы](049-reliable-workflow-export-groups.md).

- [050 — Переимпорт ресурсов и сворачиваемые свойства классов](050-asset-updates-and-inspector.md).

- [051 — Туториал свойств и перемещение панели контента](051-properties-tutorial-and-content-dock.md).

- [053 — Игрок и камера в свойствах](053-player-camera-properties.md): реализовано, проверены migration/SDK/UI.
- [054 — Трассировки и строительство](054-scene-traces-building-plan.md): отдельный план, реализация не начата.

- [058: GameSubsystem и GameSession](058-game-subsystems.md) — реализованы, проверены lifecycle, генерация и внешний SDK.

- [059: CLI-диагностика launcher](059-launcher-cli-diagnostics.md) — реализована, проверены справка, ошибки и создание C++-проекта.
- [060: SaveGame сцены и базовых классов](060-savegame.md) — реализация и проверка; [руководство](../../Engine/docs/SAVEGAME.md), [HTML-схема](../../Engine/docs/SAVEGAME.html).
- [061: Статистика завершённого кадра и панель LOD](061-render-diagnostics.md) — API, F7/F8 и проверка автором бомбардировки.
- [062: Повторный круг разработчиков и исправления .5](062-developer-round-2-fixes.md) — LOD, instancing, текстуры, SaveGame/input, консольные сценарии и повторные игровые проверки.

- [063: Каскады теней и тангенты](063-shadow-cascades-and-tangents.md) — реализовано, ограничения в задаче.

- [064: Мягкие тени и тени точечных источников](064-soft-shadows-and-point-light-shadows.md) — реализовано, ограничения в задаче.

- [065: Повторный прогон тангентов и теней](065-developer-round-5-tangents.md) — исправления GPU-сигнатуры и зеркального масштаба, регрессии и сборка 2026.11.07.0.
- [066: Визуальный прогон и стабильность каскадов](066-shadow-camera-visual.md) — Edit/Game, resize, движение камеры, регрессии и сборка 2026.11.07.1.
- [067: Движение точечных теней](067-point-shadow-motion.md) — лампы, skeletal/VAT-анимации, сцены без солнца и сборка 2026.11.07.2.
- [068: Две теневые лампы и LOD](068-point-shadow-slots-lod.md) — переключение слотов, плавные mesh LOD, анимации и сборка 2026.11.07.3.
- [069: Проверка материалов](069-materials-audit.md) — граф/HLSL, текстуры, импорт, GPU-матрица, ошибки .mat и сборка 2026.11.07.4.
- [070: Прозрачные слои, камера и LOD](070-transparent-overlap-lod.md) — общая сортировка экземпляров, cross-fade и сборка 2026.11.07.5.
- [071: Пересечения прозрачных мешей](071-transparent-intersections.md) — depth peeling, самопересечения, камера и сборка 2026.11.07.6.
- [072: Освещение и звук в Edit/Play/Game](072-editor-game-lighting-audio-parity.md) — режим света после загрузки C++-сцены, остановка предпрослушивания, Pause/Resume аудио и сравнение GPU-кадров.
- [073: Общая пересборка и интеграция в main](073-main-rebuild-2026-11-08.md) — Debug/Release, SDK v142/v143/v145, запуск игр, регрессия старых дат исходников и руководства.
- [074: AI-диагностика во вьюпорте](074-ai-viewport-diagnostics.md) — восемь слоёв, Edit/Play/Pause, инспектор агента, UI-регрессия и HTML с реальными снимками редактора.
- [075: Видимость камеры и расстояния](075-camera-viewport-guides.md) — направление, границы кадра, Near/Far, масштаб схемы и обновление из инспектора.

- [072: Визуальный редактор кода на нодах](072-visual-scripting-plan.md) — общее ядро графа (материалы и скрипты), интерпретатор `.vscript` в движке, `assets.script` и расширение `Engine.Scene.Script`, окно скрипта, сцена «Скрипты» в BasicExamples; [руководство](../../Engine/docs/VISUAL-SCRIPTING.md).
- [072: Повторный круг разработчиков](072-developer-round-6.md) — старые проекты на текущем main, новые фичи, импорт и регрессии.
- [075: Разработчик стратегии — физика и ИИ](075-strategy-developer-physics-ai.md) — один новый агент, игровой эпизод и исправление его находок; остальные возможности по запросу.
