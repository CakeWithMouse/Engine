# Готовый комплект PainEngine 0.4.0 r3

## Установка и первый запуск

1. Полностью распакуйте PainEngine-0.4.0-r3-win64.zip в отдельную папку.
   Сохраняйте структуру Runtime, Engine, Tutorials и папок игр.
2. Запустите EngineLauncher.exe из корня. Для готовых примеров компилятор
   не нужен; требуется Windows x64 с поддержкой Direct3D11.
3. Выберите пример и запустите игру или редактор. Documentation.html
   открывает офлайн-справку, Engine/docs/release-0.4.0-r3.html — страницу выпуска.
4. Если отсутствуют библиотеки VC++, установите Redist/vc_redist.x64.exe
   из комплекта. Лицензии находятся в Licenses.

Не запускайте EXE прямо внутри ZIP. Старый комплект и проекты сохраняйте
отдельно до проверки новой копии. Blender не входит в поставку; для .blend
он устанавливается отдельно по руководству импорта.

## Новый проект и C++

Создайте проект в лаунчере в новой папке. Project.json — его точка входа.
Откройте редактор, измените сцену и сохраните Ctrl+S. Play создаёт игровую
копию; после Stop возвращается Edit. Правки Play не заменяют сохранённую сцену.

Для C++ нужны MSVC x64 tools, Windows SDK, CMake3.24+ и Ninja в PATH.
Поставляемый Runtime/SDK использует CMake; номер SDK — 0.4.0.
Enable/Build выполняются через инструменты проекта или SDK/ProjectTools.ps1.
После неудачной компиляции рабочая DLL сохраняется. Подробности —
[CMake и SDK](CMAKE-BUILD.md), [работа с кодом](code.html).

При переходе с r2 бинарный ABI не менялся. Для более раннего 0.4.0
пересоберите C++-проект этим SDK и прочитайте [совместимость](RELEASE-0.4.0-r3.md).

## Учебные проекты

AiTutorial и RepairCorridor находятся в Tutorials. OpenEditor.cmd рядом
с Project.json запускает редактор поставляемого Runtime. Стратегия уже содержит
готовую Game.dll; исходный архив находится в Sources. Команды и доказательства
игрового прогона — Agent/Playtests/2026-11-18-0.4.0-strategy-r2/REBUILD.md.
Абсолютные пути в этом отчёте относятся к checkout автора: при повторе замените
их своими папками. Исходный ZIP стратегии распаковывается прямо в папку проекта.

Пример пересборки отдельной копии стратегии из PowerShell, открытого в корне
распакованного комплекта:

```powershell
$ErrorActionPreference = 'Stop'
$Runtime = Join-Path (Get-Location) 'Runtime'
$Archive = Join-Path (Get-Location) 'Sources/RepairCorridor-0.4.0-r2-source.zip'
$ProjectRoot = Join-Path $env:LOCALAPPDATA 'PainEngine/Projects/RepairCorridor-r3-copy'
if (Test-Path -LiteralPath $ProjectRoot) { throw 'Выберите новую папку проекта' }
Expand-Archive -LiteralPath $Archive -DestinationPath $ProjectRoot
& (Join-Path $ProjectRoot 'Build.ps1') -Runtime $Runtime
& (Join-Path $Runtime 'EngineRuntime.exe') --project (Join-Path $ProjectRoot 'Project.json') --mode game --frames 60
if ($LASTEXITCODE -ne 0) { throw 'Запуск проекта не прошёл' }
```

Для экспорта используйте «Сцена → Собрать EXE…» в редакторе. Пакет содержит
GameExecutable.exe, необходимый для экспорта; сырая CMake build-папка его
сама не создаёт. Проверьте Game.exe из отдельной папки экспорта.

## Физика Mesh

Static + Mesh подходит для арок и неподвижных объектов с отверстиями.
Dynamic/Kinematic + Mesh создаёт одну выпуклую оболочку. Она заполняет
впадины визуального меша. Настройка сохраняется в Scene JSON и инспекторе.
[Формы, примеры и ограничения](PHYSICS.md).

## Проверка и обратная связь

SHA256 ZIP находится в соседнем .zip.sha256. В PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath './PainEngine-0.4.0-r3-win64.zip'
```

Сверьте результат со строкой в .sha256. Внутри пакета SHA256SUMS.txt содержит
хеши отдельных файлов, RELEASE-MANIFEST.json — состав и версии, VALIDATION.json
— проверки готовой папки. Соседний .zip.validation.json фиксирует также
CRC и запуски из новой распаковки. При ошибке сохраните вывод запуска,
номер версии, Project.json и минимальные шаги воспроизведения.
