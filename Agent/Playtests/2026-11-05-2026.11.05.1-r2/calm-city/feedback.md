# Отзыв — роль 1, «Спокойный город» (итог с backfill .3)

Чистый CLI-проект, включение C++, сборка и upgrade отдельной копии прошли штатно. В .3 я продолжил сценарий через горячую замену DLL: после `replaced=true` CarMover продолжил тикать, координата изменилась с x=-5.48 до -5.28. Сохранённый игровой раунд ранее также пережил отдельный процесс и восстановил маршрут машины.

Для планировки города `project.scene` оказался достаточно предметным: в одном проходе я переместил группу кварталов, назначил родителей, проверил Undo/Redo, создал и удалил временную площадь, продублировал машину с её пользовательскими данными и прошёл Save As → New → Load. Результат — 21 операция без отказов; значения координат и сериализованные поля доступны в JSON evidence.

Редактор материала проверен в настоящем Qt окне .3: AI paste, Undo/Redo, Apply и Save изменяли граф, а Revert восстанавливал исходный материал. Отдельно проверен отказ записи sidecar: ошибка сообщается, dirty остаётся, Revert и последующий Save восстанавливают рабочее состояние. Графовые PNG показывают форму и canvas; native D3D preview из Qt grab не виден. Полный native-window UI suite на свежем проекте завершился с 5029 checks passed; важные кадры инспектора материала, графовой ошибки, directional light и HUD приложены.

Новый D3D A/B пробник выявил важное различие API. `MeshComponent::CreateInstance` отрисовал три отдельные копии в ожидаемых позициях, а bare `CubeComponent::CreateInstance` без материала и `SetPrimitive` дал неверно залитую поверхность. Это кандидат на дефект legacy primitive rendering; воспроизводимые кадры и логи находятся в `evidence/r3/`, описание и source path — в `issues.md`. Отчёт не смешивает его с исправлением `InstancedMeshComponent` в .3.

Автогенерация LOD уменьшила сферу с 528 до 264 треугольников при нулевом silhouette drift; панель показывает 8/8 видимых инстансов. Остались отдельные проверки импорта GLB/BLEND/текстур и reimport, particle/collision, render modes/resize/host и scene recovery. Покрытие указано по каждой строке evidence; статус not-tested означает, что сценарий не запускался.

Recovery now has a real native UI restart check: the workflow stored an unsaved checkpoint before simulated process termination, and a fresh process restored all seven objects and matched the module identity. The class wizard's validation behavior was also exercised for invalid names, Character defaults and Object arguments; I have not claimed that every class kind was created through UI.

The legacy primitive CreateInstance A/B result is recorded as BUG-R2-006: bare CubeComponent produced a bad filled surface while the MeshComponent control produced three separated copies. Root independently reproduced it and is preparing the .4 fix; the attached .3 frames are the before evidence and the project contains no workaround.

Root's .4 fix for BUG-R2-006 passed my independent fresh-project A/B: I rebuilt the same no-material CubeComponent path against .4 and ran it beside the MeshComponent control, at the same three coordinates and camera. Both rendered three separated silhouettes. The .3 failing BMP stays attached as before evidence, and the .4 retest frames as post-fix evidence.
