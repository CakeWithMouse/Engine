# P1. Resize объявлен, но не реализован

## Где

`Display::WindowProc`, ветка `WM_SIZE`; `Game.h` с объявлениями resize-related методов; `Game::InitDeferredResources`; `Player::UpdateProjectionMatrix`.

## Проблема

`WM_SIZE` читает client rect и больше ничего не делает. Размеры `Display` остаются прежними; back buffer/depth не пересоздаются, `ResizeBuffers` в рендер-коде отсутствует. Проверка размеров deferred texture не помогает, потому что получает старые значения из `Display`.

## Исправление

Сделать единый `OnResize`: отложить изменение до безопасной границы кадра, освободить bindings/views back buffer, пересоздать swap-chain-dependent ресурсы, обновить viewport и projection. Отдельно обработать minimize/нулевой размер. Создание окна привести к контракту client area.

## Критерий приемки

После resize совпадают client area, back buffer, depth, G-buffer и aspect камеры.

