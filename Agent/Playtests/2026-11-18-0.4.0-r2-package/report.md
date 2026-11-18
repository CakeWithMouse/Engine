# PainEngine 0.4.0 r2 — готовая поставка Windows x64

ZIP: `Engine/build/releases/PainEngine-0.4.0-r2-win64.zip`, 148268134 байта.
SHA256: `65cbcf1644a74fd60962a684a00df777f9b5568f1a1b286bd032821836b6d0dd`.
Packaging revision: `02cb6215e5aee110a792dcd1b45c4a0839a0c04b`.

Новый комплект собран отдельно от первого ZIP 0.4.0. Основа — main
`48901772`, релизные исправления Mesh и уникальных backup из `477728f7`.
Числовая версия Engine/SDK остаётся 0.4.0; revision в manifest — r2.

## Состав

CMake Release Runtime/Editor/SDK с Jolt и Assimp, пять игр, RuntimeExample,
AiTutorial и свежая CMake DLL RepairCorridor. Включены исходный ZIP стратегии,
офлайн-руководства, Mesh guide, release notes, отчёты и лицензии.
Вспомогательные GameExecutable.exe и EngineCommandlets.exe взяты из MSBuild;
сырая CMake build-папка их не создаёт. CMakeLists.txt и каталоги типов стратегии
включены вместе с исходниками, без SDK-копий и временных каталогов проекта.

Runtime SHA256 `CD8F4218326D91DE9322FFBA2BA093A021F20A2CA7161545442AE796C036F795`.
SDK Engine.lib SHA256 `A4288DC061C10C16B86470C85B97BA79C2342E4369F5BFDAC739E987DEB520CB`.
Game.dll стратегии SHA256 `FB565E3D8CEAA7580D03D27F3F5C8EB8AE9CA7CAD5EB012A7D63D7AB187EA9AD`.
Source ZIP стратегии SHA256 `111B211E3AF7B5FD52879DE12DCC55E46B625A3EA3EA10A591B75DAD97B56321`.

## Проверка папки

24/24: игры, редактор, оба Tutorials, создание нового Unicode-проекта,
C++ Enable/build/startup, экспорт CMake Runtime, запуск Game.exe60 и командлет
Play/Stop. PATH изолирован от DLL рабочего checkout; EngineStore отдельный.
Проверены 15 HTML,130 локальных ссылок,182 PE x64; пропущенных ссылок и DLL нет.
Результаты — [VALIDATION.json](VALIDATION.json),
[файлы и зависимости](files-validation.json), [журнал](validation.log).

Полные логи находятся также в Validation внутри комплекта. Первый архив,
исторические отчёты и source ZIP r1 сохранены. Ручной Inspector-проход агента
partial; коллизия импортированной сцены после reopen не запускалась.
Процедурные Mesh-столкновения и native Inspector RU/EN прошли отдельно.
Linux/macOS/clang-cl для этой поставки не проверены.

## Воспроизведение

Упаковка: `Engine/BuildSupport/New-ReleaseArchive.py` с `--revision r2`,
`--strategy .../fresh-final-cmake/RepairCorridor`, новым source ZIP и двумя
новыми `--report`. Проверка готовой папки — `Test-ReleasePackage.ps1`.
ZIP создаётся только после VALIDATION.json. `Test-ReleaseArchive.py` проверяет
SHA256 ZIP и CRC, безопасно распаковывает в новый каталог и сверяет SHA256
каждого файла. После этого `Test-ReleasePackage.ps1 -SmokeOnly` повторяет
запуски из распаковки. GitHub Release не создаётся; ZIP сохраняется локально.

ZIP SHA256 и CRC прошли, все 1580 SHA256 совпали после новой распаковки:
[extraction-integrity.json](extraction-integrity.json). Последний fetch перед
публикацией подтвердил тот же origin/main `48901772`; удалённая release-ветка
оставалась на прежнем `b4a10514`, конфликтующих публикаций не было.

Все 19/19 запусков из новой распаковки прошли:
[extracted-VALIDATION.json](extracted-VALIDATION.json),
[журнал](extracted-validation.log). Рядом с ZIP сохранены `.sha256` и
`.zip.validation.json` с полной проверкой папки, распаковки и целостности.
Отправка release/v0.4.0 в https://github.com/CakeWithMouse/Engine.git
авторизована пользователем ранее и выполняется обычным push после фиксации
этого аудита. Ветка main не публикуется.
