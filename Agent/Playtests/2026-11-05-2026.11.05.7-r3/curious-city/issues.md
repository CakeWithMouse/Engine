# Ошибки и ограничения прогона персонажа 2

## C++ build 1 — ошибка игрового кода

- Категория: `user-code-error`, исправлено.
- Runtime/конфигурация: Engine 2026.11.05.7, свежий Curious City проект, Release x64 / MSVC.
- Действие: после добавления `BuildOneHouse(Scene&, const glm::vec3&)` выполнен штатный `EngineLauncher --build-project`.
- Ожидание: скомпилировать и активировать проектную DLL.
- Факт: компиляция отклонена. `CityEconomySubsystem.h` не объявлял тип `glm::vec3`, поэтому overload не распознавался; `CityInteractionComponent.cpp` вызвал несуществующий `GameComponent::GetName()`.
- Диагностика: [launcher-build-1.stderr.log](evidence/launcher-build-1.stderr.log), исходный build.log сохранён в `Engine/build/playtests/20261105-r3/curious-city/fresh-city/Binaries/Release/36a18f1e721a47eb903c3b46e33450125/build.log`. Runtime сохранил предыдущую DLL (`Project build failed; previous module preserved`).
- Исправление: явно включён `<glm/glm.hpp>`, проверка ground делается по `FindComponentByName("Ground")` и `ComponentId`. Последующие сборки активировали модуль.

## CLI копирование staging — ошибка подготовки

- Категория: `probe-setup-error`, исправлено.
- Runtime/конфигурация: не затрагивался; PowerShell copy внутри собственного scratch.
- Действие: PowerShell `Copy-Item` копировал директорию в отсутствовавшую цель и развернул её дочерние элементы вместо `staged/Source`.
- Факт: следующий copy ссылался на отсутствующую папку и завершился PowerShell exit 1. Ничего в исходном архиве/проекте не изменено.
- Исправление: подготовлены отдельные `Source`/`Content` каталоги; исходные файлы копировались по элементам. Evidence находится в дневнике; отдельного движкового сбоя нет.

## Неподдержанные glyphs в тестовой подписи

- Категория: `documented-limit`, не дефект редактора.
- Runtime/конфигурация: HUD `.ui`, UTF-8, Engine 2026.11.05.7, standalone 1000×500.
- Действие: `ui.edit-text` изменил кнопку на `Дом • 100 ₽`, затем проект запущен без пересборки.
- Ожидание: подпись кнопки сохранится без изменений; остальной макет останется прежним.
- Факт: обычная кириллица и цифры показаны; `•` и `₽` отсутствуют в шрифтовом атласе. Pixel A/B diff ровно 758 пикселей внутри подписи x=27..124/y=114..130; панель, метки и другие кнопки совпадают побитово.
- Evidence: `evidence/hud-before.xml`, `evidence/hud-after.xml`, `evidence/hud-ab.pixel-diff.json`, `evidence/hud-ab-before.png`, `evidence/hud-ab-after.png`. Диапазон шрифта описан в `Engine/docs/UI.md`.
- Ограничение проверки: native GUI Inspector и live Hot Reload не использовались; `ui.edit-text` и повторный запуск проверены.

## Trace filter probe — ошибочная точка луча

- Категория: `probe-setup-error`, исправлено.
- Runtime/конфигурация: Engine 2026.11.05.7, ground fixture плюс starter Cube.
- Действие: первый project C++ луч шёл вниз по центру сцены.
- Ожидание: query попадёт в Ground и покажет effect каналов/ignore.
- Факт: луч первым пересёк starter Cube; все запросы вернули Hit, поэтому тест не изолировал Ground. Это не дефект Trace.
- Исправление: повторён лучом из `(5,10,5)` мимо Cube. Получено `Hit / Miss / Hit / Miss`, debug line count `5 → 0`. Log: `evidence/trace-filter-debug2.log`.

## SaveGame и Skeletal material slots — узкий .8 retest требуется

- Категория: `external-engine-defect-pending-retest`, BUG-R3-001 подтверждён координатором в Runtime .7; этот проектный прогон не был независимым воспроизведением.
- Runtime/конфигурация: моя frozen .7 игра сохраняет сцену с `projectData.materialSlots` и читает её во втором процессе; JSON содержит ссылки ID 0/1. В проекте нет post-load pixel assertion для этих слотов.
- Факт: собственный тест доказывает сериализацию полей и восстановление экономики, но сам факт JSON не доказывает, что SkeletalMeshComponent применил слоты к GPU-модели после восстановления. Матрица `material-slot-persist` оставлена `partial`.
- Следующий шаг: после frozen Runtime .8 проверить этот же Saved slot scene/second process и pixel ROI; не пересобирать против изменяемого общего SDK.
