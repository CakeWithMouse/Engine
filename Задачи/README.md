# Задачи по материалам `Правки.md`

Этот каталог раскладывает большой архитектурный разбор из `Правки.md` на отдельные рабочие карточки. Исходный документ остается первоисточником; здесь задачи сгруппированы так, чтобы их можно было брать в работу по одной.

## Рекомендуемый порядок

1. `P0` lifecycle: штатное завершение, отказ при ошибках инициализации, безопасные базовые указатели.
2. `P1` воспроизводимость и контракты кадра: конфигурации, resize, delta time, shader variants, input layout, прозрачность, свет/тени.
3. `P1` владение и импорт: владелец объектов/ресурсов, регистрация, transforms, FBX/import cache, input snapshot.
4. `P2` измерения и оптимизация: frame constants, instance capacity, particle sort, visibility/batching, G-buffer, HLSL policy, telemetry.

## Список задач

| ID | Приоритет | Задача |
|---|---|---|
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

