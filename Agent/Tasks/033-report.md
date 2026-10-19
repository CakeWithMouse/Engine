# 033. Отчёт по фазе 0: разведка VFS/анимации и базовый замер Battle

**Режим:** ТОЛЬКО ЧТЕНИЕ (правки в `Engine/`, `Editor/`, `Game/`, `build.ps1` отсутствуют).  
**Ветка:** `033-recon`  
**Дата проведения замеров:** 2026-10-17  

---

## 1. VirtualFileSystem.h / .cpp

### 1.1. Публичный API
* **Файл:** [`Engine/Include/Engine/Resources/VirtualFileSystem.h:55-123`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Resources/VirtualFileSystem.h#L55-L123)
* **Методы:**
  * Управление точками монтирования: `Mount(virtualRoot, physicalPath)`, `Unmount(virtualRoot)`, `UnmountAll()`, `GetMounts()`
  * Разрешение путей и проверка: `Resolve(virtualPath)` $\to$ `std::optional<std::filesystem::path>`, `Exists(virtualPath)` $\to$ `bool`
  * Чтение данных: `ReadBytes(virtualPath)` $\to$ `std::vector<uint8_t>`, `ReadText(virtualPath)` $\to$ `std::string` (автоматически удаляет UTF-8 BOM при наличии)
  * Нормализация: `static std::string NormalizeVirtualPath(std::string_view path)`
  * Определение типа ассета: `GetAssetType(virtualPath)`, `DeduceAssetType(pathOrExtension)`
  * Метаданные (Stat): `GetFileInfo(virtualPath)` $\to$ `std::optional<VirtualFileInfo>` (возвращает `virtualPath`, `physicalPath`, `type`, `sizeBytes`, `extension`)
  * Список файлов (List): `EnumerateFiles(virtualDir, recursive)` $\to$ `std::vector<VirtualFileInfo>`, `FindFiles(...)` с фильтрацией по маске и типу
  * Манипуляция файлами/папками: `ImportFile(...)`, `CreateDir(...)`, `DeleteFile(...)`, `DeleteDir(...)`, `Move(...)`, `Copy(...)`, `WriteBytes(...)`, `WriteText(...)`

### 1.2. Точки монтирования
* **Структура и хранение:** [`Engine/Include/Engine/Resources/VirtualFileSystem.h:39-43, 129`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Resources/VirtualFileSystem.h#L39-L43)
  ```cpp
  struct MountPoint {
      std::string virtualRoot;            // E.g., "/Engine", "/Game"
      std::filesystem::path physicalPath; // Absolute normalized filesystem path
  };
  std::vector<MountPoint> mounts;
  ```
* **Сортировка:** при вызове `Mount` точки сортируются по длине виртуального корня по убыванию ([`VirtualFileSystem.cpp:234-237`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L234-L237)) для обеспечения longest prefix match.
* **Защищенные корни:** пути `"/"` и `"/Engine"` защищены от удаления методом `IsProtectedRoot` ([`VirtualFileSystem.cpp:262-276`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L262-L276)).
* **Где монтируются в коде ядра:** в [`Engine/Source/Base/Base.cpp:469-473`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Base/Base.cpp#L469-L473):
  ```cpp
  Vfs.Mount("/Engine", engineContent);
  if (!options.contentRoot.empty())
  {
      Vfs.Mount("/Game", options.contentRoot);
  }
  ```

### 1.3. Кодировка путей
* Виртуальные пути передаются через `std::string_view` / `std::string` (UTF-8, разделитель `/`).
* Физические пути хранятся как `std::filesystem::path`. При сопоставлении подпутей используется `std::filesystem::u8path(subPath)` ([`VirtualFileSystem.cpp:286, 299, 446`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L286)), а при выводе — `.generic_u8string()`.

### 1.4. Потокобезопасность
* **Да, потокобезопасна.** Используется `mutable std::shared_mutex mutex;` ([`VirtualFileSystem.h:128`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Resources/VirtualFileSystem.h#L128)).
* Читающие методы (`Resolve`, `GetMounts`, `EnumerateFiles`, `FindFiles`) захватывают `std::shared_lock<std::shared_mutex> lock(mutex);` (например, [`VirtualFileSystem.cpp:309, 378, 415`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L309)).
* Модифицирующие методы (`Mount`, `Unmount`, `CreateDir`, `DeleteFile`, `Move`, `Copy`, `WriteBytes`) захватывают эксклюзивный `std::unique_lock<std::shared_mutex> lock(mutex);` (например, [`VirtualFileSystem.cpp:220, 243, 622`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L220)).

### 1.5. Наличие List / Stat / Exists
* `Exists`: **Есть**, метод `bool Exists(std::string_view virtualPath) const;` ([`VirtualFileSystem.h:68`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Resources/VirtualFileSystem.h#L68), [`VirtualFileSystem.cpp:313-322`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L313-L322)).
* `Stat`: Метода с именем `Stat` нет, функциональным эквивалентом является `GetFileInfo(virtualPath)` ([`VirtualFileSystem.h:86`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Resources/VirtualFileSystem.h#L86), [`VirtualFileSystem.cpp:387-408`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L387-L408)), возвращающий `VirtualFileInfo` (размер в байтах, расширение, тип ассета, физический путь).
* `List`: Метода с именем `List` нет, функциональным эквивалентом являются `EnumerateFiles(virtualDir, recursive)` ([`VirtualFileSystem.h:89`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Resources/VirtualFileSystem.h#L89), [`VirtualFileSystem.cpp:410-430`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L410-L430)) и `FindFiles(...)` ([`VirtualFileSystem.h:92`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Resources/VirtualFileSystem.h#L92)).

### 1.6. Нормализация ".."
* **Есть**, реализована в `VirtualFileSystem::NormalizeVirtualPath` ([`VirtualFileSystem.cpp:186-193`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L186-L193)):
  ```cpp
  else if (seg == "..")
  {
      // Directory traversal prevention: pop previous segment if any
      if (!segments.empty())
      {
          segments.pop_back();
      }
  }
  ```
  Алгоритм устраняет переходы вверх по директориям и предотвращает выход за пределы виртуального корня.

---

## 2. Поиск чтения файлов мимо VFS

Команда поиска:
`Select-String -Path "Engine\**\*", "Game\**\*", "Editor\**\*" -Pattern "\b(fopen|fopen_s|ifstream|CreateFile|D3DCompileFromFile)\b"`

Список мест чтения мимо VFS:
1. [`Engine/Include/Engine/Render/ShaderCompiler.h:43`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Render/ShaderCompiler.h#L43):
   ```cpp
   const HRESULT hr = D3DCompileFromFile(fileName.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, ...);
   ```
   *Шейдеры компилируются напрямую через Windows API с диска, минуя VFS.*
2. [`Engine/Source/Render/TextureLoader.cpp:31`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Render/TextureLoader.cpp#L31) $\to$ [`Engine/ThirdParty/stb/stb_image.h:1357, 1360`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/ThirdParty/stb/stb_image.h#L1357):
   ```cpp
   unsigned char* imageData = stbi_load(path.c_str(), &width, &height, &channels, 4);
   // Внутри stb_image.h:
   if (0 != fopen_s(&f, filename, mode)) f = fopen(filename, mode);
   ```
   *Текстуры открываются через CRT `fopen`/`fopen_s` библиотеки stb_image.*
3. [`Engine/Source/Components/SpecificComponents/FBXComponent.cpp:104`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/FBXComponent.cpp#L104):
   ```cpp
   Assimp::Importer importer;
   const aiScene* scene = importer.ReadFile(filePath, ...);
   ```
   *Assimp открывает 3D-модели FBX/OBJ через стандартные файловые потоки ОС напрямую по физическому пути.*
4. [`Engine/Runtime/Project.cpp:101`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Runtime/Project.cpp#L101):
   ```cpp
   std::ifstream input(file, std::ios::binary);
   ```
   *Чтение `Project.json` при старте EngineRuntime.*
5. [`Game/Common/Deployment.h:34`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Game/Common/Deployment.h#L34):
   ```cpp
   std::ifstream input(manifest);
   ```
   *Чтение `RequiredFiles.txt` в проверках развертывания лаунчера.*
6. [`Editor/Source/FileBrowser.cpp:27-35, 61-66`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Editor/Source/FileBrowser.cpp#L27-L35):
   ```cpp
   model_ = new QFileSystemModel(this);
   model_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
   ```
   *Файловый обозреватель Qt напрямую сканирует дисковую файловую систему ОС.*
7. [`Engine/Source/MainGame/BaseGameClass/Game.cpp:1830`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/MainGame/BaseGameClass/Game.cpp#L1830):
   ```cpp
   if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
   ```
   *Запись дампов кадров/скриншотов.*
8. [`Engine/Source/Resources/VirtualFileSystem.cpp:332`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Resources/VirtualFileSystem.cpp#L332):
   ```cpp
   std::ifstream stream(resolved->c_str(), std::ios::binary | std::ios::ate);
   ```
   *Штатное открытие потока внутри самого метода `VirtualFileSystem::ReadBytes`.*

*Примечание:* Вызовов `CreateFile` / `CreateFileA` / `CreateFileW` в репозитории не обнаружено.

---

## 3. FBXComponent

### 3.1. Флаги постобработки Assimp
* **Файл:** [`Engine/Source/Components/SpecificComponents/FBXComponent.cpp:104-109`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/FBXComponent.cpp#L104-L109)
* **Фрагмент кода:**
  ```cpp
  const aiScene* scene = importer.ReadFile(filePath,
      aiProcess_Triangulate |
      aiProcess_ConvertToLeftHanded |
      aiProcess_GenNormals |
      aiProcess_PreTransformVertices |
      aiProcess_OptimizeMeshes);
  ```

### 3.2. Обработка костей и анимаций
* **Обработка отсутствует.** Флаг `aiProcess_PreTransformVertices` (строка 108) принудительно запекает локальные трансформации узлов в геометрию вершин и схлопывает иерархию узлов.
* В методе `ImportMesh` ([`FBXComponent.cpp:54-63`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/FBXComponent.cpp#L54-L63)) считываются только позиции, нормали и UV:
  ```cpp
  const aiVector3D& position = mesh->mVertices[i];
  const aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
  const aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0.0f, 0.0f, 0.0f);
  ```
  Поля костей (`mesh->mBones`), весов вершин и массив анимаций сцены (`scene->mAnimations`) полностью игнорируются.

### 3.3. Формат вершины
* **Файл:** [`Engine/Source/Components/SpecificComponents/FBXComponent.cpp:18, 60-62`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/FBXComponent.cpp#L18)
* **Размер (stride):** 48 байт (`sizeof(DirectX::XMFLOAT4) * 3`).
* **Фрагмент кода:**
  ```cpp
  constexpr UINT MeshVertexStride = sizeof(DirectX::XMFLOAT4) * 3;
  // На каждую вершину кладутся 3 элемента XMFLOAT4:
  result.Vertices.emplace_back(position.x, position.y, position.z, 1.0f); // Position
  result.Vertices.emplace_back(normal.x, normal.y, normal.z, 0.0f);       // Normal
  result.Vertices.emplace_back(uv.x, uv.y, 0.0f, 0.0f);                   // TexCoord
  ```

### 3.4. Загрузка текстур
* **Файл:** [`Engine/Source/Components/SpecificComponents/FBXComponent.cpp:75-76, 263-295`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/FBXComponent.cpp#L263-L295)
* Из материалов Assimp извлекается только диффузный цвет: `result.Color = ReadDiffuseColor(material);`.
* Текстура загружается вручную вызовом `FBXComponent::LoadTexture(const std::string& texturePath)`:
  ```cpp
  const auto cacheKey = std::make_pair(static_cast<const ID3D11Device*>(device), MakeAssetKey(texturePath));
  std::shared_ptr<CachedFBXTexture> texture = textureCache[cacheKey].lock();
  if (!texture) {
      ...
      TextureLoader::LoadTexture2D(device, context, texturePath, true, newTexture->TextureSRV);
  }
  ```
  Используется кэш слабых ссылок `textureCache` на базе `std::weak_ptr<CachedFBXTexture>`.

---

## 4. InstancedMeshComponent

### 4.1. Формат инстанс-данных (структура)
* **CPU-структура логики:** [`Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h:10-17`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h#L10-L17)
  ```cpp
  struct MeshInstance {
      glm::vec3 Position{0.0f, 0.0f, 0.0f};
      float Yaw = 0.0f;   // radians
      float Scale = 1.0f; // uniform
      glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
      bool bVisible = true;
  };
  ```
* **GPU-структура в буфере (StructuredBuffer):** [`Engine/Include/Engine/Render/ShaderConstants.h:67-73`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Render/ShaderConstants.h#L67-L73) и [`Engine/Content/Shaders/Instancing.hlsli:6-11`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Content/Shaders/Instancing.hlsli#L6-L11)
  ```cpp
  struct InstData {
      DirectX::XMFLOAT4X4 World;                    // 64 байта
      DirectX::XMFLOAT4 ObjectColor;                // 16 байт
      DirectX::XMFLOAT4 Params{1.0f, 0.0f, 0.0f, 0.0f}; // 16 байт (x = LOD transition fade)
  };
  ```
  Итоговый размер элемента: **96 байт**. Вектор `Params.yzw` зарезервирован и обнулен.

### 4.2. Где переключается LOD
* **Метод выбора:** `InstancedMeshComponent::SelectLod` ([`InstancedMeshComponent.cpp:311-331`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/InstancedMeshComponent.cpp#L311-L331)).
* **Вызов и упаковка:** метод `InstancedMeshComponent::BuildVisibleSets()` ([`InstancedMeshComponent.cpp:406-431`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/InstancedMeshComponent.cpp#L406-L431)).
* Переключение выполняется по экранной высоте проекции ограничивающей сферы юнита `screenHeightPixels` (пороги `LodScreenHeights`) либо по расстоянию до камеры `LodDistances`. В переходном интервале `LodFadeFraction` рассчитывается dither кросс-фейд между двумя уровнями.

### 4.3. Возможность добавления clipId / phase без правки ядра
* **ОТВЕТ: НЕТ.**
* **Причина:**
  1. Структура `MeshInstance` объявляется в заголовке ядра [`InstancedMeshComponent.h`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h).
  2. Метод `InstancedMeshComponent::BuildVisibleSets()`, производящий упаковку `MeshInstance` $\to$ `InstData` (`data.Params = DirectX::XMFLOAT4(fade, 0.0f, 0.0f, 0.0f);`), скомпилирован внутри статической библиотеки `Engine.lib`.
  3. Шейдеры [`Instancing.hlsli`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Content/Shaders/Instancing.hlsli) и [`InstancedMesh.hlsl`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Content/Shaders/InstancedMesh.hlsl) жестко завязаны на раскладку `InstData`. Хотя физически в `Params.yz` есть свободное место под `clipId` и `phase`, код наполнения и проброса этих параметров из игровой логики в буфер полностью инкапсулирован в ядре.

---

## 5. Импосторы и каскадные тени

### 5.1. Интеграция импосторов
* **Файлы:** [`InstancedMeshComponent.h:146`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h#L146), [`InstancedMeshComponent.cpp:628-667`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/InstancedMeshComponent.cpp#L628-L667), [`ImpostorAtlas.cpp:60`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Render/ImpostorAtlas.cpp#L60).
* Импосторы представлены как дополнительный уровень LOD (`LodLevel ImpostorLevel`). Атлас 2D-ракурсов модели (16 азимутов $\times$ 3 угла возвышения) запекается один раз на старте в `AlbedoSRV` и `NormalSRV`.
* При превышении порога дальности инстансы попадают в `ImpostorLevel.ColorStaging`. Отрисовка выполняется функцией `DrawImpostors`: процедурный quad (`D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP` из 4 вершин) отрисовывается через `Context->DrawInstanced(4, instanceCount, 0, 0)` с шейдером [`Impostor.hlsl`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Content/Shaders/Impostor.hlsl).
* **Спрайты/импосторы не отбрасывают тени:** в [`InstancedMeshComponent.cpp:462`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/InstancedMeshComponent.cpp#L462) явно установлено: `Sprites never cast: the depth pass always uses a mesh instead. const int meshLevel = std::min(level, LodCount - 1);`.

### 5.2. Интеграция каскадных теней (CSM)
* **Файлы:** [`InstancedMeshComponent.cpp:460-491, 738-790`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/InstancedMeshComponent.cpp#L460-L491).
* **Отсутствие отсечения по камере:** инстансы за пределами экрана отбрасывают тени в видимую область, поэтому отсечение по frustum камеры для прохода теней отключено.
* **Каскадное распределение:** инстансы проверяются на попадание в охватывающие сферы каскадов `cascadeSpheres[cascade]` с учетом направления света и выноса кастеров `ShadowCasterExtrusion`.
* **Выбор LOD для теней:** для дальних каскадов выбирается более грубая геометрия:
  `const int shadowLevel = std::min(meshLevel + ShadowLodBias + cascade, LodCount - 1);`
  Данные собираются в `Lods[shadowLevel].ShadowStaging[cascade]` и отрисовываются через `DrawIndexedInstanced` с шейдером глубины `ShadowDepth.hlsl`.

---

## 6. Шейдеры и буферы констант

### 6.1. Лимиты Constant Buffer в DirectX 11
* Максимальный размер константного буфера аппаратно ограничен **64 КиБ** (4096 векторов `float4` / 65 536 байт).
* Текущий размер `ConstantBufferData` (`register(b0)`) в [`ShaderConstants.h:12-40`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Render/ShaderConstants.h#L12-L40) составляет **1072 байта** (около 1.05 КиБ). В буфере свободно более 62 КиБ.

### 6.2. Возможность размещения Bone Palette в cbuffer
* **Для единичных моделей ([`BaseFBX.hlsl`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Content/Shaders/BaseFBX.hlsl)):**
  Палитра костей для скелета из 64–128 матриц ($64 \times 64 = 4096$ байт, $128 \times 64 = 8192$ байта) свободно помещается в отдельный константный буфер (например, `register(b1)`).
* **Для массового инстансинга ([`InstancedMesh.hlsl`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Content/Shaders/InstancedMesh.hlsl)):**
  Размещение матриц костей в cbuffer **невозможно**, так как для 10 000–30 000 инстансов требуемый объем памяти составляет $30000 \times 64 \text{ кости} \times 64 \text{ байта} \approx 120\text{ МБ}$, что в тысячи раз превышает предел 64 КиБ.

### 6.3. Текстура анимации / StructuredBuffer
* Для `InstancedMesh.hlsl` вершинный шейдер сейчас занимает только слот `t0` (`StructuredBuffer<InstData> InstBuf : register(t0)`).
* Слоты `t1..t127` полностью свободны и могут использоваться под `Texture2D<float4> AnimationTexture : register(t1)` (текстура запеченных кватернионов/матриц костей) либо `Buffer<float4>`.
* В структуре `InstData` поля `Params.y` и `Params.z` свободны (reserved), что позволяет передавать `clipId` и `phase` без изменения размера структуры `InstData` (96 байт).

---

## 7. ModuleApi.h v1–v3

* **Файл:** [`Engine/Include/Engine/Runtime/ModuleApi.h:1-49`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Include/Engine/Runtime/ModuleApi.h#L1-L49)
* **Точка экспорта:**
  ```cpp
  extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* output);
  typedef int (ENGINE_MODULE_CALL *EngineGetModuleApiFn)(uint32_t version, EngineModuleApi* output);
  ```

### Структуры и функции:
1. `EngineModuleHost` (интерфейс ядра, предоставляемый модулю):
   * **v1:**
     * `uint32_t version`
     * `void* user`
     * `void (*log)(void* user, const char* message)`
     * `void (*requestExit)(void* user)`
     * `const char* projectName`
     * `const char* projectFile`
     * `const char* contentRoot`
   * **v2 (добавлены сервисы вьюпорта):**
     * `int (*viewportAttach)(void* user, void* hwnd, uint32_t w, uint32_t h)`
     * `int (*viewportResize)(void* user, uint32_t w, uint32_t h)`
     * `int (*viewportDetach)(void* user)`
   * **v3 (добавлено управление камерой):**
     * `void (*viewportCamera)(void* user, float right, float up, float forward, float yaw, float pitch, float zoom)`
2. `EngineModuleApi` (таблица функций жизненного цикла, заполняемая модулем DLL):
   * `uint32_t version`
   * `uint32_t kind` (`ENGINE_MODULE_GAME = 1u`, `ENGINE_MODULE_EDITOR = 2u`)
   * `void* (*create)(const EngineModuleHost* host)`
   * `int (*start)(void* session)`
   * `int (*update)(void* session, double deltaSeconds)`
   * `int (*overlay)(void* session)` (обязателен только для редактора)
   * `void (*stop)(void* session)`
   * `void (*destroy)(void* session)`

---

## 8. Наличие консоли и CommandRegistry

* **ОТВЕТ: НЕТ.**
* **Проверка:**
  * Поиск по имени: `git grep -i CommandRegistry` $\to$ совпадений 0.
  * Поиск консоли: в [`Editor/Include/MainWindow.h:28`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Editor/Include/MainWindow.h#L28) имеется только виджет вывода текста логов `QPlainTextEdit* console_`.
  * Интерактивной консоли команд, реестра команд или парсера командной строки в рантайме движка **не существует**.

---

## 9. FileBrowser.cpp и VFS

* **Файл:** [`Editor/Source/FileBrowser.cpp:27-35, 61-66`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Editor/Source/FileBrowser.cpp#L27-L35)
* **ОТВЕТ: Ходит в файловую систему напрямую.**
* **Фрагмент кода:**
  ```cpp
  const QFileInfo projectFile(QString::fromUtf8(host_->host().projectFile ? host_->host().projectFile : ""));
  projectDir_ = projectFile.absoluteDir().absolutePath();
  contentDir_ = QString::fromUtf8(host_->host().contentRoot ? host_->host().contentRoot : "");
  ...
  model_ = new QFileSystemModel(this);
  model_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
  ```
  Используется `QFileSystemModel` из состава Qt, который обращается к физическим каталогам ОС. Виртуальная файловая система `VirtualFileSystem` здесь не задействована.

---

## 10. Аллокации на кадр в Battle

Команда поиска:
`Select-String -Path "Game\Battle\Source\*.cpp", "Engine\Source\Components\SpecificComponents\InstancedMeshComponent.cpp" -Pattern "new |malloc|push_back"`

* **В `Game/Battle/Source/BattleGame.cpp`:**
  * В per-frame методах `PreUpdate` и `UpdateUnits` вызовов `new`, `malloc` и `push_back` **НЕТ**.
  * Позиции обновляются на месте:
    ```cpp
    batch.Component->SetInstancePosition(index, position);
    // внутри: Instances[index].Position = position;
    ```
* **В конвейере инстансинга ядра ([`InstancedMeshComponent.cpp:442, 489`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/Components/SpecificComponents/InstancedMeshComponent.cpp#L442)):**
  ```cpp
  bucket.ColorStaging.push_back(data);
  Lods[shadowLevel].ShadowStaging[cascade].push_back(data);
  ```
  *Векторы очищаются через `.clear()`, сохраняя аллоцированную емкость (`capacity`). После первых 5–10 кадров прогрева емкость стабилизируется, и аллокации из кучи на кадр отсутствуют.*

---

## 11. ozz-animation

* **Репозиторий:** [guillaumeblanc/ozz-animation](https://github.com/guillaumeblanc/ozz-animation)
* **Актуальная версия:** **0.17.0** (август 2026; стабильная ветка 0.16.x / 0.17.x).
* **Лицензия:** **MIT License** (разрешительная лицензия с сохранением копирайта Guillaume Blanc).
* **Требования к сборке:**
  * Исходный проект использует CMake.
  * Для runtime-декодирования и выборки анимаций требуются только два модуля: `ozz_base` и `ozz_animation_runtime`.
  * Модули написаны на стандартном C++ с SIMD-оптимизациями (SSE/AVX/NEON) и не имеют сторонних зависимостей. Они компилируются как статическая библиотека или напрямую включаются в `Engine.vcxproj`.
  * Модуль `ozz_animation_offline` (конвертер raw/fbx $\to$ ozz) требует FBX SDK/Assimp и для рантайма игры не нужен.
* **Оценка объёма vendoring:**
  * ~25–30 файлов исходного кода (.cc / .h).
  * Объем исходников runtime: около 1.5–2.0 МБ.

---

## 12. Базовый замер Battle (Release)

### 12.1. Конфигурация системы
* **GPU (DXGI):** NVIDIA GeForce RTX 5060 Laptop GPU (Direct3D 11.1 feature level)
* **CPU:** AMD Ryzen 7 7745HX with Radeon Graphics (8 физических ядер, 16 логических процессоров)

### 12.2. Параметры сцены Battle
* **Юнитов:** 30 000
* **Типов техники:** 4 (Танк, БТР, Грузовик, САУ)
* **Статических объектов (Scenery):** 300
* **Режим рендеринга:** Deferred
* **Тени:** Включены (3 каскада CSM для направленного света + запеченная статическая карта 2048x2048)
* **Импосторы (спрайты):** Включены

### 12.3. Результаты замеров

Команда запуска:
`.\Game\build\x64\Release\Battle\Battle.exe --frames 200`

**Вывод встроенного замера:**
```text
battle-measurement units=30000 types=4 scenery=300 mode=deferred shadows=on sprites=on frames=190 frame_ms=10.94 fps=91.4 gpu_ms=8.58 draws=19.0 instances=50747 lod0=0 lod1=0 lod2=34 sprite=19091 culled=10909
```

* **Средний кадр CPU:** **10.94 мс** (соответствует **91.4 FPS**).
* **Средний кадр GPU:** **8.58 мс** (по аппаратным timestamp disjoint запросам D3D11).
* **Draw calls:** **19.0** на кадр.
* **Отрисованных инстансов:** **50 747** (включая проходы теней).
* **Распределение по уровням детализации:**
  * LOD 0: 0
  * LOD 1: 0
  * LOD 2: 34
  * Спрайты (импосторы): 19 091
  * Отсечено вне видимости (culled): 10 909

### 12.4. Анализ работы встроенного GPU таймера
В [`Game.cpp:2779-2840`](file:///c:/Users/Yuri/Documents/GitHub/Engine/Engine/Source/MainGame/BaseGameClass/Game.cpp#L2779-L2840) встроен таймер на `D3D11_QUERY_TIMESTAMP_DISJOINT` с конвейерным буфером `GpuTimerLatency = 3`.
При коротких запусках (до 100 кадров) или при задержке драйвера метод `GetData(..., D3D11_ASYNC_GETDATA_DONOTFLUSH)` может возвращать `S_FALSE`, сбрасывая `GpuTimerIssued = false`, из-за чего в логе периодически фиксируется `gpu_ms=0.00`. При достаточном числе кадров (от 150–200 кадров) конвейер стабилизируется и сообщает достоверное время GPU (**8.58 мс**).

---

## Таблица компонентов

| Компонент | Что есть | Что нужно | Где риск дублирования |
| :--- | :--- | :--- | :--- |
| **VirtualFileSystem** | `Mount`, `Resolve`, `Exists`, `ReadBytes/Text`, `GetFileInfo`, `EnumerateFiles`, `NormalizeVirtualPath`, `shared_mutex` | Интеграция в загрузку шейдеров (`ShaderCompiler`), текстур (`TextureLoader`), моделей Assimp и FileBrowser | Риск параллельного существования VFS и прямых вызовов CRT/Win32/Qt путей |
| **FBXComponent** | Загрузка статических мешей Assimp, схлопывание геометрии (`PreTransformVertices`), кэширование моделей и текстур | Чтение костей, весов вершин, клипов анимаций, скелетной иерархии, связывание с VFS | Дублирование структур вершин (сейчас 3x float4 без весов и индексов костей) |
| **InstancedMeshComponent** | 4 уровня LOD, запекание импосторов в атлас, каскадные тени, dither кросс-фейд | Поддержка анимированных инстансов: хранение `clipId`, фазы/времени, чтение костей из Animation Texture | Риск перегрузки константных буферов; необходимость синхронизации полей в `InstData` и шейдере |
| **Шейдеры (HLSL)** | Deferred G-Buffer (`Common.hlsli`), инстансинг (`InstancedMesh.hlsl`), тени (`ShadowDepth.hlsl`), импосторы | Вершинный скиннинг для FBX (`BaseFBX.hlsl`), выборка матриц костей из Texture2D в `InstancedMesh.hlsl` | Расхождение формата `InstData` между C++ (`ShaderConstants.h`) и HLSL (`Instancing.hlsli`) |
| **ModuleApi** | ABI v3: хост логирования, выхода, вьюпорта Qt и команд камеры | Контракт для передачи команд отладки, параметров времени анимации или консоли | Ломающие изменения сигнатуры при смене версии ABI (требует синхронного обновления модулей) |
| **FileBrowser** | Полноценный Qt UI для навигации по папкам проекта | Отображение через VFS-дерево (виртуальные точки монтирования `/Engine`, `/Game`) | Расхождение реальной структуры диска и виртуальных путей VFS |

---

## Список вопросов к пользователю

1. **Интеграция ozz-animation:** планируется ли компилировать исходники `ozz_base` и `ozz_animation_runtime` напрямую в составе `Engine.vcxproj` (как vendored third-party код без внешних DLL) или собирать отдельной статической библиотекой через отдельный проект?
2. **Формат хранения анимаций для инстансов:** для отрисовки 30 000 юнитов в Battle со скиннингом классический подход CPU-скиннинга не уложится в бюджет кадра. Подтверждаете ли вы концепцию **Animation Texture (VAT)**, где матрицы/кватернионы костей запекаются в текстуру, а `InstancedMesh.hlsl` сэмплирует их по `(clipId, phase, boneIndex)`?
3. **Миграция подсистем на VFS:** требуется ли на следующих этапах полный перевод `ShaderCompiler` (через компиляцию из памяти `D3DCompile` вместо `D3DCompileFromFile`), `TextureLoader` (`stbi_load_from_memory`) и Assimp (`Assimp::Importer::ReadFileFromMemory`) на чтение через `VirtualFileSystem::ReadBytes`?
4. **Консоль разработчика:** требуется ли реализация полноценного реестра команд (`CommandRegistry`) и внутриигровой/редакторской консоли, или на текущем этапе достаточно интеграции с существующим `MainWindow::console_` в Qt?

---

## Не проверено

1. **Работа ozz-animation на текущем компиляторе MSVC v145:** прямая компиляция библиотек ozz-animation в текущем решении не производилась (в соответствии с запретом на добавление и изменение исходного кода движка в фазе 0).
2. **Время работы Battle в режиме Forward:** замер производился в основном боевом режиме (Deferred + Shadows + Impostors). Режим Forward для 30 000 юнитов детально не профилировался.
3. **Поведение GPU Timer при VSync = ON:** при замерах флаг VSync был автоматически отключен ключом `--frames`, поведение таймера при локе на 60 Гц не фиксировалось.
