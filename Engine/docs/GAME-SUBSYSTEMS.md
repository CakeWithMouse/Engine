# Игровые подсистемы

`GameSubsystem` хранит данные одной игровой сессии. Каждый помеченный класс
создаётся ровно один раз на сессию. Смена сцены сохраняет экземпляры; Stop
уничтожает их, следующий Play создаёт новые. Edit и preview материалов
не создают подсистемы. Hot reload не переносит их состояние.

Создайте заголовок в `Source` игрового проекта:

```cpp
#pragma once
#include <Engine/MainGame/GameSubsystem.h>

ENGINE_SUBSYSTEM()
class SessionCounterSubsystem final : public GameSubsystem {
public:
    unsigned scenesOpened = 0, ticks = 0;
    SessionCounterSubsystem() { bTickEnabled = true; }
    void OnSceneReady(Scene&) override { ++scenesOpened; }
    void Tick(float) override { ++ticks; }
};
```

Получение из игрового кода:

```cpp
if (auto* counter = game.GetSubsystem<SessionCounterSubsystem>()) {
    counter->bTickEnabled = false;
}
```

Указатель заимствован, действителен до Stop; владение не передаётся.
Поле `bTickEnabled` по умолчанию `false`, проверяется перед каждым Tick.
Меняйте его только в игровом потоке. Параметров у `ENGINE_SUBSYSTEM()` нет.

## Генерация и SDK

`ComponentGenerator` сканирует заголовки исходных корней при каждой обычной
сборке, включая добавленные и удалённые файлы. В одном модуле только один
владелец генерации: `ComponentGeneration.targets` в репозитории либо
`ProjectSdk.targets` в установленном SDK. Фабрики подсистем входят в тот же
`GeneratedComponents.cpp`. Ручной регистрации и глобальных C++ конструкторов нет.
Ключ — полное C++ имя класса, включая namespace; порядок — лексикографический
по ключу. Дубликаты отклоняются генератором и контейнером до создания объектов.

Поддерживаются обычные нешаблонные классы в глобальной области или именованных
namespace. Нужны публичное наследование от `GameSubsystem`, публичный конструктор
без аргументов и конкретный, не абстрактный класс. Условные объявления, вложенные
классы и параметры аннотации запрещены. Ограничения сканера совпадают с
[генерацией компонентов](COMPONENT-GENERATION.md).

Внешние C++ проекты используют публичные заголовки, `Engine.lib` и генератор
установленного SDK. Для старых потребителей без генерации MSVC x64 linker
подставляет пустой каталог через `/alternatename`. Это не механизм регистрации
и не singleton текущей игры. Один каталог на DLL/EXE; несколько независимо
сгенерированных каталогов в одном модуле не поддерживаются.

## Жизненный цикл

1. Проверка всего каталога, создание всех объектов, затем `Initialize` по ключу.
2. `Initialize(SubsystemContext&, std::string&)` не требует окна, GPU или сцены.
   `context.GetScene()` возвращает `nullptr`. Доступ к другим подсистемам через
   `GetSubsystem` во время Initialize возвращает `nullptr`; зависимостей пока нет.
3. `Game::InitializeGameplay` настраивает HUD и контроллер, вызывает
   `OnSceneReady`, затем `BeginPlay` компонентов. Повторная инициализация
   готовой сцены не повторяет уведомление.
4. На шаг симуляции после `ActionInput.Tick`, перед контроллером и компонентами
   вызывается один Tick. Передаётся уже рассчитанный gameplay dt. Отрисовка
   editor viewport не вызывает второй Tick. Pause сохраняет сессию без тиков.
5. Перед уничтожением сцены `OnSceneClosing` вызывается в обратном порядке.
   Подсистема должна освободить ссылки и подписки на эту сцену.
6. Stop запрещает обычные callbacks и тики, закрывает активную сцену, вызывает
   `Deinitialize() noexcept` в обратном порядке, затем уничтожает объекты
   в обратном порядке. Повторный Stop безопасен.

Если Initialize вернул false или бросил исключение, Deinitialize вызывается
для всех объектов, чей Initialize уже начался, включая неудачный объект.
Остальные созданные объекты только уничтожаются. Ошибка OnSceneReady закрывает
уже уведомлённые объекты и останавливает сессию. Исключения OnSceneClosing
не препятствуют очистке; сессия останавливается. Deinitialize обязан не бросать.

Stop из callback (через `context.session.Stop()`) откладывает освобождение до
возврата callback. Следующий обычный callback уже не вызывается. Повторный
Stop из Deinitialize безопасен. Реентерабельный Stop editor API также завершает
уничтожение Play после возврата текущего API-вызова. Уничтожать сам `Game` или
`GameSession` непосредственно из callback запрещено.

Исключение из Tick выходит в существующую обработку симуляции: editor сообщает
`EngineSceneSimulationFailed` и переходит в Pause, сохраняя сессию. После Resume
можно продолжить. Runtime сообщает ошибку кадра и завершает игру.

## Смена сцены внутри одной сессии

Минимальный внутренний путь — внешний `GameSession` и последовательные `Game`:

```cpp
GameSession session;
std::string error;
if (!session.Initialize(error)) throw std::runtime_error(error);
{
    Game first;
    first.AttachSession(session);
    // Наполнить first.GetScene(). Окно/GPU не обязательны.
    first.InitializeGameplay();
    first.StepProjectSimulation(0.016f);
} // OnSceneClosing, но подсистемы живы
{
    Game second;
    second.AttachSession(session);
    second.InitializeGameplay(); // те же подсистемы, новая сцена
}
session.Stop();
```

Между сценами Tick пропускается. Одновременно активна только одна сцена;
переходы выполняются между callbacks в потоке владельца. Внешняя сессия должна
пережить использующие её Game. Самостоятельный Game без внешнего владельца
создаёт собственную сессию при запуске gameplay. В редакторе владелец —
SceneEditSession; в игровом DLL — модульная Session. Объекты и фабрики остаются
в DLL, host получает только существующий C ABI. Очистка происходит до FreeLibrary.

UI переходов, граф зависимостей и перенос состояния при hot reload в MVP отсутствуют.
Пример находится в `Game/BasicExamples/Source/SessionCounterSubsystem.h`.
Проверки: `Engine/tests/subsystems/run.ps1` и `run-generator.ps1`.

## Воспроизведение проверок

Из корня репозитория:

```powershell
Engine/tests/subsystems/run.ps1 -Configuration Release
Engine/tests/subsystems/run-generator.ps1 -Configuration Release
Engine/tests/subsystems/run-sdk.ps1 -Configuration Release
```

Первый скрипт собирает тесты и генератор, проверяет безоконный Play и D3D viewport. Второй проверяет сканер. Третий экспортирует SDK в отдельный тестовый каталог вне репозитория, собирает DLL и host, проверяет добавление/удаление класса, ожидаемый отказ некорректного наследования, откат запуска и очистку до выгрузки/повторной загрузки DLL. Для DLL-загрузки не требуется открывать редактор.
