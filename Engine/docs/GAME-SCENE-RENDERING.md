# Отображение startupScene в game mode

Этап 044: существующий renderer Game подключён к той же Scene, которую загрузил
GameSceneApi. Редактор и EngineProperty не изменялись; JSON остался версии 1.
Контракт загрузки: [GAME-STARTUP-SCENE.md](GAME-STARTUP-SCENE.md).

## Видимый пример

RuntimeExample содержит два каталожных класса:

- Mover — прежний компонент логики без геометрии.
- MovingCube — наследник CubeComponent, создающий CPU-геометрию в конструкторе
  и перемещающий куб по синусоиде в Tick.

`Game/RuntimeExample/Project.json` указывает
`Content/Scenes/Main.scene.json` с одним MovingCube. В start камера ставится в
(0,0,5), смотрит на начало координат. Геометрия и существующие шейдеры компонентов
используются напрямую; нового формата материала или меша в JSON нет.
Код конкретного куба находится в Game/RuntimeExample, не в ядре.

Сборка RuntimeExample копирует всё содержимое своего Content, включая JSON,
рядом с игровым DLL. Для запуска нужны собранный EngineRuntime и его EngineContent.
Путь EngineContent по умолчанию — рядом с exe, а не с DLL или Project.json.

```powershell
./Game/build/x64/Debug/Runtime/EngineRuntime.exe --project Projects/RuntimeExample/Project.json --mode game
```

Должно открыться окно с движущимся кубом. Enter не требуется. Закрытие окна или
Escape запрашивают завершение runtime. Для ограниченного smoke-прогона добавьте
`--frames 3`: выполнятся три host-кадра с dt=1/60.
Без startupScene RuntimeExample сохраняет прежний безоконный режим; другие игровые
DLL автоматически не переводятся на новый renderer.

## Владение и lifecycle

```text
create: Session → unique_ptr<Game> → Game.GetScene()
load:   JSON → candidate Game.GetScene() → commit
start:  Game.InitializeRuntimeWindow() → окно, device, shaders, camera
update: Game.RunRuntimeFrame(dt) → messages → resize → Tick → render → Present
stop:   остановка игровой сессии
destroy: компоненты → графические ресурсы → input/window → выгрузка DLL
```

Сессия больше не владеет отдельной Scene для логики рядом с Game. При загрузке
SceneDocument::Restore заполняет candidate->GetScene(); SceneCommands переключается
на неё через Rebind. После commit именно этот Game инициализируется в start.
Объекты не пересоздаются для отображения. На первом кадре SynchronizeSceneResources
привязывает Game и создаёт GPU-буферы/шейдеры для загруженных компонентов.

load не вызывает Tick и не создаёт окно. start создаёт графические ресурсы, но не
обновляет сцену. Единственный владелец симуляции в оконном режиме — кадровый путь
Game. В ModuleApi.update нельзя дополнительно вызывать Scene.Tick или StartGame.
RuntimeExample выставляет SimulationTimeScale=1, поэтому dt выражен в секундах
без старого игрового множителя 7.8. Старые приложения Game сохраняют свои настройки.

## Однокадровый API Game

```cpp
bool InitializeRuntimeWindow(bool visible = true);
enum class RuntimeFrameResult { Running, Closed, Failed };
RuntimeFrameResult RunRuntimeFrame(float deltaTime);
```

InitializeRuntimeWindow предназначен для собственного standalone-окна, которым
владеет Game. visible=false используется тестами для скрытого настоящего Win32/D3D
окна. Этот режим не меняет InitializeViewport и не запускает вложенный цикл Run.
При отсутствии шейдеров возвращается false с диагностикой без модального MessageBox.
Исключения своей инициализации автор DLL обязан перехватить на ABI-границе.

RunRuntimeFrame выполняет один кадр на потоке создания окна:

1. Обрабатывает Win32-сообщения; закрытие завершается до следующего Tick.
2. Применяет отложенный resize.
3. При минимизации пропускает симуляцию и render, делает короткую паузу 10 ms.
4. Ограничивает конечный неотрицательный dt верхней границей существующего Game.
5. Вызывает UpdateFrame ровно один раз, затем renderer и Present.
6. Завершает кадр InputDevice и обновляет статистику отрисовки.

Running — продолжить. Closed — вызвать host.requestExit и вернуть успех из update.
Failed — залогировать ошибку и вернуть 0. Исключения Tick/рендера также преобразуются
в отказ update. После Closed не продолжайте кадры: завершайте сессию.
Оконный pump читает очередь потока; один активный standalone Game на поток — текущий
сценарий. Несколько независимых игровых окон этим этапом не вводятся.

## Как runtime узнаёт о непрерывном режиме

Новый необязательный заголовок — `Engine/Runtime/GameWindowApi.h`:

```cpp
struct EngineGameWindowApi {
    uint32_t version; // ENGINE_GAME_WINDOW_API_VERSION = 1
    void* context;
    void* (__cdecl *getWindow)(void* context); // borrowed HWND, или nullptr
};
// Экспорт DLL, запрос после успешного start:
int __cdecl EngineGetGameWindowApi(uint32_t version, uint32_t outputSize,
                                 void* moduleSession, EngineGameWindowApi* output);
```

Экспорт C, __declspec(dllexport), стандартные проверки версии/размера/указателей,
output неизменен при отказе. Неполная или несовместимая таблица — ошибка запуска.
Ненулевой HWND должен быть действительным. Runtime запоминает непрерывный режим
после start и вызывает update без блокирующего чтения консоли.
Без экспорта или с nullptr остаётся прежнее управление кадрами консолью.
`--frames N` работает в обоих случаях. ModuleApi v4, GameSceneApi v1, SceneModuleApi
v1 и edit API v2 не менялись.

HWND заимствован: host не уничтожает его и не рисует второй кадр. Сообщения,
симуляция и render принадлежат update игрового модуля. Runtime лишь вызывает callback.
Нельзя уничтожать/заменять это окно во время работы, не завершив сессию.
После destroy и выгрузки DLL таблица и HWND недействительны.

## Закрытие, ошибки и повторный запуск

Для runtime-окон WM_CLOSE/Escape ставят флаг, а не уничтожают HWND немедленно.
update сообщает host об окончании; штатные stop/destroy освобождают компоненты и
GPU до уничтожения окна. При отказе start runtime вызывает destroy без stop.
Отказ frame приводит к обычному cleanup успешно запущенной сессии.

Display регистрирует оконный класс на модуль, содержащий WindowProc, и пытается
снять регистрацию при уничтожении принадлежащего ему окна. Это важно для DLL reload:
Windows не должен хранить адрес процедуры выгруженного модуля. Уничтожение окна
владельцем не посылает лишний WM_QUIT в следующий запуск. Embedded HWND не затронуты.
Неудачный startup не переключает игру в невидимый headless-режим.

## Проверки без редактора

`Engine/tests/edit-session/run.ps1`: 575 проверок, включая новый RuntimeRenderTests.h.
JSON восстанавливается до графики; проверяются GPU-буферы и DrawCalls, тождество
сцены логики/renderer, один Tick и dt, resize, минимизация/возврат, закрытие до Tick,
сохранение HWND до teardown, отсутствие остаточного WM_QUIT, отказ shader initialization,
повторное создание окна, настоящий игровой DLL, стартовый пример и непрерывный режим.
Регрессии: scene-module — 109; базовый набор — 13/13. Проверено Debug MSVC v143.
Тесты используют Win32/D3D11, Editor/Qt не собирались и не запускались.

Проверен настоящий EngineRuntime.exe с --mode game --frames 3: загрузка JSON, D3D11, три кадра, штатные stop/destroy, exit code 0. Runtime собран с BuildProjectReferences=false после сборки Engine/игры; Editor не собирался.
