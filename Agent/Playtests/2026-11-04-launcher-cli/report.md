# Результат: диагностика CLI и проект SaveGame

2026-11-04, ветка Engine/fixses. Выполнен [промпт](../../Prompts/launcher-cli-diagnostics.md). SaveGame не реализован; подготовлен отдельный [план](../../../Engine/docs/SAVEGAME-DESIGN.md) и [HTML-схема](../../../Engine/docs/SAVEGAME-DESIGN.html).

## Реализация CLI

- `EngineLauncher --help`, `-h`, `/?`: stdout, exit 0, команды и последовательность Create → Enable → Build → Smoke, требования SDK/MSVC и путь журнала.
- Неизвестная команда/неверное число аргументов: UTF-8 stderr, exit 2. Для --create-code-project подсказка указывает поддерживаемые --create-project и --enable-code.
- Ошибка операции: UTF-8 stderr, exit 1. Конфликт существующей папки содержит полный путь и требование NEW. Существующая папка, включая пустую, не используется как новый проект.
- Подключение GUI executable к существующей консоли сохраняет перенаправленные handles. Обычные CLI-ошибки не открывают MessageBox. GUI-запуск не переделывался; специально названные prompt-команды остаются интерактивными.
- Неуспешная C++ сборка сообщает полный путь CodeBuild.log, а manifest сохраняет предыдущую DLL.

## Проверки

| Проверка | Результат |
|---|---|
| До изменения, быстрые CLI-сценарии | 1/12; создание работало, ожидаемая диагностика отсутствовала |
| Финальный CLI suite | 16/16: help, alias, неизвестная команда, неизвестный C++ флаг, неверные аргументы, отсутствующий проект, существующие папки, Create/Enable, неуспешная и успешная Build, Smoke |
| Отдельный C++ workflow | 8/8: компилятор с кириллицей, сохранение старой DLL, успешный hot reload |
| Release launcher | Собран, ошибок сборки нет |
| API Reference | Сгенерирован; --check прошёл: 38 страниц, 294 API entries, 83 заголовка, 10 примеров |
| Поставка документации SDK | LAUNCHER-CLI.md включён в New-ProjectSdk; копия проверена по SHA256 |
| Обычная консоль | --help через Start-Process -NoNewWindow в PTY вне sandbox показал справку, exit 0 |
| HTML | Структура ID, синтаксис JS и DOM-модель: 9 блоков, 4 вкладки, Save/Load и отказ Load проверены |

Свидетельства: [before](evidence/before/results.json), [первый after](evidence/after/results.json), [финальный CLI](evidence/verified/results.json), [C++ workflow](evidence/cpp-workflow/results.json), [проверка HTML](evidence/html-check.log). Последний CLI набор расширен отрицательной компиляцией C1189: это намеренная ошибка fixture, не оставшийся дефект.

Браузерный инструмент запретил открывать локальный file:// по политике URL. Другие способы обхода не использовались. **Визуальный browser-preview HTML не проверен**; DOM-модель не подтверждает фактическую раскладку браузера. HTML — самостоятельный офлайн-файл без внешних ресурсов, пользователь может открыть его локально.

В sandbox прямой console attach не дал видимого текста; тот же запуск вне sandbox отобразил справку. Перенаправленный UTF-8 вывод проверен отдельно интеграционным runner. При уточнении подключения к консоли сохраняются перенаправленные file/pipe handles, а не произвольные значения неинициализированных standard handles.

Обновлены LAUNCHER-CLI, CODE-PROJECTS, USER-PROJECTS, RUNTIME-QUICKSTART, COMMANDLETS, source и сгенерированный API Reference, SDK packaging, task index и уточнение анализа Curious City. Инструкция подробно объясняет ожидание GUI-процесса из PowerShell.

## Границы

Текущая версия runtime/API не менялась: правка касается launcher и документации. Выполнена сборка в рабочем дереве, обновлена документация рабочего SDK; ранее установленный immutable EngineStore не перезаписывался. Экономика, properties, Raycast и спавн не менялись. Неполные пользовательские проверки Controller/recovery остаются отдельной задачей.

SaveGame документирован как новая возможность: DTO игры + codec, host coordinator/store, stable gameId/SaveId, очередь на границе кадра, атомарная запись, отдельная восстановленная сессия, optional C ABI и отсутствие произвольного rollback внешних побочных эффектов. Пример API явно помечен как проектный, не компилируемый с текущим SDK.
