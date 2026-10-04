# Игры и запуск

Новый **Runtime Example** поддерживает кнопки «Играть» и «Редактировать»: обе запускают EngineRuntime.exe с разными DLL. Это консольный каркас (Enter — кадр, q — выход), без графики и панелей редактора. [Инструкция](../Engine/docs/RUNTIME-QUICKSTART.md). Скрипт сборки выбирает установленный v145 или v143; Visual Studio 2022 также поддержан.

Дважды щёлкните [Start.cmd](Start.cmd): после успешной сборки откроется лаунчер. Нужны Visual Studio с C++ v145 или v143 и Windows SDK 10. Скрипт не устанавливает инструменты.

- BasicExamples — выбор сцен «Квадраты» / «Куб».
- Pong — ракетки и мяч.
- SunGame — солнечная система.
- Katamari — требует vcpkg/Assimp; после установки и настройки VCPKG_ROOT используйте [Start-With-Assimp.cmd](Start-With-Assimp.cmd).

Лаунчер находится в `Launcher`, общий код примеров — в `Common`, проверки — в `tests`. В каждой игре свои Source, Content и проект. Старые лабораторные сохранены в `Archive/1_Lab` для сравнения и не участвуют в сборке.

Результаты: `build/x64/Debug/EngineLauncher.exe` и `build/x64/Debug/<Name>/<Name>.exe`; аналогично для Release. Ресурсы лежат рядом с каждым игровым exe. Для переноса сборки копируйте весь каталог соответствующей конфигурации. Лаунчер запускает отдельные процессы игр.

Сборка и архитектура — [документация Engine](../Engine/docs/FOLDER-ARCHITECTURE.md). Из корня репозитория: `./Engine/build.ps1`, затем `./Game/tests/run-games.ps1`.
