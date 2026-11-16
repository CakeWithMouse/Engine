# Проблемы r6

База d74aebb / Engine 2026.11.07.6, Windows x64 Release v143, Qt 6.7, Assimp.

## BUG-R6-001 — Две точки в имени модели отклоняются как traversal (P2, defect, исправлен)

Найден review curious-city и координатора. `ModelImportSidecar::WriteSidecar` для
`/Game/Models/Hero..v2.glb` отвергал имя как выход за проект. Ожидание: обычное имя
файла, масштаб 0.0254 сохраняется; факт: false и ошибка sandbox, новый объект
теряет выбранный масштаб. Причина: поиск подстроки `..` вместо сегмента пути.
Исправление сохраняет отказ настоящего сегмента `..`, включая native separators.
До: `fixes/logs/core-before.log`, FAIL Dotted model name accepts import settings.
После: CoreTests 478; `ImportSettingsCommandletTests.py` выполняет настоящий импорт
Hero..v2.obj → spawn → reimport → spawn → move → spawn, все три scale=0.0254.
Workaround до исправления: переименовать исходный файл без двух соседних точек.

## BUG-R6-002 — Неверный explicit scale импортирует модель до отказа sidecar (P2, defect, исправлен)

Найден review координатора, подтверждён curious-city. `Assets.Import` с source и
scale=-1/0/строкой/значением за диапазоном принимал команду: проверка выполнялась
лишь при записи sidecar после импорта. Ожидание: отказ до изменения Content.
Причина: QVariant::toDouble без проверки conversion/range/finite.
Исправление: валидировать командный параметр до отправки ImportModel, записать
диагностику и вернуть false. Обычный spinbox уже ограничивал диапазон.
До: `fixes/logs/ui-before.log`, Invalid explicit scale rejects import before mutation.
После: UiTests (включая NaN/Infinity), шесть отдельных командлетных отказов без
нового bundle и изменения scene. Проверен и default scale=1.
Workaround до исправления: передавать только конечный масштаб 0.0001–10000.

## ENV-R6-001 — MSVC C1902 в sandbox (environment-blocker, обход подтверждён)

Первая All сборка в ограниченном окружении не смогла использовать PDB-сервис.
Лог: `fixes/logs/build-all.log`. Повтор той же сборки в обычном окружении успешен,
`build-all-host.log`. Это не ошибка исходников и не повод подменять DLL старой.

## BUG-R6-003 — Play сообщает игровому коду выключенную симуляцию (P1, defect, исправлен)

Найден tired-bomber при реальном project.input: 130 кадров Play вызвали PreUpdate,
но guarded игровой код `if (!IsSimulationEnabled()) return` не двигал юниты и
не выпускал бомбу. Та же DLL в standalone попадала по цели за 1.95 с.
Причина: SceneEditSession оставлял флаг false, чтобы render не повторял ручной
шаг симуляции. Это внутреннее решение нарушало публичный игровой контракт.
До: `tired-bomber/evidence/project-input-prefx`; независимая engine регрессия
`fixes/logs/simulation-before.log` падает на Play reports simulation enabled.
Исправление отделяет выбор автоматического шага в RenderViewportFrame от
состояния симуляции Game. Play/Resume true, Pause/ошибка тика false; editor host
рендерит с advanceSimulation=false после своего шага. Старый одноаргументный
RenderViewportFrame сохранён. Тест проверяет guarded Game, ровно один тик,
Pause/Resume/Stop с настоящим viewport и без него. Требуется обновить SDK и
пересобрать пользовательские DLL. Повтор исходного запроса роли на новой DLL:
150 кадров exit 0, bomb release t=1.008, HIT unit id=7 t=1.936;
юнит переместился, за 10 paused-кадров смещение 0, после Resume — 0.05343.
Standalone и Save/load вторым процессом также повторены успешно.

## CLEAN-R6-001 — Пустой случайный каталог (local cleanup, выполнено)

Удалён только пустой E:/Engine/.exe после проверки абсолютного пути и отсутствия
содержимого. Исторические build/playtests и исходные локальные изменения r5 сохранены.

## BUG-R6-004 — Пустая схема свойств выдаёт внутреннюю ошибку JSON (P2, defect, исправлен и повторён)

Найден curious-city в продолжении: project.scene, объект Primitive.Cube Market,
property `/transform/position` со значением `[99,99,99]`. У компонента нет
DescribeEditorProperties; describe штатно возвращает `{}`, но setProperty
обращался к отсутствующему fields. Факт: EngineSceneFailure и
`[json.exception.out_of_range.403] key 'fields' not found`; ожидание — штатный
отказ `Property is not editable`, без изменения сцены.

Причина исправлена в SceneAuthoring.cpp: пустая схема отклоняется до чтения fields.
Контракт describe `{}` сохранён. Регрессия в AuthoringTests проверяет сообщение
и неизменность объекта. До исправления: 69/70 EngineTests, единственный FAIL на
ожидаемом сообщении; после: 70/70, без skipped.
Логи `Engine/build/playtests/20261114-r6/continuation-authoring-before-tests.log`
и `continuation-authoring-after-tests.log`. Исходный запрос роли сохранён в
curious-city/continuation. Runtime/SDK обновлены. Curious-city повторил исходный
запрос на новой DLL: штатный отказ без изменения позиции, Save/startup работают.
Tired-bomber независимо проверил Cube и Camera: status 6 и штатное сообщение.

## BUG-R6-005 — Normal map меняет UV-направление на обратной стороне (P2, defect, исправлен)

Найден новым render.mesh и независимо подтверждён curious-city. Настройки:
twoSided=true, plane, material выводит NormalFromMap для V-axis normal map в цвет;
сравнить scale [1,1,1], [-2,1,.5], [1,1,-1]. Отражение X должно сохранять мировой
+V, отражение Z — менять его знак. До: X менял blue 0→255, Z оставлял 0.
После: X сохраняет 0, Z меняет на 255; SaveGame возвращает тот же кадр.

MATERIAL_TWO_SIDED переворачивал Normal на back face, но не handedness тангента.
Теперь знак w меняется вместе с Normal, чтобы cross(N,T)*w сохранял UV-направление.
Регрессия выделена отдельно от прежней односторонней: до 70/71 EngineTests,
после 71/71. MeshDiagnosticsTests: 8 сценариев, 16 GPU-кадров в Forward/Deferred,
обычные/отражённые UV, неравномерный scale и SaveGame, плюс импорт texture/audio.
Результат исходного запроса curious-city подтверждён после обновления шейдера;
логи/кадры в continuation evidence. Прежняя односторонняя регрессия проходит.

## BUG-R6-006 — Переполнение текстур графа повторно занимает t127 (P2, defect, исправлен)

Calm-city отправил граф с 122 уникальными Texture2D, все с slot=2147483647.
Компилятор нашёл отсутствие свободного регистра, но после break всё равно
сгенерировал второй register(t127). PasteFromAi принимал граф до проверки
compiler problems. До: accepted=true, два разных имени привязаны к t127.

Компилятор теперь возвращает ошибку узла без HLSL при исчерпании регистров.
PasteFromAi проверяет ошибки до изменения модели/undo и передаёт конкретную
причину в диалог и commandlet. 121 уникальная текстура допустима (t6..t127,
кроме зарезервированного t32), 122-я отклоняется. CoreTests до исправления
падает на запрете aliased HLSL, после проходит. Настоящий Qt commandlet проверяет
обе границы, уникальность регистров, неизменность графа и Undo/Redo после отказа.
Роли независимо повторяют стресс с собственными входными данными;
свидетельства находятся в их continuation/evidence.json.

## BUG-R6-007 — Стандартное небо игнорирует назначенную cubemap (P2, defect, исправлен)

Calm-city назначил Studio и Space через публичный Sky.SetCubeMap, но реальные
кадры совпали: SceneProject::Sky использовал BasicSky.hlsl с постоянным градиентом.
Дополнительные ранние затруднения теста (камера, сырой SkyboxComponent и несколько
перекрывающих skybox) отделены от дефекта. Минимальная регрессия с одним стандартным
Sky воспроизвела именно игнорирование карты: 71/72, FAIL studio != gradient.

BasicSky теперь читает t1/s1 при ReflectionData.x > 0.5; без карты сохраняется
прежний градиент. После: 72/72 EngineTests, 0 skipped, разные Studio/Space и
побитовое восстановление исходного кадра после SetCubeMap(nullptr). ABI не менялся.
Runtime shader обновлён; роли повторяют собственные SDK-сценарии.
