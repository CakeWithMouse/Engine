# Проверка STRATEGY-016

| Контракт | Вызов | Наблюдаемый результат |
| --- | --- | --- |
| Сопровождение объекта | Group::MoveTo(ObjectRef), цель движется 3 с | Destination совпадает с текущей целью, отряд прибывает к ней |
| Навигация меняется во время сопровождения | NavGrid::SetCost вдали от пути | Отряд продолжает преследовать актуальную цель |
| Замена приказа | MoveTo(ObjectRef), затем MoveTo(point) | Прибытие к точке, движение старой цели не влияет |
| Удаление цели | Scene::RemoveComponent во время приказа | Unreachable, координаты бойца не меняются после остановки |
| Игра автора | project.input с r4 DLL | Один приказ сопровождает охранника; HP4→0, победа, после смерти атака остановлена; strategy/r4-attack-single-* |

Регрессия в Engine/tests/ai/Source/CrowdTests.cpp: GroupMovingTargetTests.
Запуск с фильтром:

```powershell
$env:PATH='E:\Engine\Engine\vcpkg_installed\x64-windows\bin;'+$env:PATH
./Engine/build/x64/Release/AiTests.exe 'group follows moving'
```

До правки: `moving-target-before.log`, 0/1. После: `ai-final.log`, 57/57 всего.
Сборка тестов: Invoke-MSBuild.ps1 с -Project Engine/tests/ai/AiTests.vcxproj,
-Configuration Release -PlatformToolset v143 -EnableAssimp.

Editor, commandlets, Runtime и SDK пересобраны, whole-archive link прошёл.
Актуальная Engine.lib SHA256:
`358E352C9E30FCCD6ED87D7FE54581FC7F3820BBCDD3E6B31CDE7D142626ABEE`.
