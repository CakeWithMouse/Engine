# Findings — Curious City r6

## Код-ревью импорта

До сборки был замечен кандидат на ложный отказ: `ModelImportSidecar::ResolveSidecarPath` проверял строку `contains("..")`, поэтому имя `Hero..v2.obj` выглядело как возможный отказ без traversal. Текущая сборка `Game/build/x64/Release/Runtime/EngineCommandlets.exe` приняла это имя, создала sidecar с scale 0.01 и сохранила масштаб при переимпорте и переносе. В собственном актуальном сценарии дефект не воспроизвёлся.

Параметры scale вне диапазона в пользовательском UI этого персонажа не проверялись; общий Core/UI/Host regression и негативные commandlet сценарии проверял координатор. Новых подтверждённых дефектов движка в роли не заведено.

## Уровень наблюдения

- `project.input` использовал реальную игровую DLL, реальные editor input callbacks и runtime объекты; сохранение/загрузка подтверждены логами и результатом объектов.
- `project.import`, `render.point-shadows`, `render.transparency` и `material.graph` выполнялись текущим собранным EngineCommandlets, а не чтением общих результатов suite.
- Для прозрачности renderer выдал warning о внешнем dynamic VFS mount HLSL, но вернул `ok=true`, exit 0 и кадры. Изображения пересечений сохранены.
- Материал подтвердил native preview HWND и редакторский граф, но PNG сделан Qt capture; он не захватил D3D preview swapchain.
- Изображения point-shadow/transparency созданы на NVIDIA GeForce RTX 4070 Laptop GPU. Desktop UI игры в этой роли не захватывался.
