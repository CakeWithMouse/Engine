# Фикс движка после разработчиков — результат

2026-11-04, рабочая копия PainEngine 2026.11.01.5, Release x64 / MSVC v143 / Qt 6.7 / D3D11. Оба новых навыка созданы, установлены и применены. Исторический журнал issues.md сохранён; актуальные статусы ниже.

## Итог

**Все восемь заведённых дефектов исправлены и получили регрессии.** Создан EngineCommandlets с 14 параметризованными командами над настоящими API и виджетами, отдельные сценарии жалоб и [документация запуска](../../../../Engine/docs/COMMANDLETS.md). Исходник: Editor/Tools/EngineCommandlets.cpp. Это отдельная консольная программа; интерактивный редактор не требуется. UI использует offscreen Qt, игровые проверки — скрытый HWND и D3D11.

Проверено: **25/25 commandlet-сценариев**, **8/8 шагов свежего C++ workflow**, **6/6 игровых утверждений до/после reload**, ещё **5/5 повторов Save/уничтожения активного UIEditor**. Связанные наборы: Engine **53 passed / 1 skipped / 0 failed**, Core **374**, Host **69**, Input/Character **82**, Camera/D3D **101**; отдельный Subsystem lifecycle suite прошёл. Обычные Editor.dll, EngineRuntime и EngineLauncher собраны. git diff --check чистый.

## Дефекты и доказательства

| ID | Причина и исправление | Проверка |
|---|---|---|
| BUG-001 | Длинный диапазон чисел навязывал чрезмерную минимальную ширину осей, а горизонтальный скролл был запрещён. Поля могут сжиматься до разумного минимума, скролл доступен при необходимости | [До](baseline/BUG001.result.json), [после](after/BUG001.result.json), [реальная тема при 310 px](after/inspector.png). X/Y/Z и Reset доступны |
| BUG-002 | Пять аргументов передавались четырём уникальным placeholder. Дополнительно полный EditorTheme выявил неподдерживаемый QSS :not(#CardHeaderBtn). Добавлены обе ссылки стрелок; кнопки содержимого адресуются через CardContent | [До](baseline/BUG002.result.json), [после](after/BUG002.result.json), [render](after/style.png). Нет предупреждений arg/parse |
| BUG-003 | Save сериализовал модель до применения активного поля. Теперь сначала применяет Inspector; textEdited помечает pending changes. Усиленный прогон с темой выявил также падение при уничтожении окна с активным полем: teardown отключает сигналы детей до уничтожения root_/selected_ | [Потерянный текст до](baseline/BUG003.result.json), [сохранённая кириллица](after/BUG003.result.json), [отмена закрытия](after/PendingClose.result.json), [5 повторов](after/ui-teardown-repeats.json). Старый themed-прогон фиксирует отрицательный exit [здесь](themed-baseline/results.json) |
| BUG-004 | Предыдущее исправление исключило рекурсивное setEnabled(false) на главном окне во время Build; редактирование ограничивается sceneBusy_ и штатными кнопками | [Свежий workflow](cpp-workflow/results.json): реальная неудачная сборка возвращается, окно включено, старая DLL сохранена; успешная активирует новую без закрытия текущего окна |
| BUG-005 | Предыдущее исправление убирает PSModulePath только из дочернего окружения Windows PowerShell в Qt и Win32 launcher | Тот же workflow сначала создаёт проект и вызывает launcher --enable-code при PS7 PSModulePath, затем компилирует через production QAction редактора. [Launcher log](cpp-workflow/launcher-enable.log) |
| BUG-006 | Консольные байты MSBuild/PowerShell интерпретировались неоднозначно. MSBuild пишет явный UTF-8 file log, PowerShell отдаёт UTF-8, Qt декодирует UTF-8 с буферизацией неполных строк | [Русский #error в Console](cpp-workflow/failed.result.json), без U+FFFD, компилятор C1189. Файл сборки остаётся в проекте |
| BUG-007 | Новая игровая сессия не получала сохранённый фокус; сфокусированный QWidget не обязан ещё раз послать focusIn. QtHost запоминает InputFocus и передаёт состояние при замене API | [Контракт focus](after/BUG007.result.json), отдельный FocusOff; [реальная игра](reload-input/result.json): до и после замены DLL T создаёт первый предмет, HUD — второй, без reopen |
| BUG-008 | Center drag обходил квантование, применённое к осям/плоскостям. ViewportWidget теперь вызывает общую операцию center translation с world/local Snap | Историческое проявление в issues.md; [результат -1.5 вместо -1.6378729](after/BUG008.result.json), отдельные disabled/zero/local проверки |

Нативные ошибки окончания процесса не скрываются наличием старого JSON: runner удаляет предыдущий result перед вызовом и обязательно проверяет exit code. При обычном unhandled native exception commandlet пытается записать result.json.dmp; heap fast-fail может обходить этот обработчик.

## Функционал и тесты

Полная связь API/жалоба/команда/результат — [coverage.csv](coverage.csv). Поверхность включает текущие Camera properties, screen rays, Scene::Raycast, GameSession/GameSubsystem, MeshPrimitives, GameSettings, UI Save/theme/layout, focus rebinding, C++ Build и reload. Расширение следующего релиза описано в COMMANDLETS.md. Имена жалоб находятся в сценариях, игровые правила остаются в игровой DLL.

Для текущего EngineTests runner добавлена пересборка RuntimeFixtures: четыре старые VFS lifecycle ошибки оказались fixture DLL с устаревшим Module API. Input/Subsystem runner больше не требует Assimp без необходимости. Импортная часть AuthoringTests отдельно сообщает SKIP, предварительно проверяя доступные свойства/текстуры и ожидаемый отказ отсутствующего импортёра. Полный сценарий моделей сохраняется для -EnableAssimp.

Это не заявление о полном покрытии всех ветвей всех 73 групп из прошлого playtest: его исходные ограничения остаются в report.md. В этом прогоне закрыты восемь конкретных дефектов и добавлена поддерживаемая консольная поверхность текущих API; импорт моделей в конфигурации без Assimp не объявлен проверенным.

## Зафиксированные проблемы разработки обвязки

- Первая сборка commandlet: C2440 (сигнатура focus), C2065 (неверное имя EngineModuleHost), C2248 (private DrainCommands). Исправлено использованием текущих сигнатур и публичного Update. Затем LNK2001/LNK1120 для Scene/Mesh: добавлена ссылка на Engine.vcxproj. Первичные ошибки есть в переписке вызовов; финальный лог — logs/commandlet-build.log.
- Первый offscreen запуск не имел qoffscreen.dll. Сборка теперь явно разворачивает плагин. Незавершённый запуск остановлен. Для читаемого render загружается Segoe UI; предупреждение Qt offscreen об отсутствии собственного lib/fonts не является ошибкой QSS и не мешает загруженному шрифту.
- При разработке project.build commandlet удерживал QFile Project.json и мешал атомарной замене (Windows error 5). Read handle закрывается сразу после чтения. Это ошибка новой обвязки, а не дополнительный дефект пользовательского проекта.
- Ранний launcher-прогон не ожидал GUI-subsystem exe и выбрал установленный старый runtime. Исправленный CppWorkflow использует Start-Process/WaitForExit и отдельный EmptyEngineStore; его результаты выше заменяют раннюю пробу.
- Первая команда scene.property имела пустой каталог типов; исправлена на штатный SceneProject::GetComponentTypes, проверки валидного/невалидного FOV прошли.
- Один вызов сборки использовал неверный Launcher.vcxproj и получил MSB1009; [лог](logs/fix-launcher-wrong-path.log). Правильный EngineLauncher.vcxproj собран успешно.
- Полная тема расширила воспроизведение BUG-002/003: оставшийся parse warning и выходы 0xC0000005/0xC0000374 учтены и исправлены, а не исключены из теста.
- C1189 с кириллицей и ошибки HLSL в тестовых сломанных shader fixtures — преднамеренные негативные проверки, не незакрытые ошибки релиза.

## Артефакты и ограничения

Логи итоговых сборок/наборов находятся в logs/. Машинный итог — outcome.json. Отдельно составлен [анализ улучшений по четырём персонам](../improvement-analysis.md); wishlist этим навыком не реализуется.

Исправления находятся в исходниках и рабочем Game/build/x64/Release/Runtime. Пакет установленного релиза в пользовательском Engine Store не перепубликовывался; при проверках launcher явно направлялся на рабочую сборку. Версия публичного API не изменена. Недельный счётчик аккаунта проверялся; граница 30% не достигнута, кредиты сброса не использовались.
