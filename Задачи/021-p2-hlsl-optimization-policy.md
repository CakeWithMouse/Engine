# P2. HLSL всегда компилируется без оптимизации

## Где

`Game::CreateShader` около строки 1919; `ParticleSystemComponent::CompileShaders`.

## Проблема

Флаги `DEBUG | SKIP_OPTIMIZATION` не зависят от C++ configuration. Поэтому выбор Release сам по себе не дает оптимизированные runtime shaders. Shader cache есть только в памяти процесса; кроме того, FBX input layout компилирует вспомогательный VS на экземпляр при первом draw.

## Исправление

Явно разделить debug shader policy и optimized policy, включить policy в cache key; вынести input layouts в кеш по vertex declaration и shader signature. При необходимости добавить offline shader artifacts или disk cache.

## Критерий приемки

Release/optimized shader policy реально использует оптимизированные HLSL; после смены флагов изображение проверено на регрессии.

