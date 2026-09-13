# P1. Input layout зависит от предыдущего объекта

## Где

`GameComponent::Render`, `FBXComponent::Render/RenderShadow`, `Game::RenderShadowMaps`, `SunGame::DrawForward`.

## Проблема

FBX draw устанавливает собственный input layout, а обычный `GameComponent::Render` свой layout не устанавливает и ожидает, что он уже назначен снаружи. После FBX или shadow draw примитив может получить mesh/shadow layout предыдущего объекта. Порядок компонентов в `std::map` превращается в скрытую зависимость результата кадра.

## Исправление

Каждый draw packet должен явно определять layout и обязательное pipeline state, либо renderer должен гарантированно назначать их при переключении типов пакетов. Добавить явные состояния для opaque, transparent, skybox и shadow.

## Критерий приемки

Перестановка независимых opaque objects и изменение их имен не ломают vertex interpretation.

