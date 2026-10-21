# 038-PLAN: Скелетная анимация и импорт glTF/GLB моделей

## 1. Цели и контекст
- **Цель:** Реализовать поддержку загрузки и проигрывания скелетных анимаций (Skinned Mesh) формата GLB/glTF через Assimp и VFS, вычисление матричной палитры костей на CPU, скининг вершин в вершинном шейдере HLSL, компонент `SkeletalMeshComponent` и интеграцию демонстрационной модели в `RuntimeExample`.
- **Ветка:** `feature/skeletal-import` (создана от `feature/vfs-hardening`).
- **Ограничения:** 
  - Максимум 64 кости в палитре (превышение => явная ошибка загрузки).
  - Никаких C++ исключений через границы подсистем.
  - VFS загрузка через `VfsRead::Active().ReadBytes(...)` + `importer.ReadFileFromMemory(..., hint "glb")`.
  - Запрет `aiProcess_PreTransformVertices`.
  - Отсутствие динамических аллокаций в `Tick(dt)` и `Draw(...)` в кадре.
  - Лимит строк в новых/изменяемых файлах (~500 строк).

---

## 2. Чек-лист этапов реализации

- [ ] **Этап 1: Тестовая модель (CesiumMan.glb / RiggedSimple.glb)**
  - [ ] Загрузка модели из Khronos glTF-Sample-Models (raw.githubusercontent.com).
  - [ ] Создание/обновление `Content/Models/README.md` (лицензия, авторство, атрибуция).
  - [ ] Размещение в `Game/RuntimeExample/Content/Models/`.
  - [ ] Фиксация размера файла и коммит `chore(assets): add skeletal test model and license info`.

- [ ] **Этап 2: Структуры данных и загрузка без GPU**
  - [ ] Определение структур `SkinnedVertex` (pos, normal, uv, uint32/uint8 indices[4], float weights[4]).
  - [ ] Определение структуры `Skeleton` (имена костей, иерархия/parent, inverseBind-матрицы).
  - [ ] Определение структуры `AnimationClip` (каналы translation/rotation/scale с ключевыми кадрами, длительность, тики).
  - [ ] Определение структуры `SkeletalModel`.
  - [ ] Загрузка из памяти: `VfsRead::Active().ReadBytes` + `importer.ReadFileFromMemory(..., hint "glb")`.
  - [ ] Флаги Assimp: `Triangulate`, `GenNormals`, `LimitBoneWeights`, `JoinIdenticalVertices`, БЕЗ `PreTransformVertices`.
  - [ ] Конвертация системы координат под левостороннюю систему DirectX движка.
  - [ ] Проверка лимита 64 костей (явная ошибка, без молчаливой обрезки).
  - [ ] Обработка ошибок без проброса исключений.
  - [ ] Коммит `feat(render): skeletal data structures and memory loader`.

- [ ] **Этап 3: CPU анимация и юнит-тесты без GPU**
  - [ ] Функция вычисления локальных TRS и матричной палитры костей:
    - lerp для translation/scale.
    - slerp для rotation (кватернионы).
    - зацикливание времени по длительности клипа.
    - $M_{\text{final}} = M_{\text{world}} \times M_{\text{invBind}}$.
  - [ ] Модульные тесты `Engine/tests/SkeletalTests.cpp` (< 500 строк):
    - Число костей и вершин модели.
    - Длительность клипа > 0.
    - Веса каждой вершины суммируются в 1.0 (с допуском epsilon).
    - Индексы костей строго < числа костей.
    - Палитра в $t=0$ для bind-позы даёт единичные матрицы (с допуском).
    - Палитра в разные $t$ различается и непрерывна (нет скачков на стыке зацикливания).
    - Отклонение моделей с > 64 костями с явной ошибкой.
  - [ ] Регистрация тестов в `EngineTests`.
  - [ ] Коммит `test(render): cpu skeletal animation evaluation and unit tests`.
  - [ ] **СТОП после Этапа 3**: демонстрация результатов, чисел, диффа.

- [ ] **Этап 4: GPU ресурсы, HLSL шейдер и SkeletalMeshComponent**
  - [ ] `SkinnedMesh.hlsl`:
    - Constant buffer `cbuffer BoneBuffer : register(b1) { float4x4 gBones[64]; };`
    - Скининг позиций и нормалей через взвешенную сумму матриц костей.
  - [ ] Input layout для `SkinnedVertex` (`POSITION`, `NORMAL`, `TEXCOORD`, `BLENDINDICES`, `BLENDWEIGHT`).
  - [ ] Вершинный и индексный буферы Direct3D 11.
  - [ ] Константный буфер палитры костей с обновлением через `UpdateSubresource` или `Map(WRITE_DISCARD)` без покадровых аллокаций.
  - [ ] `SkeletalMeshComponent`:
    - `LoadFromVfs(path)`.
    - `Tick(dt)`.
    - `Draw(...)` по паттерну `FBXComponent` (forward без теней, без deferred).
  - [ ] Добавление файлов в `Engine.vcxproj`.
  - [ ] Коммит `feat(render): gpu skinned mesh rendering and skeletal component`.

- [ ] **Этап 5: Демо в RuntimeExample и валидация**
  - [ ] Интеграция скелетной модели в сцену `RuntimeExample` с автоматическим проигрыванием клипа.
  - [ ] Проверка `run-runtime.ps1` без регрессий.
  - [ ] Новая автопроверка (headless / `--frames 3`, проверка лога `"SkeletalModel loaded: bones=N clip=...s"`).
  - [ ] Скриншот через существующий механизм viewport-теста при наличии.
  - [ ] Коммит `feat(demo): skeletal mesh demo in runtime example`.
  - [ ] **СТОП после Этапа 5**: ожидание ручной проверки в окне от пользователя.

---

## 3. Решения
1. **Загрузка и парсинг:**
   - Чтение файла выполняется исключительно через виртуальную файловую систему `VfsRead::Active().ReadBytes(...)`, что гарантирует работу как в автономных играх (`LocalVfs`), так и в модулях DLL под хостом (`HostVfsProxy`).
   - Использование `importer.ReadFileFromMemory` с хинтом расширения `"glb"` исключает обращение Assimp к диску в обход VFS.
   - Исключения отключаются/перехватываются внутри обёртки загрузчика с возвратом кодов ошибок/`std::optional`/`std::expected` (без утечки через DLL ABI).
2. **Система координат:**
   - glTF использует правую систему координат (Y-up, forward -Z).
   - Движок использует левостороннюю систему DirectX (Y-up, forward +Z).
   - Выбор метода конвертации: флаг Assimp `aiProcess_ConvertToLeftHanded` либо явное матричное преобразование (обоснование выбора фиксируется на Этапе 2).
3. **Лимит костей и шейдерная модель:**
   - Строгий лимит 64 кости обусловлен размером `cbuffer` ($64 \times 64 = 4096$ байт, укладывается в один константный буфер DirectX 11). Превышение отсекается с явным сообщением об ошибке на этапе парсинга.
4. **Рендеринг:**
   - Первичный пайплайн — forward рендеринг без теней по аналогии с `FBXComponent`. Тени и deferred-проходы вынесены в Backlog.

---

## 4. Backlog
1. Поддержка теней (Shadow Map pass) для `SkeletalMeshComponent`.
2. Поддержка deferred-рендеринга для скелетных сеток.
3. Поддержка морфинга (blend shapes / morph targets).
4. Поддержка смешивания нескольких анимационных клипов (Animation Blending / Crossfade / State Machine).
5. LOD для скелетных сеток.
6. Выделение палитры костей свыше 64 костей через StructuredBuffer / TextureBuffer.

---

## 5. Не проверено в рантайме
- Поведение загрузчика на повреждённых glTF/GLB контейнерах (fuzzing).
- Производительность скининга на CPU при количестве вершин > 100 000.
- Совместимость с нестандартными форматами кватернионов/не-нормализованными весами в сторонних glTF моделях.
