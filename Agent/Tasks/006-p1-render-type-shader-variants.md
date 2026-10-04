# P1. RenderType и shader variants могут расходиться

## Где

`Game::RegisterComponent`, `ShouldUseDeferredGeometryVariant`, `SetRenderingType`; fallback в `SunGame::DrawDeffered`.

## Проблема

Компоненту назначается одна пара VS/PS при регистрации. `SetRenderingType` меняет только enum. Поэтому переключение после регистрации оставляет старый вариант шейдера; fallback из deferred в forward тоже продолжает использовать G-buffer shaders на одном цветном target.

## Исправление

Хранить у материала варианты для проходов и выбирать нужный при submit/draw. Промежуточно можно запретить runtime-переключение после регистрации и явно проваливать неудачную инициализацию deferred, убрав ложный fallback. Для произвольного shader path проверять поддержку нужного варианта.

## Критерий приемки

Цветной target и shader output signature всегда соответствуют проходу; невозможный fallback выдает понятную ошибку.

