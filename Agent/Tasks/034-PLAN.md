# 034-PLAN: Чтение текстур и шейдеров через VFS

## 1. Статус выполнения
- [x] **Шаг 1. Текстуры (коммит `d2f1ca1 refactor: load textures via VFS`)**
  - Замена вызова `stbi_load` на чтение через `VirtualFileSystem::ReadBytes` и парсинг через `stbi_load_from_memory`.
  - Реализована `nullptr`-проверка буфера VFS до передачи в `stb_image`.
  - Логирование пути при ошибке чтения VFS с сохранением стандартного контракта возврата `false`.
  - Кэш текстур (`textureCache`) и форматы результирующего `ID3D11ShaderResourceView` не изменялись.
- [x] **Шаг 2. Шейдеры (коммит `898890c refactor: compile shaders via VFS and custom ID3DInclude`)**
  - Замена вызова `D3DCompileFromFile` на `D3DCompile` из буфера, прочитанного через VFS.
  - Реализован класс `VfsShaderInclude : public ID3DInclude`, перенаправляющий `Open`/`Close` в VFS.
  - Буферы заголовочных файлов `#include` хранятся в RAII-контейнере `std::vector<std::vector<uint8_t>>` на протяжении всей компиляции и автоматически освобождаются при выходе из скоупа (без raw `new`/`malloc`).
  - Имя/путь файла передаётся в `D3DCompile` как `pSourceName` (UTF-8) для корректной локализации ошибок компилятора HLSL.
- [x] **Доработка 034b (коммит `74fa9c4 fix(shader-compiler): standardize include search and log VFS errors`)**
  - Исключена тройная цепочка поиска: резолв сведён к строгой семантике `D3D_COMPILE_STANDARD_FILE_INCLUDE`.
  - Обработка исключений `catch(const std::exception& ex)` с информативным логированием пути и причины ошибки.
- [x] **Доработка 034c: Единый VFS на бинарник и изоляция интерфейсов**
  - **Шаг 1 (коммит `9b4394e`):** `VfsRead.h` преобразован в заголовок объявлений; реализация перенесена в `VfsRead.cpp` с `static VirtualFileSystem s_vfs` и `std::call_once`, без `inline static`.
  - **Шаг 2 (коммит `35af925`):** `BaseEngine` использует канонический экземпляр (поле `VirtualFileSystem& Vfs` инициализируется ссылкой на `VfsRead::GetVFS()`). При `Play` явный Mount `/Game` заменяет эвристический без дубликатов (согласно логике `VirtualFileSystem::Mount`).
  - **Шаг 3 (коммит `974d2b1`):** В `VfsRead::ReadBytes` исключения перехватываются с логированием пути и причины. Динамическое монтирование каталогов `/_ext_*` логируется с указанием пути и причины.
  - **Шаг 4 (коммит `4e47c36`):** `VfsShaderInclude` и `CompileFromFile` вынесены в `Engine/Source/Render/ShaderCompiler.cpp`. Публичный заголовок `ShaderCompiler.h` очищен от `<Windows.h>` и включений из `Source/`.
  - **Шаг 5 (коммит `df6ce7b`):** Проверка результата обоих вызовов `WideCharToMultiByte`; при ошибке выдаётся диагностический лог и возвращается `E_FAIL` вместо усечения wchar в char.
  - **Шаг 6 (коммит `5f86ab7`):** В `EngineTests.cpp` добавлены тесты вложенной цепочки `#include` (`a.hlsl -> sub/b.hlsli -> ../c.hlsli`) и проверка ошибки/лога при отсутствующем файле заголовка.
- [x] **Мерж main (коммит `bb5203a merge: main into refactor/vfs-textures-shaders`)**
  - Актуальный `main` влит в ветку без конфликтов; сохранена полная работоспособность всех подсистем и тестов.
- [x] **034f. Тесты жизненного цикла VFS (коммит `7a37821`)**
  - Создан тестовый модуль `Engine/tests/VfsLifecycleTests.cpp`. Доказана гипотеза сброса маунтов при `Stop()`.
- [x] **034g. Селективный откат маунтов в BaseEngine::Stop()**
  - `BaseEngine` сохраняет и откатывает только собственные маунты; `Vfs.UnmountAll()` удалён.
  - Тесты B и C стали зелёными без ослабления проверок; добавлены тесты инвариантов сохранения системных маунтов и отката проектного `/Game`.

---

## 2. Принятые архитектурные решения

### 2.1. Семантика поиска `#include` в `VfsShaderInclude`
Логика включений строго соответствует семантике `D3D_COMPILE_STANDARD_FILE_INCLUDE`:
1. **Локальный поиск (`parentDir`):** путь ищется относительно каталога того файла, внутри которого встретилась директива `#include` (`NormalizeVirtualPath(parentDir + "/" + pFileName)`).
2. **Поиск относительно корня (`baseDir`):** если файл не найден и `parentDir != baseDir`, выполняется поиск относительно каталога корневого шейдера (`NormalizeVirtualPath(baseDir + "/" + pFileName)`).
3. **Отказ с логированием:** если файл не найден ни в одном из указанных каталогов, компилятор логирует ошибку в `std::cout` с указанием проверенных путей и возвращает `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)`.
При возникновении исключений чтения байтов (`vfs.ReadBytes`) перехватывается `const std::exception& ex` с выводом сообщения в лог (путь + `ex.what()`).

### 2.2. Единый экземпляр VFS на уровне бинарника
- `VfsRead.cpp` владеет экземпляром `s_vfs` внутри `Engine.lib`.
- `BaseEngine` хранит ссылку `VirtualFileSystem& Vfs` на канонический экземпляр, возвращаемый `VfsRead::GetVFS()`.
- При вызове `BaseEngine::Play(options)` монтирование `/Game` перезаписывает эвристический путь `options.contentRoot`, так как `VirtualFileSystem::Mount` обновляет существующий `physicalPath` при совпадении `virtualRoot`, не создавая дубликатов.

### 2.3. Результаты тестирования жизненного цикла VFS (034f)
Тесты жизненного цикла VFS в `EngineTests` подтвердили гипотезу:
1. **Сценарий A (зелёный):** До запуска `BaseEngine` глобальный VFS инициализирован через `std::call_once`; `VfsRead::ReadBytes("/Engine/Shaders/RotatedFigure.hlsl")` читает 5479 байт.
2. **Сценарий B (красный, гипотеза подтверждена):** После вызова `BaseEngine::Stop()` маунты у `s_vfs` полностью сброшены (`VfsRead::GetVFS().GetMounts().size() == 0`). Последующий `VfsRead::ReadBytes` возвращает 0 байт с ошибкой `Virtual file not found (unmounted root)`.
3. **Сценарий C (красный, гипотеза подтверждена):** После `Stop()` вызов `ShaderCompiler::CompileFromFile` завершается ошибкой `0x80070002` (`ERROR_FILE_NOT_FOUND`), так как компилятор не может прочитать файл шейдера через размонтированный VFS. (Примечание: в 034f в тесте C изначально использовалась точка входа `"VS"`, отсутствующая в `RotatedFigure.hlsl`; в 034g исправлено на валидную точку входа `"VSMain"`).
4. **Сценарий D (зелёный):** Второй экземпляр `BaseEngine` стартует успешно (`Run` возвращает 0), так как `Play()` заново вызывает `Vfs.Mount("/Engine", ...)` и маскирует проблему на время работы сессии. При возврате из `Run()` снова вызывается `Stop()`, и маунты вновь сбрасываются.
5. **Сценарий E (зелёный на уровне VFS):** Проверено, что `VirtualFileSystem::Mount` для существующего виртуального корня обновляет путь на месте без дублирования записей.

### 2.5. Селективный откат маунтов BaseEngine (034g)
- `BaseEngine` хранит стек собственных маунтов: `struct AppliedMount { std::string virtualRoot; std::optional<std::filesystem::path> previousPath; }; std::vector<AppliedMount> appliedMounts;`.
- При вызове `ApplyMount(virtualRoot, physicalPath)` текущий путь точки монтирования (если был) запоминается в `previousPath`. Запись добавляется в `appliedMounts` до вызова `Vfs.Mount` (чтобы исключение при монтировании не теряло откат).
- В `BaseEngine::Stop() noexcept` вместо разрушительного `Vfs.UnmountAll()` выполняется откат в обратном порядке: если `previousPath` был задан — восстанавливается прежний путь через `Vfs.Mount(virtualRoot, *previousPath)`, иначе виртуальный корень удаляется через `Vfs.Unmount(virtualRoot)`.
- Все операции отката защищены `try/catch` с выводом ошибок в stderr, что исключает `std::terminate`. Метод `Stop()` идемпотентен (`appliedMounts.clear()`).
- В результате базовые маунты процесса (`/Engine`, эвристический `/Game`) остаются нетронутыми после остановки движка, а специфичный проектный `contentRoot` не утекает в глобальный VFS.

---

## 3. Backlog (найденные внешние дефекты и архитектурные ограничения)
1. **Два экземпляра VFS между бинарниками (`Editor.dll` / `Game.dll` и хостом)**:
   - В текущей архитектуре хост `EngineRuntime.exe` и динамические библиотеки (`Editor.dll`, `Game.dll`) компонуются независимо. Из-за отсутствия передачи указателя на VFS через `EngineModuleHost` (контракт ABI v3 запрещает передачу объектов с CRT-состоянием) DLL создают свой локальный экземпляр `s_vfs`.
   - Решение: добавить интерфейс VFS в `EngineModuleHost` и поднять версию до `ENGINE_MODULE_VERSION = 4`.
2. **Обход маунтов через `/_ext_*`**:
   - При запросе физического пути, находящегося вне смонтированных корней (`/Engine`, `/Game`), `VfsRead::ToVirtualPath` динамически монтирует родительский каталог как `/_ext_<hash>`. Это обеспечивает прозрачную обратную совместимость, но нарушает инкапсуляцию VFS. Необходим строгий аудит мест вызовов и перевод всех путей на канонические виртуальные пути `/Engine/...` и `/Game/...`.
3. **Katamari.vcxproj MSB3107**:
   - При сборке с `-EnableAssimp` проект `Game/Katamari/Katamari.vcxproj` выдаёт ошибку `MSB3107`:
     `error MSB3107: Указанные метаданные ссылки проекта для ссылки "..\..\Engine\Engine.vcxproj" отсутствуют или имеют недопустимое значение: Project [Game\Katamari\Katamari.vcxproj]`
   - Дефект воспроизводится на чистом `main` (проверено через `git stash / checkout main`).
4. **D3D11 GPU Timer Query Latency и безусловный сброс слота (`Game.cpp:2860`)**:
   - В `Game.cpp:2852-2860` опрос таймеров `Context->GetData(..., D3D11_ASYNC_GETDATA_DONOTFLUSH)` при неготовности запроса возвращает `S_FALSE`. Строка 2860 безусловно сбрасывает `GpuTimerIssued[slot] = false;` и немедленно начинает новый запрос `Context->Begin()`, затирая незавершённый запрос из кольцевого буфера (`latency=3`). На встроенном GPU (AMD Radeon 610M) из-за задержки драйвера и отсутствия `Context->Flush()` запросы перезаписываются до готовности, что приводит к потере сэмплов (`gpu_ready = 0`).
   - В старом коде `BattleGame` (`MeasuredFrames` / `BuildReport`) поле `LastGpuFrameMs` суммировалось в `GpuMsSum` каждый кадр независимо от возврата `GetData`, что приводило к повторному суммированию старых значений.
5. **Утечка `/_ext_*` между тестами (`VfsRead.cpp:167-170`)**:
   - При запросе путей вне стандартных точек монтирования `VfsRead::ToVirtualPath` динамически регистрирует точку `/_ext_<hash>` на глобальном экземпляре `s_vfs`. Точки `/_ext_*` не очищаются при остановке движка и накапливаются в VFS на протяжении всей жизни процесса.
6. **`std::getenv` в `viewportCamera` (`Base.cpp:515`)**:
   - Проблема в вызове `std::getenv("ENGINE_VIEWPORT_INPUT_TRACE")` в горячем пути обработки ввода камеры вьюпорта на каждом событии/кадре (не в предупреждении C4996). Требуется кэширование значения при инициализации.
7. **`stderr` против `stdout` в `VfsRead`**:
   - В `VfsRead.cpp:168, 193` диагностика и ошибки выводятся через `std::fprintf(stderr, ...)`. В то же время остальные подсистемы движка (`ShaderCompiler.cpp`, `Base.cpp`) пишут информацию в `stdout` (`std::cout`, `std::puts`). Требуется унификация каналов логирования.
8. **Поведение P99 на dGPU**:
   - Рост $+5.9\%$ (10 прогонов) и $+10.9\%$ (20 прогонов) лежит в пределах шума $1\sigma$ ($sd \approx 0.36$–$0.37$ ms для 6 худших кадров из 600). Наблюдать в дальнейших замерах.

---

## 4. Замер производительности A/B (Battle Release, --frames 300)

### Методика и результаты
- Выполнялся поочерёдный запуск 5 прогонов `main` и 5 прогонов `branch` (`refactor/vfs-textures-shaders`).
- Прогоны выполнялись на конфигурации Release, флаг `--frames 300`.
- **Уточнение по видеокарте:** Замеры 033 и 034 фактически шли на встроенном GPU **AMD Radeon(TM) 610M**, а не на **RTX 5060 Laptop**. В `Game.cpp:517` D3D11 создавался с `pAdapter = nullptr`, что на ноутбуке с гибридной графикой направляло процесс на iGPU (поскольку в `UserGpuPreferences` не было записи). В отчёте 033 RTX 5060 был указан по паспорту системы, а не по рантайм-выводу DXGI.
- **Уточнение по прогреву:** В формулировке 034 заявлялось «150 кадров прогрева», однако фактически в коде `BattleGame.cpp:17` константа прогрева равнялась `WarmupFrames = 10`. При `--frames 300` измерялось ровно 290 кадров (отсюда $M=290$). В задаче 035 введён полноценный прогрев по умолчанию на 150 кадров.
- **Main (CPU frame_ms):** вариационный ряд `[11.30, 11.52, 13.46, 13.50, 15.75]`, медиана **13.46 ms**.
- **Branch (CPU frame_ms):** вариационный ряд `[10.80, 11.35, 12.71, 13.67, 14.83]`, медиана **12.71 ms**.

> [!NOTE]
> **Вывод по измерению:**
> Текущий замер обладает высокой межпрогонной дисперсией (разброс от 10.8 до 15.7 ms обусловлен шедулингом ОС, фоновыми процессами и прогревом D3D runtime) и **не различает изменения в пределах 3%**.
> Корректный A/B замер с достаточной статистической мощностью переносится на специализированный инструмент из задачи **035**.

---

## 5. Результаты верификации
- `Engine\build.ps1 -Configuration Debug`: **0 ошибок, 0 новых warnings**.
- `Engine\build.ps1 -Configuration Release`: **0 ошибок, 0 новых warnings**.
- `Engine\tests\run.ps1`: **10/10 PASS**.
- `Engine\tests\run-runtime.ps1`: **22/22 PASS**.
- `Game\tests\run-games.ps1 -Configuration Release`: **ALL PASS**.
- `Game\tests\run-games.ps1 -Configuration Debug`: **ALL PASS**.
- `grep "imgui|Qt|QFile"`: **0 совпадений в коде** (по `Engine/Source/Render`, `Engine/Source/Resources` и `Engine/Include`).

### Объёмы файлов (`wc -l` <= ~500)
- `Engine/Source/Render/TextureLoader.cpp`: 101 строка
- `Engine/Include/Engine/Render/ShaderCompiler.h`: 32 строки
- `Engine/Source/Render/ShaderCompiler.cpp`: 206 строк
- `Engine/Source/Resources/VfsRead.h`: 15 строк
- `Engine/Source/Resources/VfsRead.cpp`: 204 строки
- `Engine/Include/Engine/Base/Base.h`: 57 строк
- `Engine/Source/Base/Base.cpp`: 841 строка (модифицировано 6 строк в части VFS)
- `Engine/tests/EngineTests.cpp`: 573 строки

---

## 6. Мерж main (задача 034e)
- **Хеш источника (main / origin/main):** `b864afcebe0cfb4ba19d46b023f4ed4e3df796c4`
- **Список пришедших коммитов:**
  - `b864afc Startup editor without runtime game` (автор: Mysar)
- **Разрешённые конфликты:**
  - Автоматическое 3-way слияние без конфликтов.
  - В `Engine/Engine.vcxproj` объединены: `<ClInclude Include="Include\Engine\Runtime\ViewportSession.h" />` (из `main`) и `<ClCompile>` записи для `ShaderCompiler.cpp` и `VfsRead.cpp` (из ветки).
  - В `Engine/Include/Engine/Base/Base.h` сохранены `class ViewportSession;`, `std::unique_ptr<ViewportSession> viewportSession;` (из `main`) и `VirtualFileSystem& Vfs;` (из ветки).
  - В `Engine/Source/Base/Base.cpp` сохранён контроль жизненного цикла `viewportSession` (из `main`) и инициализация `Vfs(VfsRead::GetVFS())`, `#include "../Resources/VfsRead.h"`, `Vfs.UnmountAll()` (из ветки).
  - В `Engine/tests/EngineTests.cpp` сохранены тесты `game.SetSimulationEnabled` в `ParticleComponentModes` (из `main`) и тест цепочки `#include` VFS в `VirtualFileSystemTests` (из ветки).
- **Результаты проверок после мержа:**
  - Сборка Debug: 0 ошибок, 0 новых warnings.
  - Сборка Release: 0 ошибок, 0 новых warnings.
  - `Engine\tests\run.ps1`: **10/10 PASS**.
  - `Engine\tests\run-runtime.ps1`: **22/22 PASS**.
  - `Game\tests\run-games.ps1 -Configuration Debug`: **ALL PASS**.
  - `Game\tests\run-games.ps1 -Configuration Release`: **ALL PASS**.
  - Проверка grep `imgui|Qt|QFile`: **0 совпадений**.
