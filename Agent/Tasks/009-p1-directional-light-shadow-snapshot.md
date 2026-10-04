# P1. Данные directional light и shadows обновляются несогласованно

## Где

`GameComponent::Update`, `Game::RenderShadowMaps/UpdateShadowCascades`, оба полных draw-пути.

## Проблема

В начале `Update` directional light заменяется значениями по умолчанию. Настройки из `Game` копируются обратно только внутри `IsShadowEnabled()`. Значит, отключение теней мешает обычным component buffers получать настроенный directional light. Также обычный `Update` компонентов выполняется до `UpdateShadowCascades`, а shadow pass может повторно обновлять только часть объектов.

## Исправление

Собрать порядок кадра: `camera -> world transforms -> lights/cascades -> frame constants -> draw packets`. Параметры directional light обновлять независимо от включения теней. Все проходы кадра должны читать один завершенный snapshot.

## Критерий приемки

Свет регулируется при выключенных тенях; все материалы кадра используют каскады того же кадра.

