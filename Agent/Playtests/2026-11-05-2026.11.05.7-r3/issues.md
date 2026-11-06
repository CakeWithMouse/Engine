# Журнал r3

Круг завершён с ограничением покрытия по бюджету. Ошибки исходников игровых проб и окружения отделены от дефектов Engine.

## LIM001 → реализовано в .7

Исходное ограничение из `2026-11-05-imported-models/report.md`: первый материал назначался всей
импортированной модели. Реализованы material IDs, слоты, независимые render passes, UI, сцена,
SaveGame и headless-вызовы. Проверки и кадры — [fixes/README.md](fixes/README.md).

## HARNESS-01 — SaveGame probe без Configure

Категория user-code-error, координатор. Первый новый тест вызвал Capture до Configure и получил
`Configure SaveGame first`. Добавлена Configure с изолированным Save root; повтор прошёл.
Исходный лог: `Engine/build/imported-model-probe/multimaterial-assets.log`.

## HARNESS-02 — неполная внешняя FBX-fixture

Категория user-code-error, координатор. Первый MaterialSlotsCommandletTests взял ASCII FBX из
fixtures без создаваемой AssetTests внешней `maps/цвет.bmp`. Import корректно отказал и удалил
новую папку. Runner теперь требует явный Model с зависимостями; повтор на self-contained BLEND
прошёл. Первые результаты: `Engine/build/playtests/20261105-r3/slot-commandlets`;
успешные: `fixes/evidence/slot-commandlets`.

## OBS-01 — первая статистика кадра ещё не готова (закрыто, ожидаемое поведение)

Персона 1 сначала связала отсутствие снимка в коротком запуске с `CompletedFrameStats.valid`.
Повтор на том же frozen .7 с исходным условием и трассировкой первых восьми PreUpdate:
первый вызов valid=0/frame=0, второй valid=1/frame=1, следующие frame=2..7. Снимок записан,
exit 0. Дефект не подтверждён; обход в игровом коде снят. См. calm-city/diary.md и evidence.

## BUG-R3-001 — слоты пользовательского Skeletal-наследника не загружаются

Категория defect, P1, Runtime/SDK .7 Release Assimp. Персона 4 создаёт зарегистрированный
наследник SkeletalMeshComponent, загружает модель с двумя материалами, сохраняет во время
полёта и запрашивает Load. Ожидание: вернуть объект того же типа, геометрию и назначения.
Факт: SaveGame Failed, `Material slots do not match model`; попытка игрового кода продолжить
привела к RuntimeError. Независимый coordinator regression воспроизвёл ту же ошибку
через SceneDocument::Restore (derived-before.log, exit 1).

Причина: каталог создаёт пустой пользовательский объект, а Restore читает projectData
до model asset. Дополнительно обычный SetModelAsset создавал visual proxy вместо заполнения
самого Skeletal-наследника. Та же последовательность есть в batch duplicate/remapping.
Исправление .8: SceneAssets применяет модель непосредственно к SkeletalMeshComponent;
Restore и remapping загружают его assets перед codec, порядок прочих классов сохранён.
Добавлены регрессии сцены, SaveGame и batch duplicate на пользовательском типе.
Статус: исправлено и проверено на .8. Assets: 506 checks PASS, включая derived Scene/SaveGame/
batch duplicate; общий SaveGame 78 + cross-process, Core 54/54, native EditorUiTests 5034 PASS.
Персона 4 загрузила в отдельном процессе .8 именно ошибочный файл .7 (с правильным typeName):
elapsed=0.5, throws=1, затем полёт продолжился и hits=1. Новый .8 save/load также PASS.
Персона 2 отдельно прочитала оба слота после SaveGame Load в двух процессах .8.
Before/after: fixes/evidence/derived-before.log, derived-after.log, final8-assets.log;
игровой повтор и исходный JSON — ai-trash/evidence.

## HARNESS-03 — предпросмотр BMP создаёт ложное впечатление потери HUD

Персона 2, .7, `ui.edit-text` изменяет только текст кнопки `buildHouse`.
Предпросмотр BMP инструментом показал исчезнувшие Panel/Label/Save/Load. Проверка исходных
пикселей через System.Drawing и lossless-конверсия в PNG показывает полный HUD в обоих кадрах.
В ROI панели (16,16)–(345,221) изменились 758 пикселей; bbox изменений (27,114)–(124,130),
то есть только текст редактируемой кнопки. Остальная панель побитово одинакова.
Статус: дефект движка опровергнут, для визуальных свидетельств использовать PNG.
Исходные пары: Engine/build/playtests/20261105-r3/curious-city/hud-ab-{before,after}.{bmp,png}.

## LIMIT-02 — символы вне атласа HUD

В той же пробе текст `Дом • 100 ₽` сохраняется в XML полностью, но • и ₽ не рисуются.
Это documented-limit, а не потеря Unicode при сохранении: Engine/docs/UI.md документирует
ASCII, А-я, Ё/ё. Пожелание: расширяемый набор глифов/fallback. В этом выпуске не реализовано.

## Ошибки игровых проектов и подготовки (не дефекты Engine)

| ID | Категория / персона | Факт и повтор |
|---|---|---|
| GAME-R3-01 | user-code-error / градосимулятор | Не включён glm для объявления vec3, вызван отсутствующий GetName. Диагностика компилятора сохранена, исходники исправлены, новая DLL собрана и активирована. |
| GAME-R3-02 | user-code-error / градосимулятор | Ранний return при наличии стартового Cube не устанавливал CityControls. После исправления реальные клики строят дома и меняют баланс. |
| GAME-R3-03 | user-code-error / броски | C2039: TrashFlight не имел GetLocalPosition. Добавлен собственный GetFlightPosition, повторная сборка прошла. |
| GAME-R3-04 | user-code-error / броски | Импортированный наследник зарегистрирован в сцене строкой чужого типа Trash.Projectile; Load создавал другой класс и давал `Component has no project data codec`. Исправлена строка типа. Следующее отдельное падение привело к BUG-R3-001. |
| GAME-R3-05 | user-code-error / броски | LoadFromVfs создаёт пустые слоты, а игровая перестановка читала изменяемую map по ссылке. Назначены явные пути материалов, исходный неверный A/B с разными цветами отброшен; правильный shadow A/B отличался только castShadows. |
| HARNESS-R3-04 | environment / бомбардировка | Перекрылись sandbox enable-code и второй build: занят CodeBuild.log; первый дал LNK1101/MSPDB. После завершения процессов один запуск в обычном MSVC-окружении прошёл. SDK не менялся. |
| HARNESS-R3-05 | fixture-format / градосимулятор .8 | PowerShell записал ID как 5.0; валидация кандидата отказала `Expected unsigned integer ID`, прежняя DLL сохранена. JSON исправлен; rebuild/activation и игровой повтор прошли. |
| HARNESS-R3-06 | invocation / градосимулятор | Recovery harness получил Project.json вместо папки, затем неверный путь launcher. Правильные команды на новых nocode fixtures прошли workflow/recovery и authoring. |
| HARNESS-R3-07 | environment / броски | Native UI host.Start отказал при прежнем окружении. Повтор с явно заданным существующим Windows TEMP/TMP и обычным окружением прошёл recovery/authoring. Точная первоначальная переменная TEMP не зафиксирована; причинность ошибки Qt не утверждается. |
| HARNESS-R3-08 | probe setup / обе роли | Transform передан в property вместо move; луч попал в Cube вместо Ground; UV-плоскость стояла ребром. Исправлены запрос/точка луча/камера; частичные визуальные проверки так и оставлены partial. |

Полная диагностика и дополнительные шаги подготовки:
[градосимулятор](curious-city/issues.md), [бомбардировка](tired-bomber/diary.md),
[броски](ai-trash/issues.md), [спокойный город](calm-city/diary.md).
Намеренные отрицательные сценарии с правильным отказом не считаются багами.

**Итог дефектов Engine:** один новый подтверждённый дефект (BUG-R3-001), исправлен и проверен
на .8. LIM001 реализован по запросу. Остальные наблюдения выше — ошибки игрового кода,
подготовки, окружения либо документированные ограничения.
