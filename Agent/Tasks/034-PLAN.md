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

---

## 2. Принятые архитектурные решения

### 2.1. Семантика поиска `#include` в `VfsShaderInclude`
В первой итерации для поиска включаемых файлов применялась тройная цепочка (каталог родительского файла -> `/Engine/Shaders` -> физический резолв). По требованию 034b логика приведена в строгое соответствие стандартной семантике `D3D_COMPILE_STANDARD_FILE_INCLUDE`:
1. **Локальный поиск (`parentDir`):** путь ищется относительно каталога того файла, внутри которого встретилась директива `#include` (`NormalizeVirtualPath(parentDir + "/" + pFileName)`).
2. **Поиск относительно корня (`baseDir`):** если файл не найден и `parentDir != baseDir`, выполняется поиск относительно каталога корневого шейдера (`NormalizeVirtualPath(baseDir + "/" + pFileName)`).
3. **Отказ с логированием:** если файл не найден ни в одном из указанных каталогов, компилятор логирует ошибку с указанием проверенных путей и возвращает `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)`.
При возникновении исключений чтения байтов (`vfs.ReadBytes`) перехватывается `const std::exception& ex` с выводом сообщения в лог (путь + `ex.what()`).

### 2.2. Архитектура модуля `VfsRead.h` и жизненный цикл VFS
- Заголовочный файл `Engine/Source/Resources/VfsRead.h` создан `inline` для интеграции без изменения структуры проектов (`Engine.vcxproj` запрещён к модификации).
- `VfsRead::GetVFS()` инициализирует статический экземпляр `VirtualFileSystem` с монтированием точек `/Engine` и `/Game`.
- **Вопрос об экземпляре VFS и BaseEngine:**
  - `BaseEngine` содержит свой собственный экземпляр-член `VirtualFileSystem Vfs` (`Engine/Include/Engine/Base/Base.h:19`), монтируемый в `BaseEngine::Play` (`Base.cpp:469-473`).
  - В `VfsRead.h:36` живёт отдельный экземпляр `static VirtualFileSystem s_vfs`.
  - В `Editor.dll` и `Game.dll` при обращении к inline-функции с локальным static в заголовке создаётся отдельная статическая копия в сегменте данных соответствующей DLL.
  - **Для перехода на единый источник VFS требуется изменение API** (детали описаны в разделе 3).

---

## 3. Анализ необходимости правки API (пункт 034b.b)

Для перехода на единый источник `VirtualFileSystem` между `BaseEngine`, хостом `EngineRuntime.exe`, DLL модулями (`Editor.dll`, `Game.dll`) и легаси играми (`Battle.exe`, `Pong.exe` и др.):

1. **Файл:** `Engine/Include/Engine/Base/Base.h` (строки 19, 45-46)
   - **Что требуется:** заменить приватное поле `VirtualFileSystem Vfs` на обращение к глобальному / синглтон-провайдеру `VirtualFileSystem`, либо экспортировать синглтон из `Engine.lib`.
   - **Зачем:** чтобы `BaseEngine::GetVFS()` возвращал тот же объект, через который работают `TextureLoader` и `ShaderCompiler`.
2. **Файл:** `Engine/Source/Base/Base.cpp` (строки 427, 469-473)
   - **Что требуется:** вызывать монтирование точек `/Engine` и `/Game` на каноническом экземпляре VFS.
   - **Зачем:** чтобы точки монтирования проекта (`options.contentRoot`), переданные через `Project.json`, автоматически становились доступны всем вызовам чтения текстур и шейдеров.
3. **Файл:** `Engine/Include/Engine/Runtime/ModuleApi.h` (строки 13-30, структура `EngineModuleHost`)
   - **Что требуется:** добавить поле `void* vfs` или указатели на функции чтения VFS в `EngineModuleHost`, обновив версию `ENGINE_MODULE_VERSION` (с 3 на 4).
   - **Зачем:** ABI v3 строго запрещает передачу STL-типов и кросс-DLL зависимостей между хостом и DLL (`Editor.dll`, `Game.dll`). Без передачи контекста VFS через хост каждая DLL физически изолирована от хоста.
4. **Легаси-игры (`Battle`, `Pong`, `SunGame`, `BasicExamples`):**
   - Не используют `BaseEngine` вообще (используют старый запуск через `Game::StartGame`). Для них канонический VFS в `VfsRead` является единственным источником VFS.

> Согласно регламенту задачи 034b, правки сторонних файлов не производились до прямого согласования архитектором.

---

## 4. Backlog (найденные внешние дефекты, не относящиеся к текущему рефакторингу)
1. **Katamari.vcxproj MSB3107**:
   - При сборке с `-EnableAssimp` проект `Game/Katamari/Katamari.vcxproj` выдаёт ошибку `MSB3107`:
     `error MSB3107: Указанные метаданные ссылки проекта для ссылки "..\..\Engine\Engine.vcxproj" отсутствуют или имеют недопустимое значение: Project [Game\Katamari\Katamari.vcxproj]`
   - Дефект воспроизводится на чистом `main` (проверено через `git stash / checkout main`).
2. **D3D11 GPU Timer Query Latency**:
   - В `Game.cpp` сбор статистики GPU через `D3D11_ASYNC_GETDATA_DONOTFLUSH` в отдельных кадрах возвращает `S_FALSE`, из-за чего в `gpu_ms` может попадать 0.00 ms при ультрабыстром прогоне `--frames 300` без VSync.
3. **MultiByteToWideChar CP_UTF8 Fallback**:
   - В `Game.cpp` пути передаются через `std::wstring`. В `ShaderCompiler.h:46-60` выполняется конвертация `WideCharToMultiByte(CP_UTF8, ...)`. Рекомендуется в будущем унифицировать интерфейс на `std::string` (UTF-8).

---

## 5. Замер производительности A/B (Battle Release, --frames 300)

### Методика
- Поочерёдный запуск 5 прогонов `main` и 5 прогонов `branch` (`refactor/vfs-textures-shaders`).
- Прогоны выполнялись на идентичной конфигурации Release, флаг `--frames 300`, отбрасываются кадры разгона (первые 150 кадров, в рантайме Battle измерение ведётся после warmup).
- **Формула `frame_ms`:**
  $$\text{frame\_ms} = \frac{\sum_{i=1}^{M} (t_i - t_{i-1})}{M}$$
  где $M = \text{MeasuredFrames}$, $t_i$ — таймстемп `std::chrono::steady_clock` (в миллисекундах) на кадре $i$.
  $$\text{FPS} = \frac{1000.0}{\text{frame\_ms}}, \quad \text{gpu\_ms} = \frac{\sum_{i=1}^{M} \text{LastGpuFrameMs}_i}{M}$$

### Результаты прогонов

| Прогон | Сторона | `frame_ms` (CPU) | FPS | `gpu_ms` (GPU) |
|---|---|---|---|---|
| Run 1 | Main | 15.75 ms | 63.5 | 15.19 ms |
| Run 1 | Branch | 14.83 ms | 67.5 | 0.00 ms |
| Run 2 | Main | 13.50 ms | 74.1 | 0.00 ms |
| Run 2 | Branch | 13.67 ms | 73.1 | 0.00 ms |
| Run 3 | Main | 13.46 ms | 74.3 | 0.00 ms |
| Run 3 | Branch | 12.71 ms | 78.7 | 0.00 ms |
| Run 4 | Main | 11.52 ms | 86.8 | 0.00 ms |
| Run 4 | Branch | 11.35 ms | 88.1 | 0.00 ms |
| Run 5 | Main | 11.30 ms | 88.5 | 0.00 ms |
| Run 5 | Branch | 10.80 ms | 92.6 | 9.73 ms |

### Сводные метрики (Медиана и p95)
- **Main (CPU frame_ms):**
  - Вариационный ряд: `[11.30, 11.52, 13.46, 13.50, 15.75]`
  - **Медиана (p50):** **13.46 ms** (74.3 FPS)
  - **p95:** **15.75 ms**
- **Branch (CPU frame_ms):**
  - Вариационный ряд: `[10.80, 11.35, 12.71, 13.67, 14.83]`
  - **Медиана (p50):** **12.71 ms** (78.7 FPS)
  - **p95:** **14.83 ms**
- **Регресс медианы:** **-5.6%** (ветка работает быстрее `main`, **регресса нет, порог 3% соблюдён**).

---

## 6. Результаты верификации
- `Engine\build.ps1 -Configuration Debug`: **0 ошибок, 0 новых warnings**.
- `Engine\build.ps1 -Configuration Release`: **0 ошибок, 0 новых warnings**.
- `Engine\tests\run.ps1`: **10/10 PASS**.
- `Engine\tests\run-runtime.ps1`: **22/22 PASS**.
- `Game\tests\run-games.ps1 -Configuration Release`: **ALL PASS**.
- `Game\tests\run-games.ps1 -Configuration Debug`: **ALL PASS**.
- `grep "imgui|Qt|QFile"`: **0 совпадений в коде** (по `Engine/Source/Render`, `Engine/Source/Resources` и `Engine/Include`).

### Объёмы файлов (`wc -l` <= ~500)
- `Engine/Source/Render/TextureLoader.cpp`: 101 строка
- `Engine/Include/Engine/Render/ShaderCompiler.h`: 205 строк
- `Engine/Source/Resources/VfsRead.h`: 200 строк
- `Engine/Source/Resources/VfsRead.cpp`: 3 строки
