# Legacy-исходники лабораторных работ

Новая рабочая точка входа в репозиторий — `../Engine.sln`. Этот каталог больше не является самостоятельным собираемым приложением.

Здесь сохранены Pong/Katamari, `FirstSemestrLegasy`, исторический `StartMain.cpp` и старые проекты VS. `BaseEngine`, `BaseResources` и `BaseGameConfig` перенесены в `../Engine/Include/Engine/Base` и `../Engine/Source/Base` как основа движка. Staged-изменения не сбрасывались. Старые проекты и StartMain ссылаются на прежнее расположение/API ядра: не используйте их для новой сборки.

Рабочие общие исходники, GLM/stb и шейдеры перенесены в `../Engine`. Класс SunGame находится в `../Game/SunGame/Source/SunGame.h`; его построение сцены извлечено из `FirstSemestrLegasy::ThirdLabStart` в `SunGame.cpp`. Историческое тело ThirdLabStart сохранено здесь для сравнения с пользовательским рефакторингом.

Оригиналы моделей и текстур остаются здесь; рабочая копия obj/jpg для Katamari находится в `../Game/Katamari/Content/Models`. SunGame их не использует. Перенос и результаты проверок отслеживаются в `../Задачи/029-games-and-launcher.md`.

Пять сцен восстановлены в Game/BasicExamples, Pong, SunGame и Katamari (задача 029). Исходный FirstSemestrLegasy и игровые классы здесь сохранены для сравнения; сборка использует только новые проекты. GameExample теперь находится в Game/SunGame.
