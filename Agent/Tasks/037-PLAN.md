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
9. Разбить `Engine/tests/EngineTests.cpp` (604 строки, лимит 500 строк превышен ещё до Фазы 1 — было 589 строк на коммитах `a5ec278`/`75cfc5a`). Вынести тесты шейдеров, VFS и компонентов в отдельные файлы тестового проекта (`VfsTests.cpp`, `ShaderCompilerTests.cpp`).

---

## 5. Не проверено в рантайме / закрытие оговорок Фазы 1

### Итог закрытия оговорок Фазы 1 (Фаза 2a, Раздел A)
1. **A1. Положительный контроль Dynamic mount:**
   - Проведён запуск на временном worktree родительского коммита `bc2a5c5` (до коммита `0ca354f`).
   - Команда `powershell -ExecutionPolicy Bypass -Command ".\Engine\tests\run.ps1 2>&1 | Select-String 'Dynamic mount'"` вернула **1 совпадение**:
     `EngineTests.exe : [VfsRead] Dynamic mount '/_ext_7130158442022204148' -> 'C:/Users/Yuri/Documents/GitHub/temp-wt-a1/Engine/tests/ShaderIncludeFixture' (path 'C:/Users/Yuri/Documents/GitHub/temp-wt-a1/Engine/tests/ShaderIncludeFixture/a.hlsl' outside registered mounts)`
   - В `run.ps1` запуск выполняется напрямую оператором вызова: `& (Join-Path $PSScriptRoot "../build/x64/$Configuration/EngineTests.exe")`. Потоки stdout/stderr наследуются процессом. При перенаправлении `2>&1` PowerShell упаковывает строки `stderr` в записи ошибок `RemoteException / NativeCommandError`, которые успешно отфильтровываются `Select-String`.
   - Положительный контроль подтверждён: на родителе 1 совпадение, на текущем коммите `0ca354f` — 0 совпадений. Доказательство Фазы 1 полностью действительно.
2. **A2. Аудит предупреждений сборки:**
   - Выполнена полная пересборка Debug (`/t:Rebuild`) через `MSBuild.exe Engine\Engine.sln /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /m:1 /v:m`. Сборка завершилась с кодом 0.
   - Список всех уникальных предупреждений:
     - `Engine/Source/Base/Base.cpp(636,26): warning C4996: 'getenv'` (известно, бэклог #6).
     - `Editor/EditorModule.vcxproj: warning : Cannot find any version of the dxcompiler.dll and dxil.dll` (предупреждение утилиты `windeployqt`).
     - `Editor/EditorModule.vcxproj: warning : Cannot find Visual Studio installation directory, VCINSTALLDIR is not set` (известно, бэклог #7, вызов `windeployqt`).
   - Новых предупреждений компилятора CL: **0**.
3. **A3. Реальные счётчики строк (`git grep -c "^"`):**
   - `Engine/tests/edit-session/SceneJsonTests.h`: **293** строки (< 500).
   - `Engine/tests/edit-session/EditSessionTests.cpp`: **209** строк (< 500).
   - `Engine/tests/EngineTests.cpp`: **604** строки (> 500).
   - `Engine/tests/EngineTests.cpp` на родительских коммитах `a5ec278` и `75cfc5a`: **589** строк. Файл превышал лимит ещё до начала Фазы 1. Задача разбиения внесена в Backlog (#9).
4. **A4. Раскладка проверок `run-games.ps1` (25 vs 24 + 1 skip):**
   - Скрипт `Game/tests/run-games.ps1` выполняет вызовы `Invoke-CheckedProcess`, каждый успешный шаг выводит `PASS: <command> => <exit_code>`.
   - Состав всех 25 проверок (при запуске без Assimp):
     1. `PASS: BasicExamples.exe --smoke => 0`
     2. `PASS: EngineLauncher.exe --check BasicExamples => 0`
     3. `PASS: EngineLauncher.exe --smoke BasicExamples => 0`
     4. `PASS: Pong.exe --smoke => 0`
     5. `PASS: EngineLauncher.exe --check Pong => 0`
     6. `PASS: EngineLauncher.exe --smoke Pong => 0`
     7. `PASS: SunGame.exe --smoke => 0`
     8. `PASS: EngineLauncher.exe --check SunGame => 0`
     9. `PASS: EngineLauncher.exe --smoke SunGame => 0`
     10. `PASS: Battle.exe --smoke => 0`
     11. `PASS: EngineLauncher.exe --check Battle => 0`
     12. `PASS: EngineLauncher.exe --smoke Battle => 0`
     13. `PASS: BasicExamples.exe --scene triangles --smoke => 0`
     14. `PASS: BasicExamples.exe --scene cube --smoke => 0`
     15. `PASS: BasicExamples.exe --smoke --scene unknown => 1`
     16. `PASS: Battle.exe --units 5000 --frames 60 => 0`
     17. `PASS: Battle.exe --units 0 --smoke => 1`
     18. `PASS: EngineLauncher.exe --check Katamari => 1` (ветка `else` при отсутствии Assimp: проверка, что лаунчер сообщает о несобранной игре)
         - Затем выводится текст `Katamari runtime: not built (requires Assimp).` (сообщение о пропуске рантайм-смоука).
     19. `PASS: EngineLauncher.exe --check UnknownGame => 2`
     20. `PASS: EngineLauncher.exe --check BasicExamples => 1` (фикстура: отсутствующий exe)
     21. `PASS: EngineLauncher.exe --check BasicExamples => 1` (фикстура: отсутствующий манифест)
     22. `PASS: EngineLauncher.exe --check BasicExamples => 1` (фикстура: отсутствующий шейдер)
     23. `PASS: BasicExamples.exe --smoke => 1` (фикстура: прямой запуск завершается ошибкой)
     24. `PASS: EngineLauncher.exe --check BasicExamples => 1` (фикстура: отсутствующий ассет модели)
     25. `PASS: EngineLauncher.exe --smoke BasicExamples => 0` (запуск лаунчера из каталога с пробелами)
   - Различие формулировок: «25» — общее число строк `PASS:`, выводимых скриптом в консоль. «24 + 1 skip» — 24 теста работающих сценариев и фикстур + 1 пропуск сборки Katamari (проверяемый вызовом `--check Katamari => 1` с печатью информационного сообщения). В логике проверок ничего не менялось.

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

---

## 6. Фаза 2a: проект ABI сервиса VFS

### B1. Жизненный цикл EngineModuleHost
- **Создание хоста:** В `Engine/Source/Base/Base.cpp:588` локальная структура `host` создаётся на стеке функции `BaseEngine::Play`:
  ```cpp
  EngineModuleHost host{ENGINE_MODULE_VERSION, this, Log, Exit,
      options.projectName.c_str(), options.projectFile.c_str(), options.contentRoot.c_str()};
  ```
- **Передача в сессию:** На строках `Base.cpp:638+` хост передаётся по константной ссылке в метод `activeSession->Open(modulePath, kind, host, ...)`.
- **Копирование и вызов create:** В `LoadedSession::LoadAndInitialize` (`Base.cpp:205, 240`):
  ```cpp
  moduleHost = host; // копирование по значению в поле класса LoadedSession
  // ...
  session = api.create(&moduleHost); // передаётся адрес поля объекта LoadedSession
  ```
- **Время жизни указателя:** Указатель `&moduleHost` указывает на поле `activeSession->moduleHost`, существующее в куче всё время активности сессии вплоть до вызова `api.destroy(session)` в деструкторе/методе `LoadedSession::Close()`.
- **Хранение хоста в модулях:**
  - `Game.dll` (`Game/RuntimeExample/Source/Module.cpp:18, 25, 32`): структура `Session` содержит собственное поле `EngineModuleHost host;` и инициализирует его копированием: `explicit Session(const EngineModuleHost& value) : host(value) {}`.
  - `Editor.dll` (`Editor/Source/Module.cpp:12`, `Editor/Source/QtHost.cpp:19`): класс `EditorQtHost` содержит поле `EngineModuleHost host_;` и копирует структуру: `EditorQtHost::EditorQtHost(const EngineModuleHost& host) : host_(host) {}`.
  - **Вывод:** Модули копируют `EngineModuleHost` по значению. Поле `user` указывает на `BaseEngine`, который гарантированно живёт дольше модулей. Модули могут безопасно использовать `host.getService(host.user, ...)` на протяжении всего своего жизненного цикла.

### B2. Методы VFS, необходимые на стороне модулей
Аудит всех вызовов VFS в коде подсистем ядра и модулей:
1. `Engine/Source/Render/TextureLoader.cpp:28`:
   - `const std::vector<uint8_t> fileBytes = VfsRead::ReadBytes(path);`
   - Требуется: чтение сырых байтов файла по виртуальному или относительному пути.
2. `Engine/Source/Render/ShaderCompiler.cpp:68, 72, 79, 99`:
   - `auto& vfs = VfsRead::GetVFS();`
   - `vfs.Exists(targetVirtual)` (строки 72, 79) -> `Exists`.
   - `vfs.ReadBytes(targetVirtual)` (строка 99) -> `ReadFile`.
   - Требуется: проверка существования файла и синхронное чтение байтов `#include` файлов.
3. `Engine/Source/Render/ShaderCompiler.cpp:164`:
   - `const std::vector<uint8_t> shaderSource = VfsRead::ReadBytes(utf8Path, &virtualPath);`
   - Требуется: чтение байтов шейдера с трансляцией физического/относительного пути в канонический виртуальный (`ToVirtual`).
4. `Engine/Source/Resources/VfsRead.cpp:76-176`:
   - `ToVirtualPath(vfs, path)`: выполняет нормализацию, проверку префиксов и сопоставление физического пути со списком точек монтирования `vfs.GetMounts()` (строки 94, 144).
5. `Engine/Source/MainGame/BaseGameClass/Game.cpp`:
   - Прямых вызовов VFS нет. Загрузка ресурсов делегируется в `TextureLoader` и `ShaderCompiler`.

**Необходимость `toVirtual(ctx, physical, ...)` в C-ABI сервисе:**
- Метод `toVirtual` **критически необходим** в C-ABI сервисе.
- Причина: в настоящее время `ToVirtualPath` напрямую обходит `vfs.GetMounts()`, возвращающий `std::vector<MountPoint>`, содержащий `std::string` и `std::filesystem::path`. Передача контейнеров STL через границу DLL недопустима из-за несовместимости ABI разных рантаймов MSVC.
- Решение: хост выполняет сопоставление физических путей с точками монтирования на своей стороне и отдаёт канонический виртуальный путь через C-ABI буферную функцию `toVirtual(ctx, physical, out, capacity, needed)`.

### B3. Полный текст заголовка Engine/Include/Engine/Runtime/VfsApi.h

```c
#pragma once
#include <stdint.h>

// ============================================================================
// Engine VFS Service C-ABI Interface
// Windows x64, C calling convention, UTF-8 strings, no STL, no C++ exceptions.
// All buffer sizes and offsets use uint64_t.
// ============================================================================

#define ENGINE_SERVICE_VFS        0x53465601u // 'VFS' service ID
#define ENGINE_VFS_API_VERSION    1u

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EngineVfsStatus
{
    ENGINE_VFS_OK               = 0,
    ENGINE_VFS_NOT_FOUND        = 1,
    ENGINE_VFS_IO_ERROR         = 2,
    ENGINE_VFS_ALLOC_FAILED     = 3,
    ENGINE_VFS_BAD_ARGUMENT     = 4,
    ENGINE_VFS_NOT_SUPPORTED    = 5,
    ENGINE_VFS_BUFFER_TOO_SMALL = 6
} EngineVfsStatus;

typedef enum EngineVfsEntryType
{
    ENGINE_VFS_TYPE_FILE      = 1,
    ENGINE_VFS_TYPE_DIRECTORY = 2,
    ENGINE_VFS_TYPE_UNKNOWN   = 3
} EngineVfsEntryType;

struct EngineServiceHeader
{
    uint32_t version;
    uint32_t size;
};

struct EngineVfsStat
{
    uint32_t type;       // EngineVfsEntryType
    uint32_t reserved;   // 64-bit alignment padding
    uint64_t size;       // File size in bytes
    uint64_t mtime;      // Last modification time (UTC Unix epoch seconds)
};

// Allocator callback for file reading.
// Invoked synchronously by the host on the calling thread.
// Caller allocates memory of at least 'size' bytes and returns a pointer.
// If size == 0, returning nullptr or a non-null placeholder is valid.
// If memory allocation fails, returning nullptr causes readFile to abort and return ENGINE_VFS_ALLOC_FAILED.
// Host NEVER stores, reallocates, or frees this pointer.
typedef void* (*EngineVfsAllocFn)(void* allocUser, uint64_t size);

// Enumeration callback. Return 1 to continue iteration, 0 to abort early.
typedef int (*EngineVfsEnumerateCallback)(void* user, const char* name, uint32_t type, uint64_t size);

// Search callback. Return 1 to continue iteration, 0 to abort early.
typedef int (*EngineVfsFindCallback)(void* user, const char* vpath, uint32_t type, uint64_t size);

// Mount point enumeration callback. Return 1 to continue, 0 to abort.
typedef int (*EngineVfsMountCallback)(void* user, const char* vroot, const char* physicalPath);

// File change notification callback (Phase 4).
typedef void (*EngineVfsChangeCallback)(void* user, const char* vpath, uint32_t changeType);

struct EngineVfsApi
{
    // Service header: version must be >= ENGINE_VFS_API_VERSION, size >= sizeof(EngineVfsApi)
    struct EngineServiceHeader header;
    void* context;

    // --- Core Read & Inspection Operations (Phase 2) ---

    // Resolves a virtual path to a physical filesystem path.
    // Returns ENGINE_VFS_OK, ENGINE_VFS_NOT_FOUND, or ENGINE_VFS_BUFFER_TOO_SMALL.
    // If capacity is insufficient, *needed contains the required byte count including null terminator.
    EngineVfsStatus (*resolve)(void* ctx, const char* vpath, char* outPhysical, uint64_t capacity, uint64_t* needed);

    // Translates a physical path or relative path to a canonical virtual path.
    EngineVfsStatus (*toVirtual)(void* ctx, const char* physical, char* outVpath, uint64_t capacity, uint64_t* needed);

    // Checks if a virtual path exists. Returns 1 if exists, 0 if not or on invalid argument.
    int (*exists)(void* ctx, const char* vpath);

    // Queries file/directory metadata.
    EngineVfsStatus (*stat)(void* ctx, const char* vpath, struct EngineVfsStat* outStat);

    // Synchronously reads file bytes using the caller's allocator callback.
    // If outReadBytes is not null, receives the exact count of bytes read.
    EngineVfsStatus (*readFile)(void* ctx, const char* vpath, EngineVfsAllocFn allocFn, void* allocUser, uint64_t* outReadBytes);

    // Enumerates directory entries in vdir.
    EngineVfsStatus (*enumerate)(void* ctx, const char* vdir, int recursive, EngineVfsEnumerateCallback callback, void* user);

    // Searches directory entries matching glob/extension pattern.
    EngineVfsStatus (*find)(void* ctx, const char* vdir, uint32_t typeFilter, const char* pattern, int recursive, EngineVfsFindCallback callback, void* user);

    // Enumerates active mount points.
    EngineVfsStatus (*getMounts)(void* ctx, EngineVfsMountCallback callback, void* user);

    // --- Mutating Operations (Reserved for Phase 4; return ENGINE_VFS_NOT_SUPPORTED until implemented) ---

    EngineVfsStatus (*writeFile)(void* ctx, const char* vpath, const void* data, uint64_t size, int atomic);
    EngineVfsStatus (*createDir)(void* ctx, const char* vdir, int recursive);
    EngineVfsStatus (*moveFile)(void* ctx, const char* oldVpath, const char* newVpath);
    EngineVfsStatus (*copyFile)(void* ctx, const char* srcVpath, const char* dstVpath);
    EngineVfsStatus (*deleteFile)(void* ctx, const char* vpath);

    // --- Change Notification Subscriptions (Reserved for Phase 4; return ENGINE_VFS_NOT_SUPPORTED) ---

    EngineVfsStatus (*subscribe)(void* ctx, const char* vdirPrefix, EngineVfsChangeCallback callback, void* user, uint64_t* outToken);
    EngineVfsStatus (*unsubscribe)(void* ctx, uint64_t token);
};

#ifdef __cplusplus
}
#endif
```

#### Итоговая структура `EngineModuleHost` (версия 5u)

```c
#define ENGINE_MODULE_VERSION 5u

struct EngineModuleHost
{
    uint32_t version;
    void* user;
    void (ENGINE_MODULE_CALL *log)(void* user, const char* message);
    void (ENGINE_MODULE_CALL *requestExit)(void* user);
    const char* projectName;
    const char* projectFile;
    const char* contentRoot;
    // Optional v2 viewport services (nullptr = unavailable). Call on the host thread.
    int (*viewportAttach)(void* user, void* hwnd, uint32_t w, uint32_t h);
    int (*viewportResize)(void* user, uint32_t w, uint32_t h);
    int (*viewportDetach)(void* user);
    // v3: camera intent
    void (*viewportCamera)(void* user, float right, float up, float forward, float yaw, float pitch, float zoom);
    // v4: session state control
    int (*viewportControl)(void* user, uint32_t command);
    // v5: service broker for host capabilities.
    // Returns a pointer to the requested service table, or nullptr if unavailable / version mismatch.
    const void* (*getService)(void* user, uint32_t serviceId, uint32_t minVersion);
};
```

---

### B4. Архитектура на стороне модуля (IVfs, LocalVfs, HostVfsProxy, VfsRead::Active)

1. **C++ интерфейс `IVfs`:**
   ```cpp
   class IVfs
   {
   public:
       virtual ~IVfs() = default;
       virtual bool Exists(std::string_view vpath) = 0;
       virtual std::vector<uint8_t> ReadBytes(std::string_view vpath) = 0;
       virtual std::string ReadTextFile(std::string_view vpath) = 0;
       virtual std::optional<std::filesystem::path> Resolve(std::string_view vpath) = 0;
       virtual std::string ToVirtual(std::string_view path) = 0;
   };
   ```

2. **Две реализации интерфейса:**
   - `LocalVfs : public IVfs`: оборачивает локальный экземпляр `VirtualFileSystem` (`s_vfs`). Используется в автономных играх (`BasicExamples`, `Pong`, `SunGame`, `Battle`, `Katamari`) и изолированных тестах движка.
   - `HostVfsProxy : public IVfs`: реализует `IVfs` поверх `const EngineVfsApi*`.
     - Вызывает C-ABI функцию `readFile` с лямбда-аллокатором, выделяющим память напрямую в `std::vector<uint8_t>`:
       ```cpp
       std::vector<uint8_t> out;
       auto alloc = [](void* user, uint64_t size) -> void* {
           auto* v = static_cast<std::vector<uint8_t>*>(user);
           v->resize(static_cast<size_t>(size));
           return v->empty() ? nullptr : v->data();
       };
       api->readFile(api->context, vpath.c_str(), alloc, &out, nullptr);
       ```
     - Транслирует `Resolve` и `ToVirtual` через буферные функции с запросом необходимой ёмкости.

3. **Глобальная диспетчеризация `VfsRead::Active()`:**
   - Статический атомарный указатель: `static std::atomic<IVfs*> s_activeVfs{nullptr};`
   - Статический fallback: `static LocalVfs s_localFallback;`
   - Статический флаг логирования: `static std::atomic<bool> s_fallbackLogged{false};`
   - Метод привязки: `VfsRead::Bind(const EngineVfsApi* api)` — инстанциирует `HostVfsProxy` и атомарно записывает указатель в `s_activeVfs`.
   - Метод отвязки: `VfsRead::Unbind()` — вызывается в `Destroy` модуля, сбрасывает `s_activeVfs` в `nullptr` и освобождает память прокси.
   - Метод `VfsRead::Active()`:
     ```cpp
     IVfs& Active() noexcept
     {
         if (IVfs* vfs = s_activeVfs.load(std::memory_order_acquire))
         {
             return *vfs;
         }
         if (!s_fallbackLogged.exchange(true, std::memory_order_relaxed))
         {
             std::fprintf(stderr, "[VfsRead] Warning: Host VFS service not bound. Falling back to LocalVfs.\n");
         }
         return s_localFallback;
     }
     ```

4. **Сохранение существующих сигнатур функций:**
   - Сигнатуры `VfsRead::ReadBytes(path, outVirtualPath)` и `VfsRead::ReadTextFile(path)` остаются **неизменными**. Внутри они делегируют вызов в `Active().ToVirtual(path)` и `Active().ReadBytes(...)`.
   - Сигнатура `VfsRead::ToVirtualPath(VirtualFileSystem& vfs, std::string_view path)` остаётся для обратной совместимости, а для модуля добавляется перегрузка `VfsRead::ToVirtualPath(std::string_view path)` -> `Active().ToVirtual(path)`.
   - `BaseEngine::BaseEngine()` (`Base.cpp:392`): строка `: Vfs(VfsRead::GetVFS())` не затрагивается, так как метод `GetVFS()` сохраняется для хостовой стороны.

---

### B5. Хостовая сторона реализации сервиса
- **Размещение кода реализации:**
  - Поскольку `Base.cpp` уже насчитывает 1002 строки, добавление реализации C-ABI в него недопустимо.
  - Реализация сервиса создаётся в новом файле `Engine/Source/Runtime/VfsService.cpp` и заголовке `Engine/Source/Runtime/VfsService.h`.
  - Заголовок экспортирует функцию: `EngineVfsApi CreateVfsService(VirtualFileSystem& vfs);`.
- **Минимальные изменения в `Base.cpp` (~10 строк):**
  1. Добавление в `BaseEngine` поля `EngineVfsApi vfsServiceApi;`.
  2. Инициализация в конструкторе `BaseEngine`: `vfsServiceApi = CreateVfsService(Vfs);`.
  3. В `BaseEngine::Play` при формировании `host`:
     ```cpp
     host.getService = [](void* user, uint32_t serviceId, uint32_t minVersion) -> const void* {
         auto* engine = static_cast<BaseEngine*>(user);
         if (serviceId == ENGINE_SERVICE_VFS && minVersion <= ENGINE_VFS_API_VERSION)
             return &engine->vfsServiceApi;
         return nullptr;
     };
     ```
- **Время жизни таблицы:**
  - Таблица `vfsServiceApi` хранится в объекте `BaseEngine`.
  - `BaseEngine` создаётся в `Main.cpp` и разрушается только при завершении приложения, то есть живёт заведомо дольше любого загружаемого модуля (`Game.dll`, `Editor.dll`).

---

### B6. Совместимость версий и обновление 4u -> 5u

1. **Места использования `ENGINE_MODULE_VERSION` в проекте:**
   - `Engine/Include/Engine/Runtime/ModuleApi.h:9`: определение `#define ENGINE_MODULE_VERSION 4u` -> меняется на `5u`.
   - `Engine/Source/Base/Base.cpp:229, 588`: проверка версии модуля в `LoadedSession` и инициализация `host.version`.
   - `Game/RuntimeExample/Source/Module.cpp:30, 93, 94`: валидация в `Create` и экспорт `EngineGetModuleApi`.
   - `Editor/Source/Module.cpp:10, 26, 27`: валидация в `Create` и экспорт `EngineGetModuleApi`.
   - `Engine/tests/runtime/Fixture.cpp:46, 47`: фикстура проверки `BadVersion` (возвращает 999u для кейса 1).
   - `Engine/tests/edit-session/Consumer.cpp:46, 47`: тестовый потребитель сессии.
   - `Engine/tests/edit-session/GameStartupTests.h:34, 36`: тесты старта игры.
   - `Engine/tests/scene-module/SceneModuleTests.cpp:139, 140`: тесты контракта модуля.

2. **Что потребуется пересобрать и обновить:**
   - Все модули движка (`EngineRuntime.exe`, `RuntimeExample` -> `Game.dll`, `EditorModule` -> `Editor.dll`).
   - Тестовые фикстуры `Fixtures.proj` и `Consumer.vcxproj`.
   - В `run-runtime.ps1` изменений кода не требуется: скрипт компилирует фикстуры на лету перед запуском и проверяет контракт ошибки `Incompatible module version or kind`.

3. **Уведомление для автора `SceneEditor`:**
   - Версия интерфейса модуля повышена до `ENGINE_MODULE_VERSION = 5`.
   - В структуру `EngineModuleHost` добавлено поле `const void* (*getService)(void* user, uint32_t serviceId, uint32_t minVersion)`.
   - Функция `EngineGetModuleApi` в `SceneEditor` должна принимать и возвращать версию `5u`.
   - Для доступа к VFS редактор сцен должен запрашивать интерфейс сервиса через `host->getService(host->user, ENGINE_SERVICE_VFS, ENGINE_VFS_API_VERSION)` и вызывать `VfsRead::Bind(api)` в `Create`, а `VfsRead::Unbind()` — в `Destroy`.

---

### B7. План тестов Фазы 2b (без правок кода)

Для соблюдения лимита в 500 строк тесты не будут добавляться в `EngineTests.cpp` (уже 604 строки).
Будут созданы два новых изолированных тестовых файла:

1. **Новый файл `Engine/tests/VfsServiceTests.cpp` (~250 строк, входит в `EngineTests.vcxproj`):**
   - `VfsService_Resolve`: валидация резолва `/Engine/Shaders/RotatedFigure.hlsl`, обработка граничных условий по размеру буфера (передача `capacity = 0`, проверка `needed`, повторный вызов с достаточным буфером).
   - `VfsService_ToVirtual`: проверка трансляции физического пути внутри каталога `/Engine` в канонический виртуальный путь; проверка обработки недостаточного буфера.
   - `VfsService_Exists`: проверка существующего и несуществующего пути.
   - `VfsService_Stat`: проверка `ENGINE_VFS_TYPE_FILE` и размера файла; проверка `ENGINE_VFS_TYPE_DIRECTORY` для каталогов; возврат `ENGINE_VFS_NOT_FOUND` для отсутствующих путей.
   - `VfsService_ReadFile_Normal`: чтение файла с корректным колбэком аллокатора, сверка прочитанных байтов с реальным файлом.
   - `VfsService_ReadFile_ZeroSize`: чтение пустого файла (0 байт), проверка статуса `ENGINE_VFS_OK` и возврата 0 байт.
   - `VfsService_ReadFile_NotFound`: чтение несуществующего файла, возврат `ENGINE_VFS_NOT_FOUND`.
   - `VfsService_ReadFile_AllocFailed`: колбэк аллокатора возвращает `nullptr`, проверка корректного возврата `ENGINE_VFS_ALLOC_FAILED` без краша и утечек.
   - `VfsService_Enumerate`: перечисление файлов в виртуальном каталоге с рекурсией и без, проверка досрочной остановки по возврату 0 из колбэка.
   - `VfsService_Find`: поиск по шаблону `*.hlsl` в подкаталоге.
   - `VfsService_GetMounts`: проверка перечисления активных точек монтирования `/Engine` и `/Game`.
   - `VfsService_NotSupportedStubs`: вызов `writeFile`, `createDir`, `subscribe`, `unsubscribe` возвращает `ENGINE_VFS_NOT_SUPPORTED`.

2. **Новый файл `Engine/tests/VfsProxyLifecycleTests.cpp` (~180 строк, входит в `EngineTests.vcxproj`):**
   - `TwoSessionsSequential`: последовательный запуск двух сессий в рамках одного процесса. При `Create` сессии 1 вызывается `Bind`, при `Destroy` — `Unbind` (прокси сбрасывается в `nullptr`). Затем сессия 2 делает `Bind` и `Unbind`. Проверяется отсутствие утечек и корректность указателя.
   - `FallbackLogging`: вызов `VfsRead::Active()` при не привязанном сервисе хоста перехватывает лог и проверяет наличие предупреждения `Host VFS service not bound. Falling back to LocalVfs`, при этом возвращается работоспособный `LocalVfs`.
   - `HotReloadSafety`: симуляция горячей перезагрузки (`ReloadActiveModule`): старая сессия вызывает `Destroy` -> `Unbind`, новая сессия создаётся и вызывает `Bind`. Проверяется отсутствие повисших указателей на старую таблицу сервисов.

---

### B8. Риски и открытые вопросы к архитектору

1. **Вопрос по брокеру сервисов:**
   - Согласован ли единый формат брокера `const void* (*getService)(void* user, uint32_t serviceId, uint32_t minVersion)` как универсальный механизм хоста для всех будущих подсистем (аудио, профилирование, ввод, документ сцены)?
2. **Вопрос по колбэку-аллокатору в `readFile`:**
   - В проекте используется сигнатура `void* (*EngineVfsAllocFn)(void* allocUser, uint64_t size)`. При этом вызывающая сторона владеет выделенным буфером (`std::vector<uint8_t>` или `std::string`), а хост лишь копирует данные. Подтверждено ли, что нулевой размер файла допустим и аллокатор может возвращать `nullptr` для `size == 0`?
3. **Риск несовместимости ABI при `ENGINE_MODULE_VERSION = 5`:**
   - Повышение версии отсекает все старые бинарные модули DLL. Так как внешним модулем является только `SceneEditor`, подтверждается ли синхронное повышение версии?
4. **Вопрос по кодировкам путей:**
   - Все C-ABI функции принимают и возвращают UTF-8 `const char*`. На хосте Windows выполняется конвертация в `std::filesystem::path` (`wchar_t`). В случае некорректных байт UTF-8 сервис возвращает `ENGINE_VFS_BAD_ARGUMENT`. Подтверждена ли данная семантика?


