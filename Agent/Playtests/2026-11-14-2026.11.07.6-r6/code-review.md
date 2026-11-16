# Проверка кода r6

База main d74aebb: релизный a7f8c8d (2026.11.07.6) и 23 изменённых файла
настроек импорта после него. Проверка включала полную Release All сборку,
межкомпонентные регрессии и прицельное чтение кода. Это не построчное ручное
ревью каждого файла и не доказательство отсутствия всех ошибок.

| Область | Чтение кода | Практическое свидетельство |
|---|---|---|
| Импорт/масштаб/sidecar | координатор + curious-city: ImportSettings, ModelImportSidecar, ImportCommands, AssetTools, FileService, UI | BUG-R6-001/002 до/после; project.import; Core/UI/Host |
| Материалы/каскады/point shadows/прозрачность | calm-city: Game shadow camera, instanced streams, MaterialTemplate, transparency queue | собственные сцены роли + GPU suites |
| SaveGame/input/trace | tired-bomber: capture/restore/archive, GameTrace, project.input | игровой hit, SaveGame, input и scene suites |
| CLI/SDK/старые проекты | координатор: launcher/project workflow, ProjectTools, ABI/версии | All, compatibility, generator, четыре независимые сборки |
| Editor commandlets | координатор: сервисы host, callback/владение окном, настоящий import/spawn | параметризованный project.import и сценарии жалоб |

Минимальные доработки: ранняя валидация scale, проверка сегментов пути вместо
подстроки; рефакторинг HostTests убрал запись тестом ожидаемого JSON, заменённую
настоящим e2e сценарием. Общая архитектура Engine/Game сохранена.

Полные команды и exit codes: fixes/regressions.json; поведенческие доказательства
ролей: их evidence.json. Компиляция, CPU-контракт, GPU-пиксели и осмотр кадра
учитываются отдельно. Существующие предупреждения Qt deprecated addAction не
являются новыми runtime-дефектами; массовая косметическая правка не выполнялась.
