# Функция → вызов → контракт → проверка

| Функция/BUG | Вызов | Наблюдаемый результат | Регрессия |
|---|---|---|---|
| BUG-R6-001 | project.import source=Hero..v2.obj scale=.0254 | файл настроек существует, масштаб каждого объекта .0254 | CoreTests + ImportSettingsCommandletTests |
| BUG-R6-002 | project.import с неверным scale | accepted=false, нет нового bundle/объекта | UiTests + ImportSettingsCommandletTests |
| Импорт/переимпорт/перенос | project.import reimportSource/moveTo | старые и новые объекты сохраняют scale | ImportSettingsCommandletTests |
| Материалы | material.graph / render.point-shadows | параметры, rollback, пиксели, тени | MaterialGraphCommandletTests / MaterialRenderTests |
| Точечные тени | render.point-shadows | движение depth cube, слоты ламп, LOD | PointShadowMotionTests / PointShadowSlotsLodTests |
| Прозрачность | render.transparency | source-over, пересечения, анимация, resize | TransparencyTests / TransparencyIntersectionTests |
| Совместимость | frozen fixtures + upgraded projects | старый ABI, C++ rebuild, загрузка сцены | CompatibilityTests + четыре роли |
| Остальной код | штатные наборы Engine/Editor | контракты lifecycle, input, scene, save, assets, codegen | run-regressions.py + UI suite |

Исторические логи не перезаписываются. Полный список запущенных команд, кодов
завершения и времени — regressions.json. Координаторские тесты не засчитываются
как независимый игровой опыт каждой персоны. Qt-layout PNG не доказывает D3D viewport.
