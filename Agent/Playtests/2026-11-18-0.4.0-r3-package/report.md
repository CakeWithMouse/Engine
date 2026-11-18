# PainEngine 0.4.0 r3 — документы и архив из main

ZIP: `Engine/build/releases/PainEngine-0.4.0-r3-win64.zip`,148294973 байта.
SHA256 `8a3938c707369f3795000f526b8ec2d94f484812ad4bad8edbaf86949a506fe2`.
Packaging revision: `07b1ec27e81b85019eeacf62f4d44d2df31ae050`.

Основа: main `62b4eb49`, после fast-forward release/v0.4.0.
Код движка совпадает с r2. Новый комплект содержит обновлённые release notes,
инструкцию к ZIP, HTML-страницу выпуска и обновлённые оглавления.
Runtime/Editor/CMake SDK, игры и готовая стратегия берутся из проверенной r2
сборки; числовая версия остаётся 0.4.0. Сборка движка повторно не заявляется.

## Документация

[Страница выпуска](../../../Engine/docs/release-0.4.0-r3.html),
[release notes](../../../Engine/docs/RELEASE-0.4.0-r3.md),
[инструкция](../../../Engine/docs/DISTRIBUTION-QUICKSTART.md).
Генераторы проверены: 8 HTML-руководств, API41 страниц/327 записей,
120 заголовков; source snapshot `ae651b2508fb`.

## Проверки

Регрессия неизменённого движка унаследована от r2: Engine86/86 на двух
сборках, AI58/58, CTest3/3, внешний SDK2/2, Core/Host/Ui и native UI/API.
Это исторические результаты, они не засчитываются как новый запуск тестов.
Все 182 PE-файла и 366 библиотек/заголовков SDK совпали по SHA256 с r2:
[binary-reuse.json](binary-reuse.json).

Новый полный прогон готовой папки: **24/24 passed**. Проверены 16 HTML и
145 локальных ссылок, 182 PE x64; отсутствующих ссылок/DLL нет.
[VALIDATION.json](VALIDATION.json), [files-validation.json](files-validation.json),
[журнал](validation.log).

Инструкция пересборки стратегии повторена на новой распаковке в Unicode-пути:
CMake Build, validated/activated DLL, GameUpdates=60, EngineStopped, exit0.
[Команды](verify-quickstart.ps1), [лог](quickstart-final.log),
[результат](quickstart-validation.json).
Первый вариант инструкции ошибочно проверял LASTEXITCODE после PowerShell
Build.ps1: нативный код вспомогательной команды оставался ненулевым при
успешной сборке. Пример исправлен: ошибки скрипта завершают выполнение через
ErrorActionPreference=Stop, exit code проверяется у запуска EXE. Первый лог
[сохранён](quickstart-initial.log); движок не менялся.

Проверка выполняется `Engine/BuildSupport/Test-ReleasePackage.ps1` с отдельным
EngineStore и PATH без DLL рабочего Runtime. Полный прогон проверяет игры,
редактор, оба Tutorials, новый Unicode-проект, C++ Enable, экспорт и Game.exe.
Дополнительно инструкция пересборки RepairCorridor повторяется из нового
исходного ZIP с поставляемым Runtime/SDK. Проверка после ZIP —
`Test-ReleaseArchive.py`, затем `Test-ReleasePackage.ps1 -SmokeOnly`.

Ручной Inspector и collision импортированной сцены после reopen остались
partial в игровом r2 отчёте; новым r3 результатом они не объявляются.
Другие ОС и мониторы не проверялись. GitHub Release не создаётся.

ZIP проверен по SHA256 и CRC; после новой распаковки совпали все1601 SHA256:
[extraction-integrity.json](extraction-integrity.json), [archive.log](archive.log).
Результаты запусков из распаковки сохраняются отдельно от полной проверки папки.
Все19/19 прошли: [extracted-VALIDATION.json](extracted-VALIDATION.json),
[журнал](extracted-validation.log). Рядом с ZIP сохранены .zip.sha256 и
.zip.validation.json: целостность, оба прогона, quickstart и сравнение бинарников.
Документы и аудит фиксируются в main; push выполняется после фиксации отчёта.
