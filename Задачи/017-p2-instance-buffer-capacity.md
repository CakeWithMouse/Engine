# P2. Создание инстансов постоянно перераспределяет GPU buffer

## Где

`SphereComponent::CreateSphereInstance`, `GameComponent::CreateInstanceBuffer/UpdateInstanceBuffer`.

## Проблема

Каждое добавление уничтожает buffer/SRV и создает их под точное новое количество. При последовательном росте до `N` это `N` операций создания и сумма запрашиваемых размеров, пропорциональная `1+2+...+N`. Во время draw создается временный `vector<InstData>`, заполняется и целиком копируется через `Map(WRITE_DISCARD)`.

## Исправление

Bulk creation/finalize для загрузки сцены, либо capacity с геометрическим ростом; постоянный staging vector с `reserve`; отдельная легкая instance record вместо полного `GameComponent` там, где индивидуальное поведение не требуется.

## Метрики

Время создания `N` экземпляров, число `CreateBuffer`, CPU time upload, bytes/frame.

