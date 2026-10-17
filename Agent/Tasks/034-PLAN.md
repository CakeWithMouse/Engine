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
4. **D3D11 GPU Timer Query Latency**:
   - В `Game.cpp` сбор статистики GPU через `D3D11_ASYNC_GETDATA_DONOTFLUSH` в отдельных кадрах возвращает `S_FALSE`, из-за чего в `gpu_ms` может попадать 0.00 ms при ультрабыстром прогоне `--frames 300` без VSync.

---

## 4. Замер производительности A/B (Battle Release, --frames 300)

### Методика и результаты
- Выполнялся поочерёдный запуск 5 прогонов `main` и 5 прогонов `branch` (`refactor/vfs-textures-shaders`).
- Прогоны выполнялись на конфигурации Release, флаг `--frames 300`.
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
- `Engine/Source/Render/ShaderCompiler.cpp`: 207 строк
- `Engine/Source/Resources/VfsRead.h`: 16 строк
- `Engine/Source/Resources/VfsRead.cpp`: 205 строк
- `Engine/Include/Engine/Base/Base.h`: 56 строк
- `Engine/Source/Base/Base.cpp`: 823 строки (модифицировано 6 строк в части VFS)
- `Engine/tests/EngineTests.cpp`: 568 строк
