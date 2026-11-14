# Проблемы r5

Исходник 55d3b0c / 2026.11.06.1; исправления 2026.11.07.0, Windows x64 Release v143.

## BUG-R5-001 — GPU теряет TANGENT (P1, defect, исправлен)

Сценарий: MeshPrimitives::Plane → MeshComponent → material Surface читает i.Tangent.w.
Ожидание: нормальный UV-базис имеет abs(w)=1. Факт: w=0, используется derivative
fallback. Input layout описывал TANGENT, но MeshSignatureShader его не объявлял.
Это скрывало собственно ошибку зеркального масштаба и отключало новый vertex basis.

Координатор воспроизвёл настоящим WARP-рендером: material выдаёт magenta при отсутствии
тангента. До исправления `original != 0xffff00ffu` падает. После дополнения входной
сигнатуры атрибут доходит до пиксельного шейдера. Влияние: статические Mesh/FBX и
неанимированные инстансы с материалом. Обход: производные UV; это не эквивалент
авторскому вершинному базису. Regression: MaterialMirroredNormalMap, EngineTests.
Свидетельства: fixes/logs/tangent-before.log, fixes/logs/core-after.log.

## BUG-R5-002 — отражение инвертирует бинормаль (P2, defect, исправлен)

Найден ролью 1 (calm-city), подтверждён координатором после устранения BUG-R5-001.
Сценарий: тот же материал возвращает normal-map sample вдоль +V; scale=(-1,1,1).
Ожидание: отражение X не меняет мировой +V. Факт: цвет центрального пикселя меняется,
поскольку преобразованный tangent сохраняет старый w. До правки падает
`draw({-1,1,1}) == original`. Исправление: умножить w на знак determinant world matrix
в обычном и instanced варианте. Проверены также (-2,1,0.5) и отражение Z.
Обход: не использовать отражение масштаба в normal-mapped моделях.
Свидетельства: fixes/logs/mirror-before.log, fixes/logs/core-after.log.

## BUG-R5-003 — commandlet clone теряет тень лампы (P2, defect, исправлен)

Найден ролью 2 (curious-city). `project.scene`: duplicate Warm Light с
`castShadows=true` → save → new → load. Копия сохранялась без `castShadows` и
загружалась с false; у оригинала флаг оставался true. Сам SceneDocument работал
правильно. Причина: commandlet getService не возвращал PointLightV2, поэтому
SceneCommandHandler использовал старый setter без флага. Обычный runtime
передаёт это расширение: ошибка штатного GUI этим сценарием не установлена.

Исправлен адаптер сервисов EngineCommandlets. До правки регрессия
PointLightCloneCommandletTests падает именно на Warm Light 2; после — сохраняет
true у обоих объектов до и после повторного открытия. Это проверка данных,
визуальная тень копии отдельно не измерялась. Логи: fixes/logs/point-light-before2.log
и fixes/logs/point-light-after.log; исходный пользовательский сценарий в curious-city.

## ENV-R5-001 — MSVC PDB в sandbox (environment-blocker, обход проверен)

Сборка ядра C1902, проектные линковки LNK1101 MSPDB140.DLL. Повтор тех же сборок вне
песочницы успешен. Это не дефект Engine/SDK; установка VS не менялась. Первые два
отказа сохранения .mat в core-before исчезли при переносе TEMP в scratch внутри
workspace. Итоговый core-after: 60/60 без исключения тестов.

## Ошибки и ограничения прогона

- Перенос Source с вложенной Source/Source у curious-city: ошибка подготовки проекта,
  диагностированная ECG001; не дефект генератора.
- Попытка старой закреплённой .06.1 сцены на текущем .07.0 runtime: корректный отказ.
  Дальнейший путь — новая копия/пересборка, исходные проекты сохранены.
- Два максимальных shadow point lights, radius >1.5, отсутствие tangent у skinned
  форматов и отдельный legacy BaseSphereFigure — документированные ограничения.
- Дрожание каскадов при движении камеры, анимированные point-shadow casters, все
  аппаратные GPU и все пользовательские сценарии не считаются проверенными.

Пожелания ролей сохранены в feedback.md; они не объявлены дефектами движка.
