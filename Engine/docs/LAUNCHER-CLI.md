# EngineLauncher: команды и диагностика

Запуск без аргументов открывает обычный launcher. `--help` (также `-h`, `/?`) выводит справку. Неизвестная команда и неправильное число аргументов выводят пояснение в stderr. Обычные CLI-ошибки не открывают MessageBox; специальные команды `*-prompt` намеренно показывают диалог обновления.

## Создать C++-проект

Нужны собранный launcher с каталогом Runtime, шаблон SceneProject, Visual Studio C++ tools, Windows SDK и SDK закреплённой версии движка. Компилятор не нужен для запуска уже собранной игры. Родительский каталог должен существовать, каталог самого проекта — **не существовать**, даже пустой. Существующие файлы не перезаписываются.

Launcher — Windows GUI executable. В PowerShell явно ожидайте завершения; один `& EngineLauncher.exe ...` не является надёжным способом дождаться GUI-процесса и прочитать его exit code. Пример для PowerShell 7 из любой папки (замените корень репозитория/выпуска):

```powershell
$launcher = 'E:/Engine/Game/build/x64/Release/EngineLauncher.exe'
$projectDir = 'E:/Games/Мой город'
# E:/Games уже существует; $projectDir ещё не существует.
function Invoke-EngineLauncher([string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new($launcher)
    $start.UseShellExecute = $false
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    $code = $process.ExitCode
    $process.Dispose()
    if ($code -ne 0) { throw "EngineLauncher завершился с кодом $code" }
}
Invoke-EngineLauncher @('--help')
Invoke-EngineLauncher @('--create-project', $projectDir, 'Мой город')
Invoke-EngineLauncher @('--enable-code', "$projectDir/Project.json")
Invoke-EngineLauncher @('--build-project', "$projectDir/Project.json")
Invoke-EngineLauncher @('--smoke-project', "$projectDir/Project.json")
```

`--enable-code` создаёт исходники и выполняет первую сборку. После изменения Source используйте `--build-project`, проектный Build.ps1 или Hot Reload редактора. Команды `--create-code-project` нет: сообщение об ошибке подсказывает два штатных шага. Флажок создания C++ в GUI остаётся доступным.

Windows PowerShell 5.1 не имеет ProcessStartInfo.ArgumentList: используйте `Start-Process -Wait -PassThru` и явно заключайте аргументы с пробелами в кавычки. Например, захват справки:

```powershell
$p = Start-Process -FilePath $launcher -ArgumentList '--help' -Wait -PassThru -NoNewWindow `
    -RedirectStandardOutput './launcher-help.txt' -RedirectStandardError './launcher-error.txt'
$p.ExitCode
Get-Content './launcher-help.txt' -Encoding UTF8
```

## Команды

| Команда | Действие |
|---|---|
| `--create-project <NEW-folder> <name>` | Создать проект из SceneProject |
| `--enable-code <Project.json>` / `--build-project <Project.json>` | Включить C++ / собрать и активировать проверенную DLL |
| `--check-project <Project.json>` | Проверить manifest, модуль и стартовую сцену |
| `--smoke-project <Project.json>` / `--smoke-project-editor <Project.json>` | Запустить игру/редактор на 3 кадра с закреплённым runtime |
| `--smoke-project-current <Project.json>` / `--smoke-project-editor-current <Project.json>` | Проверка совместимого запуска на Runtime рядом с этим launcher |
| `--smoke-project-prompt <Project.json>` / `--smoke-project-editor-prompt <Project.json>` | Проверка с диалогом обновления; не для безусловной автоматизации |
| `--install-engine <Runtime-folder>` | Установить выпуск в EngineStore |
| `--upgrade-project <Project.json> <NEW-copy-folder>` | Обновить новую копию; исходник сохраняется |
| `--check <example>` / `--check-editor <example>` | Проверить встроенный пример по имени из launcher |
| `--smoke <example>` / `--smoke-editor <example>` | Проверочный запуск встроенного примера |

## Ошибки и потоки

| Exit | Значение | Где искать пояснение |
|---|---|---|
| 0 | Успешная операция | Справка в stdout; операции могут не печатать подтверждение |
| 1 | Операция отклонена: существующий каталог, отсутствующий проект/SDK, ошибка сборки и т. п. | stderr; при ошибке C++ указан полный путь CodeBuild.log |
| 2 | Неизвестная команда/пример или неверное число аргументов | stderr с подсказкой |

Smoke дополнительно может возвращать ненулевой код завершения дочернего Runtime. Ненулевой код нельзя считать успехом даже при наличии старой DLL. Перенаправленный вывод launcher — UTF-8 без BOM; консоль получает Unicode. Текст полного журнала C++ находится в `<папка проекта>/CodeBuild.log`; предыдущая рабочая DLL сохраняется при отказе кандидата.

Пример конфликта: `Choose a NEW project folder; this path already exists: E:\Games\Мой город`. Выберите другой новый путь. Не удаляйте существующий проект ради повторения команды.

## Проверка разработчиком движка

```powershell
# Из корня репозитория; сборка в штатном MSVC-окружении:
& Engine/build.ps1 -Configuration Release -Target EngineLauncher
& Game/Launcher/Tests/CliTests.ps1
```

Runner требует PowerShell 7, создаёт отдельный проект и EngineStore, строго читает UTF-8, проверяет stdout/stderr, коды завершения, сохранность существующих файлов, Create → Enable → Build → Smoke. `-SkipCpp` оставляет быстрые проверки, `-Observe` сохраняет baseline без утверждения об успехе. `-Output` должен указывать новый каталог.
