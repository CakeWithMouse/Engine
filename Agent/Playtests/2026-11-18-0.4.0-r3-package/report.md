# PainEngine 0.4.0 r3 — документы и архив из main

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
Состав Runtime/SDK и бинарники новой поставки сверяются с SHA256 r2.
Результаты новых проверок папки и распаковки добавляются после их выполнения.

Проверка выполняется `Engine/BuildSupport/Test-ReleasePackage.ps1` с отдельным
EngineStore и PATH без DLL рабочего Runtime. Полный прогон проверяет игры,
редактор, оба Tutorials, новый Unicode-проект, C++ Enable, экспорт и Game.exe.
Дополнительно инструкция пересборки RepairCorridor повторяется из нового
исходного ZIP с поставляемым Runtime/SDK. Проверка после ZIP —
`Test-ReleaseArchive.py`, затем `Test-ReleasePackage.ps1 -SmokeOnly`.

Ручной Inspector и collision импортированной сцены после reopen остались
partial в игровом r2 отчёте; новым r3 результатом они не объявляются.
Другие ОС и мониторы не проверялись. GitHub Release не создаётся.
