# Material graph probe

Изолированная копия `CalmCity/Content` под `GraphProbe`; исходный игровой материал не менялся. .7 `material.graph` запустил настоящий Qt граф и вернул `nativePreviewWindow=true`. Последовательность: enable → save → preset/apply → попытка отката при удерживаемом read-only sidecar → повторный revert → намеренно неверный AI graph → preset → undo/redo → revert. Результат exit 0; проверки последовательности и диалоги в `material-graph.result.json`, canvas в `material-graph.png`. Ошибка записи ожидаема: dirty-граф удержался и восстановление состоялось после освобождения файла.

# Импортированная тень A/B

Обе сцены рендерились тем же frozen .7, Runtime, камерой и размером 1000×500. В `ShadowOn.scene.json` слот 1 — импортированный `Material_1.mat`; в `ShadowOff.scene.json` он назначен на копию с тем же `Imported.hlsl` и `render.castShadows=false`. Геометрия/трансформация одинаковы. Оба процесса exit 0. В D3D submission overlay index count изменился 83472 → 65760. Прямое сравнение BMP с исключением панели overlay выявило 35 различающихся пикселей в области под импортированной моделью (x=448..512, y=222..252); это подтверждает малый видимый shadow effect. Тень остаётся маленькой в масштабе общего кадра, поэтому визуальный приоритет — partial в общем shadows feature, passed для imported-shadows.

# Completed frame stats

При начальном заходе в `PreUpdate` метрика ожидаемо ещё недействительна: frame 0. В следующем callback после завершённого кадра она стала valid=1/frame=1 и дальше росла до frame=7. Исходное условие `valid` успешно создало LodPanel.log и кадр. Это пользовательский лог первого кадра, не defect.
