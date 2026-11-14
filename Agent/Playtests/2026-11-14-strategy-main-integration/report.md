# Интеграция стратегии r1–r3 в main

По запросу пользователя все накопленные изменения стратегии подготовлены к
отправке в main. До интеграции локальная база — `388d7e9e`; получен и применён
fast-forward до `91c0c5e4` (PR 27: геометрия заставки, фон и иконка консоли).
Конфликтов нет, новые изменения upstream сохранены.

Включены три этапа Repair Corridor, исходные архивы, сценарии, отзывы и
доказательства повторных проверок тем же агентом. Исправления движка:
плотность физических тел, двойная регистрация участника группы как цели,
завершение недостижимого приказа, ранняя остановка малых юнитов и повторное
использование поля потока для несовместимой цели. Последний этап игры добавляет
выбор группы и обход препятствий. Исторические неудачные проверки сохранены
и не заменены успешными результатами последующих исправлений.

Проверки объединённого дерева (Release/v143, локальный SDK):

- Engine: 83/83, 0 skipped, 0 failed; включая регрессию плотности.
- AI: 56/56, включая новые регрессии групповой навигации.
- Editor, Runtime и Launcher: сборка успешна; SDK whole-archive link passed.
- Launcher UI: 23/23; изображение актуальной заставки — `splash.png`.
- Engine.lib SHA256:
  `18622FC582C05F0DEC25753C4297C3276C44C736D764FBFDA460D2E56F35B1B3`.

Команды проверок:

```powershell
./Engine/tests/run.ps1 -Configuration Release -PlatformToolset v143 -EnableAssimp
$env:PATH = 'E:\Engine\Engine\vcpkg_installed\x64-windows\bin;' + $env:PATH
./Engine/build/x64/Release/AiTests.exe
./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Editor/EditorModule.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk
./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Game/Launcher/EngineLauncher.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk
./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Game/Launcher/Tests/LauncherUiTests.vcxproj -Configuration Release -PlatformToolset v143 -LocalSdk
./Engine/build/x64/Release/LauncherUiTests.exe E:/Engine/Engine/build/playtests/strategy-main-integration/splash.png
```

Логи расположены рядом с отчётом. Исходные архивы и хеши трёх этапов сохранены
без изменения; `.gitattributes` сохраняет точные байты доказательств.
В git включены исходники игр в архивах отчётов, а не локальные build-каталоги.
Публикация нового релиза движка не выполняется.
