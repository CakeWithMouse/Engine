# Проверка ветки release/v0.4.0 — 18.11.2026

Fetch обновил origin/main с `114b1b69` до `5458f022`. Локальный main обновлён
fast-forward; конфликтов и исходных незакоммиченных правок не было.
`release/v0.4.0` создана от `5458f022`. Engine/Version.json и обе свежие
поставки Runtime/SDK имеют номер `0.4.0`; ABI и формат сцены не повышены.

## Результат координатора

| Проверка | Результат | Доказательство |
|---|---|---|
| MSBuild Release runtime/editor/launcher/GameExecutable/SDK, v143 14.44 | PASS; whole-archive SDK link | runtime-build-retry.log |
| CMake Release, MSVC 19.40, Qt 6.7, Assimp, Jolt | PASS | cmake-build-test.log |
| CTest CMake: PortableCore, ComponentGeneratorPortable, Engine | 3/3 PASS | cmake-build-test.log |
| Установленный CMake SDK: внешний Core и генератор | 2/2 PASS | sdk-consumer.log |
| Новый C++-проект с Unicode-путём через Windows PowerShell 5.1 | PASS после REL040-001 | cmake-projects-fixed.log |
| CMake startup, no-op, AddClass, header dependencies, отказ активации ошибочной DLL | PASS | cmake-projects-fixed.log |
| EngineTests, реальный Jolt и Assimp | 83/83, 0 skipped | engine-tests.log |
| AiTests | 58/58 | ai-tests.log |
| Editor CoreTests | 1105 checks PASS | CoreTests.log |
| Editor HostTests | 165 checks PASS | HostTests.log |
| Полный UiTests | все suites PASS после REL040-002 | ui-after-fix.log |
| Native AI diagnostics UI | PASS | ai-diagnostics.log |
| Native camera guides + правка scene properties | PASS после обновления тестового селектора | camera-guides-fixed.log |
| AI diagnostics API: preview/live/pause/clear/resume/stop/invalid | PASS | diagnostics-api.log |
| Генерация офлайн-руководств | 7/7 `--check` PASS | Engine/docs/build-guides.mjs |
| HTML API Reference | `--check` PASS: 40 страниц, 317 записей, 120 заголовков | Engine/docs/api-reference/build.mjs |
| Локальные href/src офлайн-справки | 8 страниц, 120 ссылок, отсутствующих файлов нет | docs-links.json |

UiTests включает MaterialSlotList, WheelGuard, AssetPickerDialog,
AiDiagnosticsDock, ObjectTypeInspector, InspectorCardStyle, InfoButton,
CrossTypeInfoButton, HighDpiIcon, Separator, ContentBrowser, сохранение
point-light shadows и регрессию billboard. Это автоматические проверки,
а не ручной пользовательский проход всех этих функций на стратегии.

Исправлены два подтверждённых дефекта: смешанные версии compiler/STL в CMake
SDK и выбор категории игровой камеры. Также обновлён устаревший селектор
в CameraGuideUiTests. Причины, исходные неудачи и повтор — [issues.md](issues.md).
Основной MSBuild Editor.dll и CMake Editor/ProjectSdk обновлены после исправлений.
Хеши свежей MSBuild-поставки: [runtime-hashes.json](runtime-hashes.json).

## Проверка прежним автором стратегии

Возобновлён тот же `strategy_developer`, который создал Repair Corridor r1–r5,
через его исходный родительский чат. Новая общая матрица четырёх персон не запускалась.
Агенту переданы обе новые SDK, точные SHA256 и отдельные рабочие/отчётные каталоги.
Старые исходники и архивы сохраняются. Его фактический игровой прогон, матрица
новых функций и ограничения — [отдельный отчёт](../2026-11-18-0.4.0-strategy/report.md).

Автор подтвердил миграцию полной старой копии, новую сборку MSBuild,
Runtime60, экспорт и запуск `Game.exe` из Unicode-пути, CMake-сборку своей
игры с активацией DLL и импорт плюс 13 операций слотов материалов.
Воспроизведение ввода на настоящей игре дало прибытие колонны, открытый проход
и 0 нарушений барьера. Позднее поражение охраннику соответствует старому
сценарию без атаки; победа не заявляется. Свежий D3D-кадр просмотрен также
координатором. CUA не предоставил доступ к созданному Editor-окну, поэтому
непосредственные взаимодействия автора с новыми панелями отмечены partial.
Это ограничение среды отделено от прошедших native/API-тестов.

Итог автора: 36 строк матрицы, 17 свидетельств; личные статусы 3 passed,
28 partial, 5 not-tested (включая другие платформы и документационный backlog).
Это оценка полноты именно его практического опыта, а не число упавших тестов.
Новых подтверждённых дефектов движка автор не обнаружил. Source ZIP проверен
по CRC и составу, затем заново распакован, собран и запущен на 60 кадров.
SHA256 архива: `CF4474962B03DDB5C37ED4BD164441765E377886B546D7FA8CE20BF4A36BF6C0`.
Повторный fetch после общих проверок подтвердил прежний origin/main `5458f022`.

## Воспроизведение

Общий MSBuild runner: [verify-msbuild.ps1](verify-msbuild.ps1), запуск из
E:/Engine. Его абсолютные пути относятся к этому checkout; при переносе
замените repo/out/runtime. Исходные неуспешные runner-логи сохранены отдельно.

```powershell
./Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Game/Launcher/EngineLauncher.vcxproj -Configuration Release -PlatformToolset v143 -EnableAssimp -LocalSdk
./Engine/build-cmake.ps1 -Preset editor -Configuration Release -QtRoot E:/Engine/Editor/ThirdParty/Qt6.7 -EnableAssimp -EnableJolt -Test
./Engine/BuildSupport/tests/Test-CMakeProjects.ps1 -Runtime E:/Engine/build/cmake/editor-Release/bin/Release/Runtime
node Engine/docs/build-guides.mjs --check
node Engine/docs/api-reference/build.mjs --check
```

CMake/Ninja установлены только в `Engine/build/tools/python`, на этой машине
их bin-каталоги добавлялись в PATH процесса. Первая sandbox-сборка MSVC
получила C1902; обычная сборка вне sandbox прошла. Установки пользователя,
его библиотека проектов и чужие процессы не менялись.

## Границы результата

Windows x64 проверен локально. Linux, macOS, clang-cl, новый CI и разные
физические DPI/мониторы в этом прогоне не проверялись. PortableCore запущен
на Windows; это не доказательство успешной сборки на других ОС.
Полный `test-regression.ps1 -Full` не запускался: выполнены Core/Host/Ui,
Engine/AI и целевые native/API-сценарии новых функций.
ZIP и GitHub Release не создавались. Документация содержит текущие изменения,
правила совместимости и отдельные ссылки на фактические результаты проверки.
