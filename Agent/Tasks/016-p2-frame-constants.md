# P2. Общие данные кадра пересчитываются для каждого объекта

## Где

`GameComponent::Update` около строки 131; `FBXComponent::Render/RenderShadow`; `Game::RenderDeferredDebugOverlay`.

## Проблема

Каждый компонент инвертирует одинаковые view/projection matrices, очищает/копирует light arrays и вызывает `GetPointLights`, возвращающий новый `vector`. Затем это повторяется для экземпляров. FBX дополнительно вызывает `Update` в цветном draw, а shadow вызывает его по каскадам. Debug overlay четырежды готовит большие buffers с inverse matrices и lights.

## Исправление

Разделить `FrameConstants` (view/projection/inverse/camera/light/shadow), `ObjectConstants` (world/normal matrix/color), `MaterialConstants` и instance stream. Frame data формировать один раз после камеры/каскадов; на instance хранить только необходимое для конкретного draw. Shadow должен получать готовую world matrix, а не запускать весь `Update`.

## Метрики

CPU time подготовки, число `UpdateSubresource`, временные allocations и общий FPS как вторичный индикатор.

