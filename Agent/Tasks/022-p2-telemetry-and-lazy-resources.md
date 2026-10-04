# P2. Лишние постоянные расходы и неполная телеметрия

## Где

`Game::Initialize`, `RenderDeferredDebugOverlay`, `Run`, `EndFrame`, `Update`.

## Проблема

G-buffer и shadow maps создаются независимо от использования. Три shadow maps по 2048x2048x4 байта - 48 MiB хранения depth без учета накладных расходов. Overlay всегда добавляет четыре draws. Есть повторные clear back buffer/depth между `Run` и draw. `Present(1,0)` может ограничивать видимый FPS частотой дисплея, а `Objects` не считает скрытые экземпляры и submesh.

## Исправление

Ленивое создание ресурсов по включенным возможностям, переключаемый overlay, один владелец clears, счетчики draw/triangles/instances/dispatches и отдельные CPU/GPU timings. Синхронизацию Present сделать управляемой для измерений.

## Метрики

Память ресурсов по включенным фичам, draw/dispatch counters, CPU/GPU timings, влияние overlay и Present mode.

