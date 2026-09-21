# P1. Ресурсы и объекты не имеют единого владельца

## Где

`Game::~Game`, пустой `DestroyResources`, `GameComponent` с default destructor, `SphereComponent::CreateSphereInstance`, `RegisterComponent`.

## Проблема

`Game` не удаляет компоненты из `Components`; экземпляры также не освобождаются владельцем. Базовый компонент хранит VB/IB/CB/SRV/sampler как raw COM pointers без общей очистки. У `Game` часть D3D-ресурсов в `ComPtr`, часть raw; destructor освобождает не весь набор. У `Game` есть виртуальные методы, а destructor не виртуальный.

В `CreateSphereInstance` вызов `GetDevice` увеличивает COM refcount, а соответствующий `Release` закомментирован.

## Исправление

`Game/Scene` владеет компонентами через `unique_ptr`, instance owner владеет экземплярами, `Parent` и light registry - observer references/handles. GPU-объекты перевести на `ComPtr`; разделяемые model/texture assets - на resource handles с явным владельцем. `Game` получает виртуальный destructor. Миграцию делать по подсистемам, чтобы не получить double release на разделяемых FBX-буферах.

## Критерий приемки

Создание/уничтожение сцены возвращает CPU/GPU resources к исходному набору; shutdown действительно вызывает этот путь.

