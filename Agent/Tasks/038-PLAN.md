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

- [x] **Этап 1: Тестовая модель (CesiumMan.glb / RiggedSimple.glb) (завершён, коммит `90c68d3`)**
  - [x] Загрузка модели из Khronos glTF-Sample-Models (raw.githubusercontent.com).
  - [x] Создание/обновление `Content/Models/README.md` (лицензия CC-BY 4.0, авторство Cesium / Analytical Graphics, Inc.).
  - [x] Размещение в `Game/RuntimeExample/Content/Models/CesiumMan.glb` (490,956 байт).
  - [x] Фиксация размера файла и коммит `chore(assets): add CesiumMan test model and license info`.

- [x] **Этап 2: Структуры данных и загрузка без GPU (завершён, коммит `2ad6290`)**
  - [x] Определение структур `SkinnedVertex` (pos, normal, uv, uint32 BoneIndices[4], float BoneWeights[4], 64 байта).
  - [x] Определение структуры `Skeleton` (имена костей, иерархия/ParentIndex, InverseBindPose, LocalTransform).
  - [x] Определение структуры `AnimationClip` (каналы с ключевыми кадрами, длительность, тики).
  - [x] Определение структуры `SkeletalModel`.
  - [x] Загрузка из памяти: `VfsRead::Active().ReadBytes` + `importer.ReadFileFromMemory(..., hint "glb")`.
  - [x] Флаги Assimp: `Triangulate`, `GenNormals`, `LimitBoneWeights`, `JoinIdenticalVertices`, `ConvertToLeftHanded`, БЕЗ `PreTransformVertices`.
  - [x] Конвертация системы координат: `aiProcess_ConvertToLeftHanded` (инверсия Z, адаптация обхода треугольников).
  - [x] Проверка лимита 64 костей (явная ошибка при превышении).
  - [x] Обработка ошибок без проброса исключений через границы подсистем.
  - [x] Заглушка `SkeletalLoaderStub.cpp` при выключенном Assimp (возврат `"Assimp not enabled"`).
  - [x] Коммит `feat(render): skeletal data structures, CPU animation and GLB loader`.

- [x] **Этап 3: CPU анимация и юнит-тесты без GPU (завершён, коммит `8a4d8f3`)**
  - [x] Функция вычисления локальных TRS и матричной палитры костей:
    - lerp для translation/scale.
    - slerp для rotation (нормализованные кватернионы).
    - `XMMatrixDecompose` для извлечения дефолтных компонент при неполных каналах.
    - зацикливание времени `std::fmod`.
    - $M_{\text{final}} = M_{\text{invBind}} \times M_{\text{world}}$.
  - [x] Модульные тесты `Engine/tests/skeletal/SkeletalTests.cpp` (309 строк < 500 строк):
    - Число костей (3) и вершин (4) синтетической модели.
    - Длительность клипа > 0.
    - Веса каждой вершины суммируются в 1.0 (допуск $10^{-5}$).
    - Индексы костей строго < числа костей.
    - Палитра в $t=0$ для bind-позы даёт единичные матрицы $I_{4 \times 4}$ (допуск $10^{-4}$).
    - Палитра в разные $t$ различается и непрерывна (нет скачков при зацикливании, шов $< 0.01$).
    - Модель с > 64 костями отклоняется с ошибкой.
    - Заглушка без Assimp возвращает ошибку `"Assimp not enabled"`.
    - При отсутствии Assimp тест загрузки glb отдаёт `"SKIP: no Assimp"`.
  - [x] Набор тестов с собственным раннером `Engine/tests/skeletal/run.ps1` по аналогии с `edit-session`.
  - [x] Коммит `test(render): cpu skeletal animation evaluation and unit tests`.
  - [x] **СТОП после Этапа 3**: демонстрация результатов, чисел, диффа.

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
5. **Конвенция матриц и порядок умножения:**
   - **Конвенция:** DirectXMath использует строчные векторы (row-vector convention: $v' = v \cdot M$) и row-major размещение в C++.
   - **Иерархия костей на CPU:**
     $M_{\text{local}} = S \times R \times T$ (где $S$ — скейл, $R$ — поворот кватернионом, $T$ — трансляция).
     $M_{\text{global}} = M_{\text{local}} \times M_{\text{parentGlobal}}$.
   - **Формула палитры костей (Skin Matrix):**
     Вершина из bind-позы переводится в локальное пространство кости матрицей $M_{\text{invBind}}$, а затем трансформируется в мировое пространство скелета текущей позой кости $M_{\text{global}}$:
     $M_{\text{final}} = M_{\text{invBind}} \times M_{\text{global}}$.
   - **Транспонирование при загрузке в Constant Buffer:**
     HLSL константные буферы по умолчанию ожидают column-major упаковку матриц (`float4x4`). Поэтому при передаче в `cbuffer BoneBuffer : register(b1) { float4x4 gBones[64]; };` матрица $M_{\text{final}}$ транспонируется:
     `palette.Bones[i] = XMMatrixTranspose(M_final)`.
   - **Согласование с HLSL шейдером Этапа 4 (`mul`):**
     Благодаря транспонированию при загрузке, в HLSL инструкция `mul(float4(v.pos, 1.0f), gBones[i])` вычисляет ровно вектор-строку $v \cdot M_{\text{final}}$:
     $$v_{\text{skinned}} = \sum_{k=0}^{3} w_k \cdot \text{mul}\big(\text{float4}(v.\text{pos}, 1.0), gBones[v.\text{boneIndices}[k]]\big)$$
     $$n_{\text{skinned}} = \sum_{k=0}^{3} w_k \cdot \text{mul}\big(v.\text{normal}, (\text{float3x3})gBones[v.\text{boneIndices}[k]]\big)$$
     После чего мировая позиция вычисляется стандартно для движка:
     $v_{\text{world}} = \text{mul}(v_{\text{skinned}}, worldMatrix)$.
6. **Ориентация меша и узлы-предки сцены (`MeshToModel`):**
   - Для моделей glTF, где геометрия скин-меша и корневая кость находятся под нескелетными узлами сцены (например, `node 0: Z_UP -> node 1: Armature` в `CesiumMan.glb`), геометрия вершин и `invBind` не переписываются.
   - В структуру `SkeletalModel` включена матрица `MeshToModel` ($I_4$ по умолчанию), накапливающая трансформацию от узла скин-меша до `scene->mRootNode` в row-vector конвенции:
     $$M_{\text{MeshToModel}} = M_{\text{mesh\_local}} \times M_{\text{parent1\_local}} \times \dots \times M_{\text{root\_local}}$$
   - Финальная формула палитры костей в CPU-пайплайне:
     $$M_{\text{final}} = M_{\text{invBind}} \times M_{\text{global}} \times M_{\text{MeshToModel}}$$
   - В rest-позе $M_{\text{invBind}} \times M_{\text{global}} = I$, поэтому палитра костей в rest-позе равна $M_{\text{MeshToModel}}$ (для синтетики остаётся $I$).
   - Направление ориентации: анализ смещения носков в анимации шага показал, что шаги происходят вдоль оси $Z$ (смещение $\Delta Z \approx \pm 0.49$ м при $\Delta X \approx 0.01$ м), то есть лицо и шаг CesiumMan после конвертации узлов смотрят в $+Z$.
   - Дополнительный поворот ориентации модели на сцене на произвольный угол настраивается через параметр компонента `ModelYawDegrees` (применяется как поворот вокруг $Y$ перед world-матрицей компонента).

---

## 4. Backlog
1. Поддержка теней (Shadow Map pass) для `SkeletalMeshComponent`.
2. Поддержка deferred-рендеринга для скелетных сеток.
3. Скелетные LOD (уровни детализации сетки и скелета).
4. Обязательность Assimp: переход от опционального макроса `ENGINE_ENABLE_ASSIMP` к обязательной сборке с Assimp.
5. Кэширование rest-TRS в Bone при загрузке (устранение `XMMatrixDecompose` из кадрового цикла; базово сделано на Шаге 0, расширить валидацию).
6. Поддержка морфинга (blend shapes / morph targets).
7. Поддержка смешивания нескольких анимационных клипов (Animation Blending / Crossfade / State Machine).
8. Выделение палитры костей свыше 64 костей через StructuredBuffer / TextureBuffer.

---

## 5. Не проверено в рантайме
- Seam 0.06: не проверено визуально в рантайме (разрыв матрицы на шве зацикливания CesiumMan `diff = 0.0488` на кости `Skeleton_arm_joint_R__3_`).
- Поведение загрузчика на повреждённых glTF/GLB контейнерах (fuzzing).
- Производительность скининга на CPU при количестве вершин > 100 000.
- Совместимость с нестандартными форматами кватернионов/не-нормализованными весами в сторонних glTF моделях.
