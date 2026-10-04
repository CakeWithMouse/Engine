# P1/P2. FBX cache имеет скрытые предусловия

## Где

`FBXComponent::LoadModel/CreateMeshBuffers`, статические `modelCache/textureCache`.

## Проблема

Кеш модели сохраняет GPU buffers, counts и bounds, а путь до создания буферов зависит от уже назначенного `GamePtr`. Если загрузить модель без устройства, можно опубликовать запись с пустыми GPU pointers. Последующий cache hit создает render records без исходных CPU vertices, поэтому обычное позднее создание буферов уже не имеет достаточных данных. Ключ также не включает device.

## Исправление

Разделить CPU imported asset и device-specific GPU resource либо запретить публикацию GPU cache до полного успеха. Нормализовать путь до канонического asset identity. Не считать mutex достаточным контрактом для immediate-context операций.

## Критерий приемки

Cache hit всегда возвращает валидный ресурс для текущего device; загрузка без готового device не публикует неполную cache-запись.

