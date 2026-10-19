# 037-PLAN: VFS Hardening — Единый VFS через границы DLL, интеграция SceneDocument и FileBrowser

## 1. Цели и контекст
- **Цель:** Обеспечить строгую архитектуру виртуальной файловой системы (VFS), управляемой единым экземпляром `VirtualFileSystem` на стороне хоста (`EngineRuntime.exe`), пробросить доступ к ней в `Game.dll` и `Editor.dll` через C-ABI границы без разделения на независимые статические копии, избавиться от неконтролируемых динамических маунтов `/_ext_*`, перевести загрузку/сохранение сцен и файловый браузер редактора на VFS API.

---

## 2. Чек-лист реализации (Фазы 0–5)

- [ ] **Фаза 0: Разведка (текущий этап)**
  - [x] Создание рабочей ветки `feature/vfs-hardening` от `integration/scene-vfs`.
  - [x] Создание документа плана `Agent/Tasks/037-PLAN.md` (коммит `docs(plan)`).
  - [ ] Анализ текущего механизма получения VFS в `Game.dll` и проверка прохождения путей ресурсов (шейдеры/текстуры) через `/_ext_*`.
  - [ ] Полный аудит точек вызова VFS во всех модулях движка и классификация по типам (`host`, `Game.dll`, `Editor.dll`, автономная игра).
  - [ ] Исследование реализации `FileBrowser` в `Editor/` (Qt-классы, дублирование функционала VFS, модель поиска и пути проекта).
  - [ ] Проектирование C-ABI интерфейса хоста для передачи VFS в подключаемые модули (`ModuleApi.h`, `EditSessionApi.h`).
  - [ ] Анализ потокобезопасности `VirtualFileSystem`, семантики `Resolve` для новых путей (Save As), граничных случаев `EnumerateFiles` и механизма уведомлений (watcher).
  - [ ] Анализ размера файлов редактора (`wc -l` по компонентам `FileBrowser`, выявление файлов > 500 строк).
  - [ ] Оценка рисков удаления `/_ext_*` (зависимости в тестах и автономных играх).

- [ ] **Фаза 1: C-ABI VFS мост (Host -> Game.dll, Editor.dll)**
  - [ ] Проектирование структуры `EngineVfsApi` с чистым C-ABI (функции чтения, записи, проверки, перечисления, резолва).
  - [ ] Передача указателя на `EngineVfsApi` через `EngineModuleHost` при вызове `create`.
  - [ ] Переключение `VfsRead` внутри DLL на использование хостового VFS вместо локального статического `s_vfs`.
  - [ ] Валидация синхронизации точек монтирования `/Engine` и `/Game` между хостом и загруженными модулями.

- [ ] **Фаза 2: Устранение `/_ext_*` и строгий аудит путей**
  - [ ] Запрет неявного монтирования родительских каталогов в `VfsRead::ToVirtualPath`.
  - [ ] Приведение путей загрузки шейдеров и текстур к каноническим виртуальным корням (`/Engine/...`, `/Game/...`).
  - [ ] Исправление тестов, полагавшихся на неявный динамический маунт.
  - [ ] Устранение утечки точек монтирования при перезапусках сессий движка.

- [ ] **Фаза 3: Перевод SceneDocument и SceneEditSession на VFS**
  - [ ] Перевод `SceneDocument::Read` на чтение через VFS API.
  - [ ] Реализация атомарной записи сцены с поддержкой виртуальных путей `/Game/...` (создание временного файла рядом с целевым на том же физическом томе).
  - [ ] Адаптация `SceneEditSession.cpp` (`DocumentCall`) для поддержки виртуальных путей без требования `is_absolute()`.
  - [ ] Обновление логики `startupScene` в `Project.cpp` и `Base.cpp` на использование канонических путей `/Game/...`.

- [ ] **Фаза 4: Перевод FileBrowser редактора на VFS**
  - [ ] Замена прямого использования `QFileSystemModel` / `QDir` на модель данных на базе VFS API (`EnumerateFiles`, `GetFileInfo`, `FindFiles`).
  - [ ] Реализация файловых операций редактора (создание папок, переименование, удаление, импорт) через VFS API.
  - [ ] Интеграция виртуального корня проекта (`/Game`) в дерево файлов редактора.

- [ ] **Фаза 5: Тестирование и стабилизация**
  - [ ] Добавление консольных тестов `SceneDocument` (чтение, запись, раундтрип, обработка ошибок) в `EngineTests.exe`.
  - [ ] Прогон всех наборов: `run.ps1` (21+), `run-runtime.ps1`, `edit-session/run.ps1` (575), `scene-module/run.ps1` (109), `run-games.ps1`.
  - [ ] Проверка компиляции Debug и Release: 0 ошибок, 0 новых предупреждений.
  - [ ] Проверка отсутствия `imgui|Qt|QFile` в подсистемах ядра (`Engine/Include`, `Engine/Source/Render`, `Engine/Source/Resources`).

---

## 3. Решения
1. **Единый источник истины для VFS:** Хост (`EngineRuntime.exe`) владеет экземпляром `VirtualFileSystem` и передает C-ABI таблицу указателей на функции в загружаемые модули через `EngineModuleHost`. Модули не должны владеть независимыми несогласованными маунтами.
2. **Атомарная запись:** Сохранение сцены обязано оставаться атомарным (`tmp` + `FlushFileBuffers` + `MoveFileExW` с `MOVEFILE_REPLACE_EXISTING`), при этом временный файл должен формироваться на том же физическом томе, куда резолвится целевой виртуальный путь.
3. **Безопасность DLL-границ:** Никаких STL-типов (`std::string`, `std::filesystem::path`) и исключений через границу DLL. Все пути передаются как `const char*` UTF-8.

---

## 4. Backlog
1. Предупреждение компилятора C4996 (`getenv`) в `Engine/Source/Base/Base.cpp:636`: вынести чтение переменных окружения в кэшируемую инициализацию.
2. Синтаксический дефект в `Game/Katamari/Katamari.vcxproj:43-44` (вложенный тег `<Target>` внутри `<Project>` в `ProjectReference`), блокирующий сборку цели `Katamari`.
3. Предупреждение `VCINSTALLDIR` при сборке `EditorModule.vcxproj` через `windeployqt`.
4. Перевод потоков сообщений VFS (`std::fprintf(stderr, ...)`) на единую систему логирования хоста.

---

## 5. Не проверено в рантайме / известные падения

### Базовая линия тестов (Фаза 1, Шаг 0)
- **Debug:**
  - `Engine\tests\run.ps1`: **21/21 PASS**
  - `Engine\tests\run-runtime.ps1`: **23/23 PASS**
  - `Game\tests\run-games.ps1`: **24/24 PASS** (1 пропуск: `Katamari` runtime не собран, требует Assimp)
  - `Engine\tests\edit-session\run.ps1`: **575/575 PASS**
  - `Engine\tests\scene-module\run.ps1`: **109/109 PASS**
  - `Engine\tests\component-generation\run.ps1`: **PASS** (17 scanner + 4 contract rejections)
  - `Engine\tests\run-viewport.ps1`: **6/6 PASS**
- **Release:**
  - `Engine\tests\run.ps1`: **21/21 PASS**
  - `Engine\tests\run-runtime.ps1`: **23/23 PASS**
  - `Game\tests\run-games.ps1`: **24/24 PASS** (1 пропуск: `Katamari`)
  - `Engine\tests\edit-session\run.ps1`: **575/575 PASS**
  - `Engine\tests\scene-module\run.ps1`: **109/109 PASS**

### Интерактивный тест `run-editor-viewport.ps1`:
- **Результат:** Падение в PowerShell-обвязке скрипта при получении кода возврата процесса:
  ```text
  Editor exited with 
  At C:\Users\Yuri\Documents\GitHub\Engine\Engine\tests\run-editor-viewport.ps1:111 char:36
  + ... s.ExitCode -ne 0) { throw "Editor exited with $($process.ExitCode)" }
  ```
- **Сырой лог выполнения (stdout):**
  Процесс запустился, создал окно, привязал вьюпорт, отрендерил кадр, получил WM_CLOSE и корректно остановился:
  `ViewportReady` -> `EditorViewportAttach ok` -> `EngineRender (viewport)` -> `EditorRequestExit` -> `ViewportStopped` -> `EditorUpdates=97` -> `EditorStop` -> `EditorDestroy` -> `ModuleUnloaded` -> `ResourcesStopped (stub)` -> `EngineStopped`.
- **Причина отказа:** В `Start-Process ... -WindowStyle Hidden -PassThru` свойство `$process.ExitCode` в Windows PowerShell оказывается `$null` после `WaitForExit` без явного `Handle`. Сам движок и редактор отработали штатно без крашей.

### Прочее не проверенное:
- Поведение Qt FileBrowser при динамическом обновлении файлов вне процесса (отсутствие FileSystemWatcher на виртуальных корнях).
- Атомарное сохранение на нестандартных томах (RAM-диски, сетевые шары SMB).

