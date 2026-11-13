# Findings — Curious City r5

## BUG-R5-003 — `project.scene` clone теряет PointLight `castShadows`

- Категория: defect в commandlet host; воспроизводимость: одна попытка, exit 0; все шесть команд сцены вернули status 0.
- Приоритет: P2 (commandlet мог дать ложное подтверждение сохранения свойства; обычный editor UI не проверен).
- Конфигурация: PainEngine Runtime/SDK `2026.11.07.0`; `final-city` C++ проект; GPU-изображение не оценивалось.
- Шаги: в копии `Content/Scenes/Main.scene.json` выставить `properties.Engine.PointLight.value.castShadows=true` у `Warm Light` и `Cool Light`; выполнить `project.scene` с `duplicate(target='Warm Light')`, `save(ShadowProbe.scene.json)`, `delete(Cool Light)`, `new`, `load(ShadowProbe.scene.json)`, `delete(Warm Light)`.
- Ожидание: копия унаследует все свойства источника, включая `castShadows=true`; стандартный duplicate сдвигает её на +1 по X; Save/Load сохраняет этот флаг.
- Факт: исходные Warm/Cool источники сериализуются с `castShadows:true`; копия `Warm Light 2` имеет те же color/intensity/radius, положение `[3,3,2]` вместо `[2,3,2]`, но поле `castShadows` отсутствует. SceneDocument трактует отсутствие как `false`. Остальные операции, включая save/load и удаления, успешны.
- Локализация по координатору: `Editor/Tools/EngineCommandlets.cpp` `getService` не отдавал `EnginePointLightV2Service`; дублирование использовало legacy setter без флага. Штатный editor path не проверен, поэтому дефект нельзя распространять на него.
- Обход: для commandlet теста вручную выставить флаг после duplicate или использовать штатный editor path после отдельной проверки.
- Доказательства до исправления: `Engine/build/playtests/20261107-r5/curious-city/point-light.scene.request.json`, `point-light.scene.result.json`, `ShadowProbe.scene.json`; `Agent/Playtests/2026-11-07-2026.11.06.1-r5/curious-city/evidence/point-light.scene.exitcode.txt`.
- Статус: исправлено координатором, повторено на пересобранном EngineCommandlets. Точный запрос с новым путём `ShadowProbe-after.scene.json` завершился exit 0; все операции status 0; clone сохраняет `castShadows:true`, New/Load его восстанавливает, последующее удаление затрагивает только исходный Warm Light. Повторный результат: `Engine/build/playtests/20261107-r5/curious-city/point-light.scene.after.{request,result}.json`, `ShadowProbe-after.scene.json`, `Agent/Playtests/2026-11-07-2026.11.06.1-r5/curious-city/evidence/point-light.scene.after.exitcode.txt`.

## Средовые/собственные ошибки, не баги движка

- Первичный `--enable-code` sandbox-проход: CodeBuild.log содержит LNK1101 incorrect MSPDB140.DLL; эскалированный повтор на той же чистой заготовке прошёл exit 0.
- Первый перенос исходников вложил `Source/Source` и вызвал ECG001 duplicate component; исправлено только в собственной копии проекта, повторная сборка прошла.
- Первая миграция `.06.1`→`.07.0` была вызвана до сборки пользовательской DLL и сообщила `Unknown component type: CuriousCity.House`; после сборки классов повторный `--upgrade-project` прошёл. Ошибка процесса тестирования, не дефект формата/миграции.
