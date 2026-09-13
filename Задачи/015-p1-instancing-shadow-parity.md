# P1. Цветной инстансинг и тени описывают разную геометрию

## Где

`GameComponent::Render`, `RenderShadow`, `GetBufferElementCount`, `CreateInstanceBuffer`.

## Проблема

При непустом массиве цветной проход рисует только элементы `InstanceArray`, без владельца. Shadow pass рисует один обычный `DrawIndexed` владельца, не используя instance buffer. Получается разный набор преобразований в цвете и тенях. Проверка `GetBufferElementCount() != InstanceArray.size()` не обнаруживает рассинхронизацию емкости, потому что getter возвращает тот же `InstanceArray.size()`.

## Исправление

Формализовать, входит ли владелец в список видимых экземпляров; использовать один instance transform stream во всех геометрических проходах, добавить shadow VS/путь для инстансов. Хранить фактическую GPU capacity и valid count раздельно.

## Критерий приемки

Каждый видимый opaque instance отбрасывает тень из своей world matrix; рост массива не может вызвать загрузку за пределы выделенного буфера.

