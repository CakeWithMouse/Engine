# 059 — CLI-диагностика создания C++-проекта

Статус: реализовано и проверено 2026-11-04 в Engine/fixses. Выполнен [промпт](../Prompts/launcher-cli-diagnostics.md).

Добавлены справка --help/-h/?, описание Create → Enable → Build, проверка имени команды и аргументов, UTF-8 stdout/stderr и понятный конфликт существующего каталога с абсолютным путём. GUI executable подключается к существующей консоли, сохраняя перенаправленные handles. Ошибка сборки указывает CodeBuild.log. Обычный GUI-путь сохранён; prompt-smoke команды намеренно остаются интерактивными.

Проверено: 16/16 CLI-интеграций (до изменения 1/12 быстрых), 8/8 C++ build/hot-reload workflow, Release build, API Reference --check. [Отчёт](../Playtests/2026-11-04-launcher-cli/report.md), [руководство](../../Engine/docs/LAUNCHER-CLI.md).

SaveGame в эту реализацию не входит: [отдельный проект](../../Engine/docs/SAVEGAME-DESIGN.md), [HTML-схема](../../Engine/docs/SAVEGAME-DESIGN.html). Неполные проверки удержания ввода/recovery Curious City остаются отдельными задачами и не объявлены выполненными.
