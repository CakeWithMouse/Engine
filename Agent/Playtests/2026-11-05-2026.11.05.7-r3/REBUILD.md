# Продолжить проекты r3

Итоговый локальный runtime: `Engine/build/playtests/20261105-r3/final8/Runtime`, версия .8.
Рядом находится EngineLauncher.exe. Исходный .7 runtime сохранён отдельно в корне r3.
Основная рабочая поставка — `Game/build/x64/Release/Runtime` (.8).

Исходные архивы (проверены: без SDK, DLL, LIB, PDB и Intermediate):

- [Спокойный город](calm-city/CalmCity-r3-source.zip) — .7 исходники, пересобраны и проверены с .8.
- [Градосимулятор, итоговая .8](curious-city/CuriousCity-final8-source.zip), [исходная .7](curious-city/CuriousCity-source.zip).
- [Бомбардировка](tired-bomber/tired-bomber-source-r3.zip).
- [Броски хлама](ai-trash/source.zip).

Архив — исходники и контент, не готовая двоичная поставка. Project.json из архива может
ссылаться на DLL исторической сборки; до пересборки не используйте его для smoke/upgrade.

1. Создайте новый проект штатным `--create-project`, затем `--enable-code` через launcher
   .8. Дождитесь завершения обоих GUI-процессов, проверьте exit codes и CodeBuild.log.
   PowerShell-пример корректного ожидания — [LAUNCHER-CLI](../../../Engine/docs/LAUNCHER-CLI.md).
2. Перенесите Source/Content нужного архива в новый проект. Сохраните свежие Engine.lock.json,
   Build.ps1, vcxproj/метаданные; startupScene возьмите из соответствующего игрового проекта.
3. Выполните `--build-project <Project.json>` этим же launcher. Убедитесь в `Build validated
   and activated`, а не только в наличии старой DLL.
4. Запустите `Runtime/EngineRuntime.exe --project <Project.json> --mode game --frames <N>`
   с окружением проверки из дневника роли. Все тесты должны использовать собственные EngineStore,
   TEMP/TMP и SaveData. Указывайте существующие абсолютные Windows пути для TEMP/TMP.

Подробные действия и настройки камер: [город](calm-city/REBUILD.md),
[градосимулятор](curious-city/diary.md), [бомбардировка](tired-bomber/diary.md),
[броски](ai-trash/REBUILD.md).

Для точного повтора BUG-R3-001 используйте сохранение
[round-current-dot7-save.json](ai-trash/evidence/round-current-dot7-save.json)
в слоте отдельного проекта игры про броски, как описано в его REBUILD. На .7 загрузка
падает на карте материалов наследника; .8 восстанавливает полёт и попадание.
Не копируйте тестовый slot поверх пользовательского сохранения.

Нужны Windows, D3D11, Visual C++/Windows SDK для сборки C++.
Для повторного импорта исходного .blend нужен Blender; готовый импортированный GLB в Content
используется без Blender. Консольные и GPU-регрессии перечислены в [fixes/README.md](fixes/README.md).
