# P2. Нет общего отсечения видимости и группировки draw packets

## Где

Обходы `Components` в полном renderer, `Game::RenderShadowMaps`, `FBXComponent::Render`, `Game::Run` rasterizer.

## Проблема

Каждый проход снова обходит реестр. Все подходящие объекты отправляются в draw без frustum/occlusion culling; shadow повторяет это для трех каскадов. Кеш FBX экономит хранение геометрии, но повторные модели все равно рисуются отдельно по submesh. Основной rasterizer использует `D3D11_CULL_NONE`.

## Исправление

Ввести render packets с bounds и flags; построить отдельные видимые списки для камеры и каскадов; opaque сортировать по pipeline/material/mesh, применять instancing для совместимых повторяющихся мешей. Culling включать после проверки winding и double-sided materials.

## Метрики

CPU submit time, draw calls, rendered/culled objects, triangles и shadow draws.

