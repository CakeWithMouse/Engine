# Игры и запуск

Дважды щёлкните [Start.cmd](Start.cmd): после успешной сборки откроется лаунчер. Нужны Visual Studio 2026, C++ v145 и Windows SDK 10. Скрипт не устанавливает инструменты.

- BasicExamples — выбор сцен «Квадраты» / «Куб».
- Pong — ракетки и мяч.
- SunGame — солнечная система.
- Katamari — требует vcpkg/Assimp; после установки и настройки VCPKG_ROOT используйте [Start-With-Assimp.cmd](Start-With-Assimp.cmd).

Лаунчер находится в `Launcher`, общий код примеров — в `Common`, проверки — в `tests`. В каждой игре свои Source, Content и проект. Старые лабораторные сохранены в `Archive/1_Lab` для сравнения и не участвуют в сборке.

Результаты: `build/x64/Debug/EngineLauncher.exe` и `build/x64/Debug/<Name>/<Name>.exe`; аналогично для Release. Ресурсы лежат рядом с каждым игровым exe. Для переноса сборки копируйте весь каталог соответствующей конфигурации. Лаунчер запускает отдельные процессы игр.

Сборка и архитектура — [документация Engine](../Engine/docs/FOLDER-ARCHITECTURE.md). Из корня репозитория: `./Engine/build.ps1`, затем `./Game/tests/run-games.ps1`.
