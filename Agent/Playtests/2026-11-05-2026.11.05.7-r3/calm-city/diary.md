# Дневник — роль 1, «Спокойный город», r3 / Runtime .7

## Повтор на .8 после BUG-R3-001 (координатор)

После завершения этой роли координатор штатно обновил копию через final8/EngineLauncher
`--upgrade-project` в calm-city/Final8Calm и выполнил `--build-project` с SDK .8. Затем
CALM_SAVE_TEST=save/load в отдельных Runtime процессах, по 180 кадров, exit 0.
SaveGameSelfTest.log: сохранено x=5.6, до Load x=3.1, после Load x=5.6 и продолжение x=5.35.
GPU-кадр нового процесса показывает квартал, машину, импортированный объект и LOD HUD.
Материалы мелкой модели не получили нового изолированного pixel assertion: прежний partial
для полного material-slot-persist не повышается только по факту успешного запуска.
Логи, UpgradeReport, CodeBuild и PNG: [evidence/final8](evidence/final8/).
Исходный .7 проект и его архив не заменялись; это прицельная регрессия, не новый полный круг.

## Подготовка
Прочитал skill try-engine, AGENTS, engine-developer, предыдущий CalmCity и REBUILD. Проверил состояние: перед READY был SDK .6; роль начал после сообщения о замороженном Runtime .7. Исторический CalmCity не менял. В `Engine/build/playtests/20261105-r3/calm-city/source-stage` распаковал исходный архив r2. Штатно создал новый CLI проект, включил C++ и только затем перенёс Source/Content; свежие Project.json, lock, Build.ps1 и метаданные сохранил. Store, TEMP/TMP и сборки были внутри своего каталога.

## Игровой шаг и действия
- Сохранил исходный городской квартал и маршрут процедурной машины.
- Сборка нового проекта через `--enable-code` прошла; после переноса исходников повторный `--build-project` против frozen .7 также прошёл (exit 0).
- Через .7 `asset.import` импортировал предоставленный `blend-suzanne-multiple.blend`: два mesh, два материала. Это маленький пробный импортируемый городской арт-объект; полноценные импортированные здания/машины не были созданы.
- Через `project.scene` разместил модель, переместил в (0,2,1), заменил материал первого слота, выполнил Undo/Redo, затем сохранил сцену, создал новую и загрузил сохранённую. Все шаги вернули status=0, загрузка восстановила оба разных слота.
- Запустил отдельный скрытый D3D11 runtime процесс и записал реальный кадр 1000×500. На нём различимы город, две части модели с разными цветами и городские тени. В кадре есть LOD panel.
- SaveGame запущен в одном процессе, сохранён слот с машиной, сценой и materialSlots. Второй отдельный runtime процесс загрузил слот; журнал подтвердил x=5.6 и продолжение маршрута до x=5.35. BMP нового процесса приложен.

## Наблюдения и ограничения
CLI-путь и перенос исходников ясны, но штатный `--enable-code` — это сразу настоящая сборка, поэтому первый запуск требует настроенного MSVC. Импортёр быстро подготовил слоты и /Game пути; назначение слота через editor host queue работало и пережило undo/redo и перезапуск сцены. Сцена/SaveGame сохраняют карту materialSlots.

Материалы визуально видны в game кадре. После первой порции дополнительно сравнил imported submesh с `castShadows=true/false` на одинаковых сценах; вне overlay изменились 35 пикселей в области под моделью, поэтому `imported-shadows` подтверждён. Крупный UV кадр снят, однако chart плохо читается и UV correctness оставлен partial. Перед A/B кадрами одна тестовая копия материала была расположена относительно другого HLSL корня и показала fallback; после перемещения материала рядом с Imported.hlsl исправленный A/B прошёл, ошибочный кадр в evidence не засчитывал.

Проверил CompletedFrameStats через лог первых 8 PreUpdate callbacks, восстановив исходный capture gate. Начало ожидаемое: PreUpdate=0 valid=0/frame=0; PreUpdate=1 valid=1/frame=1; далее valid=1/frame=2..7. Исходный gate сработал и создал BMP, поэтому это не дефект. В финальном Source удалил временную telemetry, восстановил условие valid и повторно собрал проект.

## Дополнительная finite-порция

После первичной сборки выполнил собственный `project.scene`: create/delete, group select/move, undo/redo, parent, duplicate/undo, Save/New/Load, delete/undo — 16 операций, все status=0. Затем вызвал все шесть `mesh.primitive`, `gizmo.snap`, `game.settings`; результат -1.5 snap и точные возвращённые render settings. Собственный `project.input` проверил Play/Pause/Resume/Stop на движущемся CarMover: движение остановилось на Pause и продолжилось после Resume. Собственный D3D `render.diagnostics` прошёл Forward, Deferred и Custom на разных размерах; разные viewport успешно выдали кадры, но resize в одном UI процессе не проверен.

Собственный `material.graph` запуск на копии WarmStone прошёл с настоящим Qt preview window: enable/save/preset/apply, ожиданный write failure при удерживаемом sidecar, повторный revert, invalid AI input, undo/redo и baseline revert. Все свои artifacts находятся в `evidence/`. Итог `evidence.json`: 34 passed, 9 partial, 13 not-tested из 56 назначенных строк роли 1. Пункты hot reload, compatibility/ABI, asset reimport, recovery, sky, particles и ещё несколько UI/материальных деталей не запускал; не подменял их coordinator или прошлым прогоном.
