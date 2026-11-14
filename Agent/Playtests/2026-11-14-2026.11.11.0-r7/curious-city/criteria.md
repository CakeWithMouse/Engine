# Curious City r7 — критерии независимого закрытия

R7 practical runs and exact partial boundaries are recorded in evidence.json and continuation.md. The table defines each closure criterion.

## 32 legacy-функции

| feature_id | Что должно быть наблюдено в своём сценарии | План собственного опыта |
|---|---|---|
| `scene-create-delete` | Идентичность и число объектов меняются после Create/Delete; Undo удалённого объекта возвращает тот же объект, Redo удаляет снова. | Один named cube, удалить, проверить snapshot/selection; Undo/Redo. |
| `scene-hierarchy` | У ребёнка правильный parent, локальная поза не меняется при назначении; вычисленная мировая позиция после parent transform совпадает с ожидаемой. | Ребёнок в parent с ненулевым переносом/поворотом/масштабом; сверить local и world через runtime/API readback. |
| `scene-transform` | Перенос, поворот и неравномерный масштаб меняют ожидаемые координаты; Undo/Redo восстанавливает обе версии; Save/Load сохраняет итог. | Собственный трансформированный house/lot с точными значениями до и после. |
| `scene-codec` | Сериализованные объекты, иерархия, transform, assets, `projectData`, script/FSM ссылки переживают process/session Save→New→Load. | Сцена с custom house, model/material slot и AI/script компонентами; сравнить JSON и загруженный snapshot. |
| `scene-clone` | Клон имеет иной ID, сохраняет тип/данные; каждый объектный reference, который указывает на исходный объект внутри клонируемого набора, указывает на соответствующий клон. | Два связанных объекта с реальным object-reference; отдельно проверить поведение одинарного и группового clone. |
| `scene-select` | Viewport click picks the exact object; Ctrl-toggle and hierarchy share selection; Focus centers the selected item. | Two separated objects, click/Ctrl/hierarchy/Focus with result IDs and camera snapshot. Marquee is not implemented in this release. |
| `time` | Масштаб времени 0/1/2 задаёт измеримый игровой прогресс; Pause не меняет время/положение, Resume продолжает, Stop возвращает сессию. | Дом с перемещением/доходом на известном dt, замерить реальное `simulationTime`/counter. |
| `input` | Игра получает нажатие, удержание, отпускание клавиши и pointer click с координатой; потеря фокуса блокирует игровое действие и отпускание не зависает. | Build-on-click + key counter с Play, focus loss/regain и результатами каждого шага. |
| `camera-perspective` | Изменение pitch/yaw/FOV и viewport aspect меняет фактический screen ray/pixels по ожидаемому направлению; resize обновляет aspect. | Перспективная камера, target-marker и две точки raycast; сравнить при повороте/FOV/resize. |
| `trace-world` | Raycast попадает в настоящий world object и возвращает верные ID/позицию/нормаль/дистанцию; контрольный miss возвращает Miss. | Луч по двору через стены/пол и луч в пустой участок; сравнить формулу пересечения. |
| `trace-filter` | Ignore id и каналы исключают ближайший объект и попадают в следующий; без фильтра ближайший объект выигрывает. | Два перекрытых коллайдера: исключить первый по ID и каналом. |
| `trace-debug` | Вызов DebugDrawTrace формирует сегменты по результату; кадр из игрового render hook показывает сегмент; Clear снимает его. | Луч Hit и Miss, frame до/во время/после Clear; считать линии и осмотреть non-empty PNG/BMP. |
| `hud-project` | Выбранный UI-файл из Project Settings действительно загружается в Play; смена файла меняет видимый HUD, повторный запуск сохраняет выбор. | Два небольших .ui с уникальными подписями, переключение в настройке и два кадра. |
| `hud-elements` | Реальный UI Editor добавляет/меняет Label и Button, сохраняет XML; в игре видны текст и реакция на клик. | Новый элемент с уникальным label и button, клики подтверждают callback/scene snapshot. |
| `hud-layout` | Anchor/rect/padding/resize меняют layout предсказуемо на двух aspect ratio, без clip/наложения текста. | Одинаковый HUD на 640x480 и 1280x720 с сохранёнными кадрами и измеренными bbox. |
| `compatibility` | Старый проект/модуль открывает новый runtime либо предлагает обновить копию; upgraded copy сохраняет исходники/Content и собирается на новом SDK; оригинал hash не меняется. | Сравнить исходный r6 project hash до/после; обновить отдельную copy и проверить build/run. |
| `model-import` | Реальный source asset появляется в Content, импортированная геометрия/материал открывается и объект ставится с заданным scale; malformed asset получает ошибку без частичной папки. | Собственный small BLEND/OBJ source и broken-file negative. |
| `skeletal` | Скелет/анимация реально загружается; pose на t=0/.5/1 меняет render, повтор t=0 возвращает тот же pose/hash. | Маленький skeletal fixture, actual renderer timestamps, сохранённые кадры. |
| `instances` | Несколько instance transforms дают ожидаемые видимые копии и счетчики; изменение/скрытие одной не портит другие. | Группа из трёх различимых домов/маркеров, собственный instance count и кадр. |
| `lod` | Качество/LOD переключается при заданном screen size/distance; transition и возврат к ближней точке воспроизводимы. | Один imported/static model с threshold boundary: ниже, ровно на/выше и возврат; capture actual render. |
| `material-params` | Редактирование .mat параметра меняет фактический pixel output; revert возвращает начальный pixel/hash. | Материал на собственном mesh, scalar/color A/B и кадры. |
| `material-editor` | Настоящее окно редактора меняет параметр/texture/shader и сохраняет файл; re-open восстанавливает значение, Preview не пустой. | Открыть проектный материал, сменить tint и save/reopen, capture non-empty preview. |
| `material-blends` | Opaque/Masked/Translucent материалы дают разные прозрачность, depth-write/cutout/overlap на реальных пересечениях. | Три панели с тестовой alpha texture, несколько углов и pixels. |
| `light-point` | PointLight изменяет освещённость по расстоянию/радиусу; intensity=0 не светит, второй свет меняет область без влияния отключённого. | Собственная сцена с двумя lights и несколькими probe surfaces; кадры + значения. |
| `shadows` | Кастер затемняет receiver; выключение света/теней убирает реальную тень; движение света меняет её и возврат восстанавливает. | Cube над плоскостью, actual render readback/кадры в двух режимах. |
| `material-graph` | Граф через окно добавляет/связывает ноды и переходы данных; apply меняет shader/pixel, undo/redo/reopen восстанавливают файлы и внешний вид. | Свой graph с Color/Scalar/Multiply; compare emitted HLSL, saved JSON and rendered pixel. |
| `material-slots` | Импорт модели с ≥2 материалами сохраняет оба slot IDs/assignments и разные видимые submeshes. | BLEND с контрастными цветами на двух субмешах, render and scene snapshot. |
| `material-slot-edit` | В настоящем Inspector конкретный slot меняется; Undo/Redo отражается в UI/scene/file и возвращается после reopen. | Выбрать imported model, изменить именно widget slot 1, не общим override. |
| `material-slot-persist` | Пер-slot assignment переживает Save Scene, отдельное New/Load и игру; визуальный submesh сохраняет новый материал. | Сцена до/после persisted override, close/reopen process и кадр. |
| `imported-shadows` | Импортированный GLB/BLEND caster бросает тень на отдельный receiver, а `castShadows=false` снимает её. | Импортированный mesh над тестовой плоскостью, PNG/BMP A/B. |
| `imported-uv` | Импортированный UV-chart с импортированной BaseMap виден на размещённом объекте; разные chart-квадранты соответствуют текстуре. | Собственная цветная checker/chart текстура; top/front frame, inspect RGB области. |
| `material-graph-current` | Проектный WarmStone проходит enable/save/preset/apply/undo/redo/revert и reopen; итоговый graph/HLSL/hash правильный, preview пиксели сняты. | Собственный текущий материал из копии city, не общий регрессионный runner. |

## Новые AI-пункты (по два независимых прогона)

- `ai-card-memory`: в настоящей карточке создать/назначить FSM, задать override `Speed` и `PostB` только одному персонажу; Undo/Redo, ↺ reset, duplicate, Save→New→Load сохраняют/изолируют память.
- `ai-graph-edit`: настоящие `AiMachineEditorWindow`/`FsmGraphWidget`: состояния, два перехода и их trigger/condition, `Ai.Wait` task, переменная с rename-ссылок, target/lastSeen/thinkInterval, Undo/Redo, закрытие/reopen и exact `.fsm` JSON.
- `ai-validation`: через окно внести конкретно ошибочный block/key и сохранить; карточка получает ошибку с id ноды, Play не стартует; исправить тот же граф, проверка очищается, Play стартует и live text показывает состояние.
- `ai-script-link`: FSM block `Ai.CallScriptEvent` зовёт `Noise` собственного `.vscript`; событие меняет видимый игровой эффект/позицию/счёт. Обратный вызов `OnCaught` также проверяется на наблюдаемом результате.

Плановые пункты требуют реального UI/runtime результата; бинарники будут запущены только после READY.
