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
1. **Хост как единственный владелец VFS и сервисная модель C-ABI:**
   - Хост (`EngineRuntime.exe`) является единственным владельцем VFS.
   - Модули получают сервисы через `EngineModuleHost::getService(user, serviceId, minVersion)`.
   - Версия модуля обновляется: `ENGINE_MODULE_VERSION -> 5`.
   - Таблицы сервисов начинаются с полей заголовка `{version, size}` и расширяются строго в конец, обеспечивая бинарную совместимость.
2. **Безопасность DLL-границ и C-ABI чтение:**
   - Чтение файлов в C-ABI выполняется одним вызовом с колбэком-аллокатором вызывающей стороны.
   - Используется `uint64_t` вместо `size_t` для размеров буферов и смещений.
   - Чистые коды статуса, полный запрет передачи STL-типов (`std::string`, `std::vector`, `std::filesystem::path`) и C++ исключений через границу DLL.
3. **Жизненный цикл сервисов и прокси VFS в модуле:**
   - Прокси VFS в модуле хранится в `std::atomic` указателе и сбрасывается в `Destroy`.
   - Таблица сервисов хоста живёт в `BaseEngine` дольше загруженных модулей.
   - Подписки организуются с выдачей токена и обязательным `unsubscribe`.
   - Любой fallback на локальный статический VFS (при отсутствии сервиса) обязательно логируется.
4. **Сцены и сохранение:**
   - Хост резолвит виртуальный путь `/Game/...` в абсолютный физический путь; контракт `SceneEditor` (работа с абсолютными путями) не меняется.
   - Атомарная запись сцены остаётся гарантированной (`tmp` + `FlushFileBuffers` + `MoveFileExW` с `MOVEFILE_REPLACE_EXISTING`), при этом временный файл формируется на том же физическом томе, куда резолвится целевой виртуальный путь.
5. **Qt FileBrowser:**
   - Перевод на `QAbstractItemModel` поверх заменяемого источника данных, `QFileSystemModel` полностью убирается.
6. **Фаза 4 и архитектурный roadmap:**
   - Снимок маунтов VFS (`shared_ptr` + `atomic_load`/`store`, C++17).
   - `Stat` (=`GetFileInfo` в сервисе).
   - Атомарная запись как встроенный метод VFS.
   - Опрос изменений (polling).
   - Полное удаление `/_ext_*`. Хеши путей отложены в Backlog (возврат при появлении реестра ассетов или pak).

---

## 4. Backlog
1. `Game/Katamari/Katamari.vcxproj:43-44`: вложенный тег `<Target>` внутри `<Project>` (корневой коммит f9f4c97, есть в main; для исправления требуется правка `Game/`, нужно явное «да» архитектора).
2. Тестирование `Enter/q`: интерактивный выход не покрыт тестами для оконных игр.
3. `run-runtime.ps1`: ветка 'game window' дублирует цикл game.
4. `run-runtime.ps1`: регэксп `GameUpdates=1` ошибочно совпадает со строкой `GameUpdates=12`.
5. Хеши путей VFS (вернуться при проектировании реестра ассетов или формата pak).
6. Предупреждение компилятора C4996 (`getenv`) в `Engine/Source/Base/Base.cpp:636`: вынести чтение переменных окружения в кэшируемую инициализацию.
7. Предупреждение `VCINSTALLDIR` при сборке `EditorModule.vcxproj` через `windeployqt`.
8. Перевод потоков сообщений VFS (`std::fprintf(stderr, ...)`) на единую систему логирования хоста.

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

