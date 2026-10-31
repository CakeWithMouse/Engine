# Видимость типов и возможности проектов с C++

Состояние реализации на 2026-10-28. Таблицы отделяют наличие типа в C++ SDK от его отображения и создания в редакторе. Регистрация пользовательских классов выполняется при сборке; неподписанные метаданными классы автоматически в палитру не попадают.

## Возможности пользователя

| Возможность | Где доступна | Условия и границы |
|---|---|---|
| Проект без кода | Лаунчер → Создать проект | Без флажка C++; готовая DLL, компилятор не требуется |
| Проект с кодом | Лаунчер → Создать проект → C++ | SDK закреплённого выпуска и Visual Studio C++ tools |
| Добавить C++ позже | Лаунчер или меню Проект / C++ | Для штатной DLL; сцены и ресурсы сохраняются; при ошибке прежний модуль остаётся активным |
| Новый класс | Проект / C++ → Добавить C++ класс | Component, Game, Object; имя, родитель и заголовок; файлы .h/.cpp |
| Родительские методы | Созданные исходники | Game: AfterInitialize, PreUpdate, TickComponents; Component: Tick; вызов непосредственного родителя |
| Свой компонент | ENGINE_COMPONENT | Публичное однозначное наследование; конкретный класс с доступным default constructor |
| Своя игра | ENGINE_GAME | Публичный конкретный наследник Game; доступный default constructor |
| Другая иерархия | ENGINE_OBJECT | Реальный T не обязан наследовать классы Engine; Scene владеет неотрисовываемой оболочкой |
| Параметризованный Object | ProjectObjectAdapter<T>::Create | Автор явно предоставляет фабрику и кодек; автоматического подбора аргументов нет |
| Классы проекта | Проект → C++ → Типы C++ проекта | Только пользовательские типы последней успешной сборки; Game / Component / Object |
| Выбор Game | Диалог классов → Выбрать Game | Применяется к следующему Play и отдельному запуску; хранится в Content/ProjectGame.txt |
| Создание Component / Object | Палитра сцены | Object имеет пометку [Object], не рисует геометрию; выбирается в иерархии |
| Категории создания | Сцена / объекты | Примитивы, Свет, Частицы, Окружение, Персонажи, Камеры, Классы проекта; общая кнопка добавления |
| Частицы | Частицы → Искры / Дым | GPU-пресеты, предпросмотр в Edit; Pause останавливает симуляцию |
| Скайбокс | Окружение → Базовое небо | Процедурный градиент без внешних текстур; не перехватывает выбор |
| Выделение | ЛКМ / иерархия / Tab | Золотистая рамка границ, подсветка строки, имя и тип в инспекторе; Esc снимает выбор |
| Встроенный Play | Панель редактора | Пользовательский Game, одна симуляция за кадр; нейтральный Game в edit preview |
| External / Играть | Редактор / лаунчер | Пользовательский Game, сохранённая стартовая сцена |
| Пользовательские значения | Данные выбранного объекта | JSON через явный SaveProjectData/LoadProjectData либо Object adapter; version обязателен |
| Undo / Redo / Duplicate / Play copy | Редактор | Используют кодек; ссылки remap-ятся объявленным автором RemapProjectReferences/Remap |
| Устойчивый ID | Второй аргумент аннотации | Например ENGINE_COMPONENT("Mover", "MyGame.Mover"); позволяет переименовать C++ класс |
| Сборка проекта | Лаунчер / меню C++ / Build.ps1 | Только проектная DLL, готовые библиотеки SDK; отдельная папка кандидата |
| Ошибка сборки | Console / CodeBuild.log / validate-error.log / startup-error.log | Активный module path не переключается до проверки сцен, Game factory и трёх кадров запуска |
| Применение | Повторное открытие редактора | Живое игровое состояние сбрасывается; автоматического переноса памяти нет |
| Visual Studio | Game.sln | SDK ищется по lock-выпуску в установленном store; применение сборки через Build.ps1 |
| Старые проекты | Лаунчер и адаптер Edit 4 / Scene 2 | Старые фиксированные C API сохранены; выпуск выбирается по lock |
| Редактирование всех полей C++ | Недоступно | Нужен объявленный кодек; private/transient поля не отражаются автоматически |
| Абстрактные и не создаваемые классы | Не появляются как создаваемые типы | Ошибка компиляции при неподходящей аннотации; обычный неотмеченный базовый класс допустим |
| Классы движка без регистрации | Только C++ API | Само присутствие заголовка не создаёт кнопку в редакторе |

## Встроенные типы нового проекта

| ID | Имя в редакторе | Реализация | Действие пользователя |
|---|---|---|---|
| Primitive.Cube | Куб | SceneProject::Cube → CubeComponent | Добавить из палитры |
| Primitive.Sphere | Сфера | SceneProject::Sphere → SphereComponent | Добавить из палитры |
| Primitive.Plane | Плоскость | SceneProject::Plane → Mesh | Добавить из палитры |
| Primitive.Cylinder | Цилиндр | SceneProject::Cylinder → Mesh | Добавить из палитры |
| Primitive.Cone | Конус | SceneProject::Cone → Mesh | Добавить из палитры |
| Primitive.Pyramid | Пирамида | SceneProject::Pyramid → Mesh | Добавить из палитры |
| Light.Point | Точечный свет | SceneProject::PointLight → PointLightComponent | Добавить из палитры |
| Light.Directional | Направленный свет (один на сцену) | DirectionalLightComponent | Сцена → Направленный свет или Свет → Добавить; поворот задаёт направление |
| Particle.Sparks | Искры | SceneProject::Sparks → ParticleSystemComponent | Частицы → Добавить |
| Particle.Smoke | Дым | SceneProject::Smoke → ParticleSystemComponent | Частицы → Добавить |
| Environment.Sky | Базовое небо | SceneProject::Sky → SkyboxComponent | Окружение → Добавить |
| Engine.Model | Импортированная модель | SkeletalMeshComponent | Импортировать ресурс и добавить модель; не пустая фабрика палитры |

Направленный свет хранится в сцене: цвет, интенсивность, включение, тени и их дальность доступны в инспекторе. Позиция и масштаб задают положение маркера, освещение определяется только поворотом. Для выбранного света viewport показывает стрелку направления лучей. Повторное добавление выбирает существующий источник; дублирование второго источника запрещено. Настройки сохраняются в формате сцены 3, работают в Play, Undo/Redo и после повторного открытия.

В редактируемых сценах нет скрытого солнца или фоновой подсветки: без источников обычные материалы чёрные. Точечные лампы продолжают освещать объекты, а неосвещаемые (unlit) и светящиеся материалы остаются видимыми. Базовое небо само по себе не освещает сцену. Старый C++ режим `Game` сохраняет ручное управление светом; сценовые модули используют `SetSceneLightingEnabled(true)`. Старые сцены без направленного источника нужно осветить явно через меню сцены.

## Все определения классов в публичных заголовках Engine

Именованные определения `class`, включая служебные и шаблонные. Forward declarations, enum, примеры в комментариях и внешние библиотеки исключены. Специализации делегатов указаны отдельно по строке исходника.

| Класс | База | Видимость пользователю редактора | Создание | Наследование / применение | Источник |
|---|---|---|---|---|---|
| `BaseEngine` | `—` | Нет, служебный тип | Нет отдельного действия | Нет: final | [Engine/Include/Engine/Base/Base.h:21](../../Engine/Include/Engine/Base/Base.h) |
| `BaseGameConfig` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Base/Config/BaseGameConfig.h:6](../../Engine/Include/Engine/Base/Config/BaseGameConfig.h) |
| `BaseResources` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Base/Config/BaseResources.h:11](../../Engine/Include/Engine/Base/Config/BaseResources.h) |
| `GameComponent` | `—` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/GameComponents.h:36](../../Engine/Include/Engine/Components/GameComponents.h) |
| `SkyboxComponent` | `public CubeComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/Light/CubeMap.h:4](../../Engine/Include/Engine/Components/Light/CubeMap.h) |
| `PointLightComponent` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/Light/PointLightComponent.h:5](../../Engine/Include/Engine/Components/Light/PointLightComponent.h) |
| `ProjectObjectComponent` | `public GameComponent` | Служебная оболочка скрыта; виден зарегистрированный T | Оболочку создаёт фабрика Object | final; специализируется ProjectObjectAdapter<T> | [Engine/Include/Engine/Components/ProjectObject.h:17](../../Engine/Include/Engine/Components/ProjectObject.h) |
| `CubeComponent` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/SpecificComponents/CubeComponent.h:37](../../Engine/Include/Engine/Components/SpecificComponents/CubeComponent.h) |
| `FBXComponent` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h:51](../../Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h) |
| `InstancedMeshComponent` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h:34](../../Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h) |
| `ParticleSystemComponent` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h:75](../../Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h) |
| `SkeletalMeshComponent` | `public GameComponent` | Как Engine.Model после импорта; C++ имя скрыто | Импорт модели или зарегистрированный наследник | Да: ENGINE_COMPONENT; модель/ресурсы задаются явно | [Engine/Include/Engine/Components/SpecificComponents/SkeletalMeshComponent.h:15](../../Engine/Include/Engine/Components/SpecificComponents/SkeletalMeshComponent.h) |
| `SphereComponent` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/SpecificComponents/SphereComponent.h:6](../../Engine/Include/Engine/Components/SpecificComponents/SphereComponent.h) |
| `TriangleComponent` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Components/SpecificComponents/TriangleComponent.h:4](../../Engine/Include/Engine/Components/SpecificComponents/TriangleComponent.h) |
| `Display` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Display/Display.h:7](../../Engine/Include/Engine/Display/Display.h) |
| `IDelegateBase` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:182](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `IDelegate` | `public IDelegateBase` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:191](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `StaticDelegate` | `public IDelegate<RetVal, Args...>` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:201](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `RawDelegate` | `public IDelegate<RetVal, Args...>` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:228](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `LambdaDelegate` | `public IDelegate<RetVal, Args...>` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:261](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `SPDelegate` | `public IDelegate<RetVal, Args...>` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:288](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `DelegateHandle` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:333](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `InlineAllocator` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:404](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `DelegateBase` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:541](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `Delegate` | `public DelegateBase` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:654](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `MulticastDelegateBase` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:787](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `MulticastDelegate` | `public MulticastDelegateBase` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/Delegates.h:794](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `InputDevice` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/InputDevice/InputDevice.h:82](../../Engine/Include/Engine/InputDevice/InputDevice.h) |
| `Game` | `—` | Сам базовый тип скрыт; зарегистрированные наследники видны | Выбор Game, не объект сцены | Да: ENGINE_GAME | [Engine/Include/Engine/MainGame/BaseGameClass/Game.h:122](../../Engine/Include/Engine/MainGame/BaseGameClass/Game.h) |
| `Player` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/MainGame/Player.h:20](../../Engine/Include/Engine/MainGame/Player.h) |
| `SceneProject::Sparks` | `public ParticleSystemComponent` | Да: Particle.Sparks | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:13](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Smoke` | `public ParticleSystemComponent` | Да: Particle.Smoke | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:23](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Sky` | `public SkyboxComponent` | Да: Environment.Sky | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:33](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::PointLight` | `public PointLightComponent` | Да: Light.Point | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:39](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Cube` | `public CubeComponent` | Да: Primitive.Cube | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:44](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Sphere` | `public SphereComponent` | Да: Primitive.Sphere | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:49](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Mesh` | `public GameComponent` | Исходный C++ тип скрыт; зарегистрированные наследники видны | Через конкретного наследника и фабрику | Да: ENGINE_COMPONENT, если конструктор доступен | [Engine/Include/Engine/Project/StandardTypes.h:54](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Plane` | `public Mesh` | Да: Primitive.Plane | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:76](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Cylinder` | `public Mesh` | Да: Primitive.Cylinder | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:87](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Cone` | `public Mesh` | Да: Primitive.Cone | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:103](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `SceneProject::Pyramid` | `public Mesh` | Да: Primitive.Pyramid | Да, из палитры | final; используйте базовый компонент | [Engine/Include/Engine/Project/StandardTypes.h:117](../../Engine/Include/Engine/Project/StandardTypes.h) |
| `ImpostorAtlas` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Render/ImpostorAtlas.h:25](../../Engine/Include/Engine/Render/ImpostorAtlas.h) |
| `VirtualFileSystem` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Resources/VirtualFileSystem.h:36](../../Engine/Include/Engine/Resources/VirtualFileSystem.h) |
| `GameEditModule` | `—` | Нет, служебный тип | Нет отдельного действия | Нет: final | [Engine/Include/Engine/Runtime/GameEditModule.h:10](../../Engine/Include/Engine/Runtime/GameEditModule.h) |
| `SceneEditSession` | `—` | Нет, служебный тип | Нет отдельного действия | Нет: final | [Engine/Include/Engine/Runtime/SceneEditSession.h:11](../../Engine/Include/Engine/Runtime/SceneEditSession.h) |
| `ViewportSession` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Runtime/ViewportSession.h:9](../../Engine/Include/Engine/Runtime/ViewportSession.h) |
| `Scene` | `—` | Нет, служебный тип | Нет отдельного действия | Нет: final | [Engine/Include/Engine/Scene/Scene.h:65](../../Engine/Include/Engine/Scene/Scene.h) |
| `Scene::UpdateScope` | `—` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Scene/Scene.h:100](../../Engine/Include/Engine/Scene/Scene.h) |
| `SceneCommands` | `—` | Нет, служебный тип | Нет отдельного действия | Нет: final | [Engine/Include/Engine/Scene/SceneCommands.h:9](../../Engine/Include/Engine/Scene/SceneCommands.h) |
| `SceneDocumentError` | `public std::runtime_error` | Нет, служебный тип | Нет отдельного действия | Служебный API; не готовый игровой тип | [Engine/Include/Engine/Scene/SceneDocument.h:8](../../Engine/Include/Engine/Scene/SceneDocument.h) |

## Структуры данных и API в публичных заголовках

Эти структуры доступны C++ коду, но не являются элементами палитры. Для самостоятельного игрового типа из другой иерархии нужна ENGINE_OBJECT и подходящий адаптер.

| Структура | База | Источник |
|---|---|---|
| `AppliedMount` | `—` | [Engine/Include/Engine/Base/Base.h:31](../../Engine/Include/Engine/Base/Base.h) |
| `ComponentVisualDefaults` | `—` | [Engine/Include/Engine/Components/ComponentType.h:8](../../Engine/Include/Engine/Components/ComponentType.h) |
| `ComponentType` | `—` | [Engine/Include/Engine/Components/ComponentType.h:15](../../Engine/Include/Engine/Components/ComponentType.h) |
| `ComponentTypeCatalog` | `—` | [Engine/Include/Engine/Components/ComponentType.h:23](../../Engine/Include/Engine/Components/ComponentType.h) |
| `ComponentShaderVariant` | `—` | [Engine/Include/Engine/Components/GameComponents.h:28](../../Engine/Include/Engine/Components/GameComponents.h) |
| `ProjectObjectAdapter` | `—` | [Engine/Include/Engine/Components/ProjectObject.h:7](../../Engine/Include/Engine/Components/ProjectObject.h) |
| `HasRemap` | `std::false_type` | [Engine/Include/Engine/Components/ProjectObject.h:28](../../Engine/Include/Engine/Components/ProjectObject.h) |
| `HasRemap` | `std::true_type` | [Engine/Include/Engine/Components/ProjectObject.h:29](../../Engine/Include/Engine/Components/ProjectObject.h) |
| `GameType` | `—` | [Engine/Include/Engine/Components/ProjectObject.h:39](../../Engine/Include/Engine/Components/ProjectObject.h) |
| `GameTypeCatalog` | `—` | [Engine/Include/Engine/Components/ProjectObject.h:45](../../Engine/Include/Engine/Components/ProjectObject.h) |
| `CubeVertex` | `—` | [Engine/Include/Engine/Components/SpecificComponents/CubeComponent.h:7](../../Engine/Include/Engine/Components/SpecificComponents/CubeComponent.h) |
| `FBXImportedMesh` | `—` | [Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h:10](../../Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h) |
| `FBXImportedModel` | `—` | [Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h:19](../../Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h) |
| `FBXGpuMesh` | `—` | [Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h:27](../../Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h) |
| `FBXGpuModel` | `—` | [Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h:36](../../Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h) |
| `CachedFBXTexture` | `—` | [Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h:41](../../Engine/Include/Engine/Components/SpecificComponents/FBXComponent.h) |
| `MeshInstance` | `—` | [Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h:12](../../Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h) |
| `LodLevel` | `—` | [Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h:151](../../Engine/Include/Engine/Components/SpecificComponents/InstancedMeshComponent.h) |
| `GPUParticleData` | `—` | [Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h:7](../../Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h) |
| `GPUParticleSimulationCB` | `—` | [Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h:14](../../Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h) |
| `GPUParticleSortPair` | `—` | [Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h:43](../../Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h) |
| `GPUParticleSortCB` | `—` | [Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h:49](../../Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h) |
| `GPUParticleRenderCB` | `—` | [Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h:58](../../Engine/Include/Engine/Components/SpecificComponents/ParticleSystemComponent.h) |
| `MemberFunction` | `—` | [Engine/Include/Engine/InputDevice/Delegates.h:149](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `MemberFunction` | `—` | [Engine/Include/Engine/InputDevice/Delegates.h:155](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `DelegateHandlerPair` | `—` | [Engine/Include/Engine/InputDevice/Delegates.h:801](../../Engine/Include/Engine/InputDevice/Delegates.h) |
| `KeyboardInputEventArgs` | `—` | [Engine/Include/Engine/InputDevice/InputDevice.h:10](../../Engine/Include/Engine/InputDevice/InputDevice.h) |
| `RawMouseEventArgs` | `—` | [Engine/Include/Engine/InputDevice/InputDevice.h:69](../../Engine/Include/Engine/InputDevice/InputDevice.h) |
| `MouseMoveEventArgs` | `—` | [Engine/Include/Engine/InputDevice/InputDevice.h:91](../../Engine/Include/Engine/InputDevice/InputDevice.h) |
| `CubeMapResource` | `—` | [Engine/Include/Engine/MainGame/BaseGameClass/Game.h:65](../../Engine/Include/Engine/MainGame/BaseGameClass/Game.h) |
| `PointLightInfo` | `—` | [Engine/Include/Engine/MainGame/BaseGameClass/Game.h:73](../../Engine/Include/Engine/MainGame/BaseGameClass/Game.h) |
| `ShadowPassContext` | `—` | [Engine/Include/Engine/MainGame/BaseGameClass/Game.h:83](../../Engine/Include/Engine/MainGame/BaseGameClass/Game.h) |
| `FrameStats` | `—` | [Engine/Include/Engine/MainGame/BaseGameClass/Game.h:100](../../Engine/Include/Engine/MainGame/BaseGameClass/Game.h) |
| `Session` | `—` | [Engine/Include/Engine/Project/SceneModule.inl:31](../../Engine/Include/Engine/Project/SceneModule.inl) |
| `Guard` | `—` | [Engine/Include/Engine/Project/SceneModule.inl:186](../../Engine/Include/Engine/Project/SceneModule.inl) |
| `ImpostorAtlasSettings` | `—` | [Engine/Include/Engine/Render/ImpostorAtlas.h:11](../../Engine/Include/Engine/Render/ImpostorAtlas.h) |
| `MeshVertex` | `—` | [Engine/Include/Engine/Render/MeshData.h:8](../../Engine/Include/Engine/Render/MeshData.h) |
| `MeshLodData` | `—` | [Engine/Include/Engine/Render/MeshData.h:16](../../Engine/Include/Engine/Render/MeshData.h) |
| `MeshBounds` | `—` | [Engine/Include/Engine/Render/MeshData.h:23](../../Engine/Include/Engine/Render/MeshData.h) |
| `ConstantBufferData` | `—` | [Engine/Include/Engine/Render/ShaderConstants.h:12](../../Engine/Include/Engine/Render/ShaderConstants.h) |
| `InstConstantBufferData` | `—` | [Engine/Include/Engine/Render/ShaderConstants.h:46](../../Engine/Include/Engine/Render/ShaderConstants.h) |
| `InstData` | `—` | [Engine/Include/Engine/Render/ShaderConstants.h:67](../../Engine/Include/Engine/Render/ShaderConstants.h) |
| `ShadowPassBufferData` | `—` | [Engine/Include/Engine/Render/ShaderConstants.h:76](../../Engine/Include/Engine/Render/ShaderConstants.h) |
| `FrameConstants` | `—` | [Engine/Include/Engine/Render/ShaderConstants.h:86](../../Engine/Include/Engine/Render/ShaderConstants.h) |
| `BakedClipInfo` | `—` | [Engine/Include/Engine/Render/SkeletalBaker.h:14](../../Engine/Include/Engine/Render/SkeletalBaker.h) |
| `BakedAnimationTexture` | `—` | [Engine/Include/Engine/Render/SkeletalBaker.h:29](../../Engine/Include/Engine/Render/SkeletalBaker.h) |
| `SkeletalBakeResult` | `—` | [Engine/Include/Engine/Render/SkeletalBaker.h:49](../../Engine/Include/Engine/Render/SkeletalBaker.h) |
| `SkinnedVertex` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:18](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `CrowdSkinnedVertex` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:39](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `Bone` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:106](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `Skeleton` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:117](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `VectorKey` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:134](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `QuatKey` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:140](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `NodeAnimationChannel` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:146](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `AnimationClip` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:155](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `SkeletalMesh` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:163](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `SkeletalModel` | `—` | [Engine/Include/Engine/Render/SkeletalData.h:169](../../Engine/Include/Engine/Render/SkeletalData.h) |
| `SkeletalLoadResult` | `—` | [Engine/Include/Engine/Render/SkeletalLoader.h:8](../../Engine/Include/Engine/Render/SkeletalLoader.h) |
| `VirtualFileInfo` | `—` | [Engine/Include/Engine/Resources/VirtualFileSystem.h:23](../../Engine/Include/Engine/Resources/VirtualFileSystem.h) |
| `MountPoint` | `—` | [Engine/Include/Engine/Resources/VirtualFileSystem.h:39](../../Engine/Include/Engine/Resources/VirtualFileSystem.h) |
| `EnginePointLightApi` | `—` | [Engine/Include/Engine/Runtime/EditExtensions.h:13](../../Engine/Include/Engine/Runtime/EditExtensions.h) |
| `EngineDocumentCapabilities` | `—` | [Engine/Include/Engine/Runtime/EditExtensions.h:20](../../Engine/Include/Engine/Runtime/EditExtensions.h) |
| `EngineEditSessionApi` | `—` | [Engine/Include/Engine/Runtime/EditSessionApi.h:10](../../Engine/Include/Engine/Runtime/EditSessionApi.h) |
| `EngineEditModuleApi` | `—` | [Engine/Include/Engine/Runtime/EditSessionApi.h:40](../../Engine/Include/Engine/Runtime/EditSessionApi.h) |
| `EngineGameSceneApi` | `—` | [Engine/Include/Engine/Runtime/GameSceneApi.h:7](../../Engine/Include/Engine/Runtime/GameSceneApi.h) |
| `EngineGameWindowApi` | `—` | [Engine/Include/Engine/Runtime/GameWindowApi.h:6](../../Engine/Include/Engine/Runtime/GameWindowApi.h) |
| `RuntimeLaunchOptions` | `—` | [Engine/Include/Engine/Runtime/LaunchOptions.h:9](../../Engine/Include/Engine/Runtime/LaunchOptions.h) |
| `EngineLegacyV4::Object` | `—` | [Engine/Include/Engine/Runtime/LegacyEditApi.h:7](../../Engine/Include/Engine/Runtime/LegacyEditApi.h) |
| `EngineLegacyV4::SceneApi` | `—` | [Engine/Include/Engine/Runtime/LegacyEditApi.h:14](../../Engine/Include/Engine/Runtime/LegacyEditApi.h) |
| `EngineLegacyV4::EditApi` | `—` | [Engine/Include/Engine/Runtime/LegacyEditApi.h:24](../../Engine/Include/Engine/Runtime/LegacyEditApi.h) |
| `EngineLegacyV4::Factory` | `—` | [Engine/Include/Engine/Runtime/LegacyEditApi.h:44](../../Engine/Include/Engine/Runtime/LegacyEditApi.h) |
| `EngineLegacyV4::VisitBridge` | `—` | [Engine/Include/Engine/Runtime/LegacyEditApi.h:55](../../Engine/Include/Engine/Runtime/LegacyEditApi.h) |
| `EngineModuleHost` | `—` | [Engine/Include/Engine/Runtime/ModuleApi.h:13](../../Engine/Include/Engine/Runtime/ModuleApi.h) |
| `EngineModuleApi` | `—` | [Engine/Include/Engine/Runtime/ModuleApi.h:38](../../Engine/Include/Engine/Runtime/ModuleApi.h) |
| `EngineProjectDataApi` | `—` | [Engine/Include/Engine/Runtime/ProjectDataApi.h:5](../../Engine/Include/Engine/Runtime/ProjectDataApi.h) |
| `EngineSceneTransform` | `—` | [Engine/Include/Engine/Runtime/SceneModuleApi.h:15](../../Engine/Include/Engine/Runtime/SceneModuleApi.h) |
| `EngineSceneTypeInfo` | `—` | [Engine/Include/Engine/Runtime/SceneModuleApi.h:21](../../Engine/Include/Engine/Runtime/SceneModuleApi.h) |
| `EngineScenePointLight` | `—` | [Engine/Include/Engine/Runtime/SceneModuleApi.h:29](../../Engine/Include/Engine/Runtime/SceneModuleApi.h) |
| `EngineSceneObjectInfo` | `—` | [Engine/Include/Engine/Runtime/SceneModuleApi.h:36](../../Engine/Include/Engine/Runtime/SceneModuleApi.h) |
| `EngineSceneInfo` | `—` | [Engine/Include/Engine/Runtime/SceneModuleApi.h:49](../../Engine/Include/Engine/Runtime/SceneModuleApi.h) |
| `EngineSceneApi` | `—` | [Engine/Include/Engine/Runtime/SceneModuleApi.h:58](../../Engine/Include/Engine/Runtime/SceneModuleApi.h) |
| `EngineSelectionApi` | `—` | [Engine/Include/Engine/Runtime/SelectionApi.h:4](../../Engine/Include/Engine/Runtime/SelectionApi.h) |
| `EngineServiceHeader` | `—` | [Engine/Include/Engine/Runtime/VfsApi.h:42](../../Engine/Include/Engine/Runtime/VfsApi.h) |
| `EngineVfsStat` | `—` | [Engine/Include/Engine/Runtime/VfsApi.h:48](../../Engine/Include/Engine/Runtime/VfsApi.h) |
| `EngineVfsApi` | `—` | [Engine/Include/Engine/Runtime/VfsApi.h:88](../../Engine/Include/Engine/Runtime/VfsApi.h) |
| `AddComponentResult` | `—` | [Engine/Include/Engine/Scene/Scene.h:24](../../Engine/Include/Engine/Scene/Scene.h) |
| `SceneTransform` | `—` | [Engine/Include/Engine/Scene/Scene.h:29](../../Engine/Include/Engine/Scene/Scene.h) |
| `SceneComponentSnapshot` | `—` | [Engine/Include/Engine/Scene/Scene.h:35](../../Engine/Include/Engine/Scene/Scene.h) |
| `SceneComponentSnapshot::PointLight` | `—` | [Engine/Include/Engine/Scene/Scene.h:44](../../Engine/Include/Engine/Scene/Scene.h) |
| `SceneSnapshot` | `—` | [Engine/Include/Engine/Scene/Scene.h:54](../../Engine/Include/Engine/Scene/Scene.h) |

## Классы и типы текущих примеров игр

Примеры Game/BasicExamples, Pong, SunGame, Battle, Katamari сохраняют существующий запуск. Их классы не копируются в пользовательский проект и автоматически не регистрируются в его каталоге. Архив Game/Archive и тестовые классы исключены.

| Тип | База | Видимость в новом пользовательском проекте | Применение | Источник |
|---|---|---|---|---|
| `BasicExamplesPreview` | `public OrderedSceneGame` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/BasicExamples/Source/EditorPreview.h:9](../../Game/BasicExamples/Source/EditorPreview.h) |
| `BattleBenchSession` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Battle/Source/BattleBench.h:14](../../Game/Battle/Source/BattleBench.h) |
| `BattleGame` | `public Game` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/Battle/Source/BattleGame.h:17](../../Game/Battle/Source/BattleGame.h) |
| `Settings` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Battle/Source/BattleGame.h:20](../../Game/Battle/Source/BattleGame.h) |
| `UnitBatch` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Battle/Source/BattleGame.h:71](../../Game/Battle/Source/BattleGame.h) |
| `Box` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Battle/Source/UnitMesh.h:16](../../Game/Battle/Source/UnitMesh.h) |
| `SmokeGame` | `public T` | Нет | Внутренняя обвязка / данные примера | [Game/Common/ExampleStartup.h:32](../../Game/Common/ExampleStartup.h) |
| `OrderedSceneGame` | `public Game` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/Common/OrderedSceneGame.h:5](../../Game/Common/OrderedSceneGame.h) |
| `PongPreview` | `public PongGame` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/Common/PreviewScenes.cpp:10](../../Game/Common/PreviewScenes.cpp) |
| `SunPreview` | `public SunGame` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/Common/PreviewScenes.cpp:20](../../Game/Common/PreviewScenes.cpp) |
| `BattlePreview` | `public BattleGame` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/Common/PreviewScenes.cpp:36](../../Game/Common/PreviewScenes.cpp) |
| `FBXComponentCollision` | `public FBXComponent` | Нет | Компонент примера; нужен перенос исходников и ENGINE_COMPONENT | [Game/Katamari/Source/FBXComponentWithCollision.h:7](../../Game/Katamari/Source/FBXComponentWithCollision.h) |
| `KatamariGame` | `public Game` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/Katamari/Source/KatamariGame.h:5](../../Game/Katamari/Source/KatamariGame.h) |
| `CollectibleObject` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Katamari/Source/KatamatiSphereComponent.h:8](../../Game/Katamari/Source/KatamatiSphereComponent.h) |
| `AttachedObject` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Katamari/Source/KatamatiSphereComponent.h:18](../../Game/Katamari/Source/KatamatiSphereComponent.h) |
| `KatamatiSphereComponent` | `public GameComponent` | Нет | Компонент примера; нужен перенос исходников и ENGINE_COMPONENT | [Game/Katamari/Source/KatamatiSphereComponent.h:26](../../Game/Katamari/Source/KatamatiSphereComponent.h) |
| `GameEntry` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Launcher/Source/Catalog.h:4](../../Game/Launcher/Source/Catalog.h) |
| `ComGuard` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/Launcher/Source/Main.cpp:435](../../Game/Launcher/Source/Main.cpp) |
| `BallComponent` | `public CubeComponent` | Нет | Компонент примера; нужен перенос исходников и ENGINE_COMPONENT | [Game/Pong/Source/Ball.h:5](../../Game/Pong/Source/Ball.h) |
| `PongGame` | `public Game` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/Pong/Source/PongGame.h:5](../../Game/Pong/Source/PongGame.h) |
| `RocketComponent` | `public CubeComponent` | Нет | Компонент примера; нужен перенос исходников и ENGINE_COMPONENT | [Game/Pong/Source/Rocket.h:12](../../Game/Pong/Source/Rocket.h) |
| `Wall` | `public CubeComponent` | Нет | Компонент примера; нужен перенос исходников и ENGINE_COMPONENT | [Game/Pong/Source/Wall.h:11](../../Game/Pong/Source/Wall.h) |
| `Mover` | `public GameComponent` | Нет | Компонент примера; нужен перенос исходников и ENGINE_COMPONENT | [Game/RuntimeExample/Source/Mover.h:8](../../Game/RuntimeExample/Source/Mover.h) |
| `MovingCube` | `public CubeComponent` | Нет | Компонент примера; нужен перенос исходников и ENGINE_COMPONENT | [Game/RuntimeExample/Source/MovingCube.h:8](../../Game/RuntimeExample/Source/MovingCube.h) |
| `SceneShaders` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/SunGame/Source/SunGame.cpp:16](../../Game/SunGame/Source/SunGame.cpp) |
| `PlanetData` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/SunGame/Source/SunGame.cpp:73](../../Game/SunGame/Source/SunGame.cpp) |
| `ConstellationEdge` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/SunGame/Source/SunGame.cpp:356](../../Game/SunGame/Source/SunGame.cpp) |
| `SunGame` | `public Game` | Нет | Game-наследник примера; нужен перенос исходников и ENGINE_GAME для выбора в своём проекте | [Game/SunGame/Source/SunGame.h:7](../../Game/SunGame/Source/SunGame.h) |
| `SunGameConfig` | `—` | Нет | Внутренняя обвязка / данные примера | [Game/SunGame/Source/SunGameConfig.h:5](../../Game/SunGame/Source/SunGameConfig.h) |

## Внутренние определения Engine

Определения в Engine/Source и Engine/Runtime не входят в SDK Include. Пользователь не видит их в редакторе и не должен подключать приватные исходники для создания игры.

| Тип | База | Видимость / создание | Источник |
|---|---|---|---|
| `ScopedHandle` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Base/Base.cpp:33](../../Engine/Source/Base/Base.cpp) |
| `BaseEngine` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Base/Base.cpp:112](../../Engine/Source/Base/Base.cpp) |
| `ImpostorConstants` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Components/SpecificComponents/InstancedMeshInternal.h:22](../../Engine/Source/Components/SpecificComponents/InstancedMeshInternal.h) |
| `GpuClipInfo` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Components/SpecificComponents/InstancedMeshInternal.h:28](../../Engine/Source/Components/SpecificComponents/InstancedMeshInternal.h) |
| `GpuAnimClipBufferData` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Components/SpecificComponents/InstancedMeshInternal.h:36](../../Engine/Source/Components/SpecificComponents/InstancedMeshInternal.h) |
| `VS_IN` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/MainGame/BaseGameClass/Game.cpp:241](../../Engine/Source/MainGame/BaseGameClass/Game.cpp) |
| `VS_IN` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/MainGame/BaseGameClass/Game.cpp:246](../../Engine/Source/MainGame/BaseGameClass/Game.cpp) |
| `LoadedFaceData` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/MainGame/BaseGameClass/Game.cpp:1297](../../Engine/Source/MainGame/BaseGameClass/Game.cpp) |
| `SortedDraw` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/MainGame/BaseGameClass/Game.cpp:2938](../../Engine/Source/MainGame/BaseGameClass/Game.cpp) |
| `AssimpVfsStream` | `public Assimp::IOStream` | Внутренний; в редакторе недоступен | [Engine/Source/Render/AssimpVfsIO.h:10](../../Engine/Source/Render/AssimpVfsIO.h) |
| `AssimpVfsIO` | `public Assimp::IOSystem` | Внутренний; в редакторе недоступен | [Engine/Source/Render/AssimpVfsIO.h:40](../../Engine/Source/Render/AssimpVfsIO.h) |
| `ImpostorBakeConstants` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Render/ImpostorAtlas.cpp:12](../../Engine/Source/Render/ImpostorAtlas.cpp) |
| `VfsShaderInclude` | `public ID3DInclude` | Внутренний; в редакторе недоступен | [Engine/Source/Render/ShaderCompiler.cpp:39](../../Engine/Source/Render/ShaderCompiler.cpp) |
| `AllocContext` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Resources/HostVfsProxy.h:15](../../Engine/Source/Resources/HostVfsProxy.h) |
| `HostVfsProxy` | `public IVfs` | Внутренний; в редакторе недоступен | [Engine/Source/Resources/HostVfsProxy.h:53](../../Engine/Source/Resources/HostVfsProxy.h) |
| `IVfs` | `—` | Внутренний; в редакторе недоступен | [Engine/Source/Resources/IVfs.h:8](../../Engine/Source/Resources/IVfs.h) |
| `LocalVfs` | `public IVfs` | Внутренний; в редакторе недоступен | [Engine/Source/Resources/LocalVfs.h:6](../../Engine/Source/Resources/LocalVfs.h) |
| `ProjectJson` | `—` | Внутренний; в редакторе недоступен | [Engine/Runtime/Project.cpp:12](../../Engine/Runtime/Project.cpp) |
| `RuntimeProject` | `—` | Внутренний; в редакторе недоступен | [Engine/Runtime/Project.h:5](../../Engine/Runtime/Project.h) |
| `EngineRelease` | `—` | Внутренний; в редакторе недоступен | [Engine/Runtime/Project.h:17](../../Engine/Runtime/Project.h) |
| `Cleanup` | `—` | Внутренний; в редакторе недоступен | [Engine/Runtime/ProjectUpgrade.h:21](../../Engine/Runtime/ProjectUpgrade.h) |

## Сгенерированные пользовательские классы

| Класс | Родитель | Что видит пользователь |
|---|---|---|
| ProjectGame | Game | В списке Game; используется в Play и отдельной игре |
| ProjectComponent | GameComponent | В палитре; геометрию и логику задаёт автор |
| Класс мастера Component | Указанный компонент | В палитре после успешной сборки |
| Класс мастера Game | Указанный Game-наследник | В выборе игры после успешной сборки |
| Класс мастера Object | Любой допустимый родитель либо без базы | В палитре как [Object]; без геометрии |
| Неотмеченный пользовательский класс | Любой допустимый родитель | Только C++ код, в редакторе не отображается |

Список построен из production-исходников скриптом Engine/Tools/Write-TypeInventory.py. Таблица показывает объявления и фактический способ экспонирования; она не обещает, что произвольный служебный класс безопасно превращается в игровой объект без адаптации.
