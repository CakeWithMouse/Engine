# P1. Прозрачность не имеет полноценного прохода

## Где

`SunGame::DrawForward/DrawDeffered`, `Game::CreateDepthBuffer/CreateBlendStates`, `ParticleSystemComponent::Render`.

## Проблема

Forward перемешивает opaque/transparent в порядке имен. Deferred отделяет прозрачные объекты, но не сортирует их. Для прозрачности меняется только blend state; depth writes не отключаются. Частицы сортируются только внутри своего emitter, поэтому это не решает смешивание между системами частиц и другими объектами.

## Исправление

Opaque pass завершать до transparency; сортировать прозрачные draw packets от дальних к ближним, оставить depth test и выключить depth write. Skybox должен получить явный контракт глубины вместо обычного куба со стандартной записью depth.

## Критерий приемки

Прозрачные поверхности не скрывают последующие поверхности только из-за своей записи depth; порядок opaque объектов не влияет на depth collision частиц.

