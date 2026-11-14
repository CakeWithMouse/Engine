# Дополнительные наблюдения

## ENV-CONT-001 — окружение нового test runner

Первый запуск AuthoringCommandletTests после добавления Enable C++ наследовал
PSModulePath от PowerShell 7 при запуске Windows PowerShell. ProjectTools не нашёл
Get-FileHash. Это ошибка окружения runner, не дефект Engine: производственный
CodeTools уже очищает PSModulePath. Runner приведён к тому же правилу.
Исходный лог: Engine/build/playtests/20261114-r6/continuation-authoring-02/enable-code.log.
Повтор: continuation-authoring-03.

## INPUT-CONT-001 — мастер классов до включения C++

Первый координаторский вызов project.class-wizard на no-code fixture дошёл до
штатного отказа ProjectTools «Enable C++ first». После включения C++ тот же
запрос создал WizardProbe.h/.cpp. Предусловие отражено в документации команды.
Это корректный отказ, не баг движка.

## INPUT-CONT-002 — изоляция копии проекта роли Bomber

При копировании обнаружены абсолютные source-пути ProjectTypes.tsv к исходному
проекту. Это пока наблюдение метаданных, не доказанная ошибка изоляции:
ProjectTools берёт исходники из собственного Source и пересоздаёт ProjectTypes.tsv
после сборки. Автор проверил реальные пути компилятора: использована continuation
копия; старый CodeBuild.log оказался скопированным из исходного проекта.
Сценарий повторён в своей копии. Исходный проект и его файлы не изменяются.

## HARNESS-CONT-001 — модальное сообщение удерживало командлет

Новая ветка project.scene с editorWindow:true передавала ошибки настоящему
окну, но не закрывала QMessageBox при последующей обработке Qt events.
Первый отрицательный сценарий curious-city с `/transform/position` оставил
PID 13124. Его идентичность проверена elevated CIM и сохранена в
evidence/stale-scene-process.json; завершён только этот собственный тест.
Sandbox Get-Process ошибочно не показывал elevated процесс.

Добавлен timer закрытия видимых QMessageBox с сохранением dialogs. После:
неверное свойство → Save → startup возвращает error/status для первого шага
и успех следующих, без зависания. AuthoringCommandletTests: 9 сценариев PASS.
Это дефект нового тестового маршрута продолжения, не прежнего редактора:
интерактивный редактор правильно ожидал нажатие пользователя.

## HARNESS-CONT-002 — Сужение числовых полей до проверки диапазона

Ревью tired-bomber обнаружило int(Number(...)) в новых Particle/Mesh diagnostics
и прежнем PointShadowDiagnostics. Факт до: mesh.simplify с slices=24.5 принимался
и молча усекал значение. Конечный float 1e30 также мог попасть в int до проверки.
Общий Integer теперь проверяет JSON number, конечность double, целочисленность
и границы до cast. Regression: 13 дробных/огромных inputs, включая frames.steps,
должны вернуть exit 2 и именно integer-field error. Все прошли; положительные
MeshDiagnostics 8/16 кадров и ParticleCommandlets 8/32 кадра повторены успешно.

## ENV-CONT-002 — Автономный runner EngineTests

Отдельная сборка EngineTests не копировала Assimp DLL chain рядом с exe;
запуск падал с -1073741515. Runner теперь временно добавляет pinned vcpkg bin
в PATH и восстанавливает прежнее окружение в finally. На машине установлен v143,
а default runner был v145; теперь default auto использует штатное обнаружение
Invoke-MSBuild. Никакие глобальные переменные/системные пакеты не менялись.
Финальный run.ps1 -Configuration Release -NoBuild: 72/72, 0 skipped.

## INPUT-CONT-003 — Изоляция регрессии графа

Первый root запуск MaterialGraphCommandletTests указывал отсутствующий WarmStone
в curious-city; второй взял сцену с пользовательским CalmCity.Building, который
стандартный commandlet не регистрирует. Это неверные входные фикстуры, не engine
defects. Повтор на свежем собственном Authoring project с копией WarmStone
прошёл все операции, границы 121/122 и историю. Исходные проекты не изменены.
