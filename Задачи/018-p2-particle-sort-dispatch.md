# P2. Полная bitonic-сортировка частиц дорогая по числу dispatch

## Где

`ParticleSystemComponent::DispatchSort` около строки 535; `GPUParticleSystem.hlsl`, `CSBuildSortKeys`, `CSBitonicSort`.

## Проблема

Для массива `M=2^k` вложенные циклы выполняют `k(k+1)/2` сортировочных dispatch на каждый render emitter, плюс один для ключей и один для симуляции. Для 10000 частиц буфер округляется до 16384: `k=14`, получается 105 sort dispatch, 107 compute dispatch вместе с keys/simulation.

Сейчас дополнительный вызов `Render` может симулировать emitter повторно, а невызванный `Render` останавливает его симуляцию.

## Исправление

Сначала сделать корректными общую transparency и depth policy. Затем измерить симуляцию, сортировку и particle fill отдельно. Для additive blending можно иметь явный материал без сортировки; для alpha transparency нужен порядок. Если сортировка доминирует - рассмотреть shared-memory block sort, межблочное объединение или radix-подход. Compute simulation перенести в отдельную фазу кадра.

## Метрики

GPU ms simulation/sort/draw, dispatch count, overdraw.

