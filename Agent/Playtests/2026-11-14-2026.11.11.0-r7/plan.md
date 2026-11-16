# r7: AI-редактор и закрытие частичных проверок

Запрос: скачать новую разработку и получить минимум два passed на каждом пункте.
Ветки develop на origin нет; AI-редактор влит в main 6899a6a, версия 2026.11.11.0.
Координатор переносит локальные r6 fixes через сохранённый stash и побайтовый
backup Engine/build/playtests/20261114-r7-integration; старые отчёты не меняются.

Общий бюджет прежнего круга: baseline7%, начало этой фазы20%, stop-new32%, max37%,
codex/10080/reset1794928911. Не сбрасывать бюджет/лимит. Координатор контролирует.
2026-11-14, при счётчике29%: пользователь явно снял ограничение навыка ответом
«Продолжать до полного покрытия». Прежние stop-new32%/max37% больше не являются
условием остановки; исходный счётчик и дальнейший расход сохраняются в usage.json.

Каждый агент пишет только свои r7/<role> report и scratch
Engine/build/playtests/20261114-r7/<role>, использует старую игру как основу.
До READY нового Runtime/SDK не запускать игровые сборки/командлеты. Общие
Engine/Editor/SDK/Intermediate меняет только координатор. Чтение текущих новых
документов и подготовка собственных входов разрешены. Desktop только по очереди.

48 прежних partial нужно закрывать практическими наблюдениями, а не сменой
критерия или переименованием partial в passed. Сначала прочитать исходный evidence
и сформулировать недостающий критерий, затем собственный сценарий. Общие tests
координатора не являются независимой проверкой роли. Фикстуры/ошибки своего кода
исправлять; подозрения на Engine передавать с request/result/log/expected/actual.
Для аудио нужен наблюдаемый выход (например реальный loopback waveform и A/B
пространственного затухания), одних успешных handles недостаточно.

Пары:
- curious-city + tired-bomber: scene-create-delete, scene-hierarchy, scene-transform,
  scene-codec, scene-clone, scene-select, time, input, camera-perspective, trace-world,
  trace-filter, trace-debug, hud-project, hud-elements, hud-layout, compatibility.
- calm-city + curious-city: model-import, skeletal, instances, lod, material-params,
  material-editor, material-blends, light-point, shadows, material-graph, material-slots,
  material-slot-edit, material-slot-persist, imported-shadows, imported-uv, material-graph-current.
- calm-city + tired-bomber: render-modes, render-resize, diagnostics, savegame-scene,
  savegame-derived, completed-frame, lod-panel, point-shadow-cubes, shadow-cascade-fit,
  audio-play2d, audio-play3d, audio-stream, point-shadow-lod-transition,
  transparency-camera-sorting, transparency-intersections, transparency-animated-lod-resize.

AI-пункты (два независимых сценария каждый):
- ai-card-memory: назначить/создать FSM, override памяти одного персонажа, reset,
  Undo/Redo, clone и сохранение/повторное открытие. calm + curious.
- ai-graph-edit: реальные окна/состояния/переходы/память/settings, history и сохранение.
  calm + curious.
- ai-validation: ошибка с конкретной нодой, отказ Play, исправление и успешный Play.
  curious + bomber.
- ai-runtime-debug: переходы/активное состояние/память и видимое поведение во времени.
  calm + bomber.
- ai-script-link: событие Noise и CallScriptEvent/переменная со встречным игровым эффектом.
  curious + bomber.
- ai-file-reload: сохранение/повторное открытие, отказ записи/retry, изменение поведения
  после правки файла и повторного Play. calm + bomber.

Сдача каждой роли: evidence.json (unique feature_id, status, expected, actual,
evidence_paths отдельными существующими файлами, engineVersion), diary.md,
feedback.md, REBUILD.md, source-only archive без SDK/Binaries/Intermediate.
Изображения осматривать; пустой frame или code-only не являются visual pass.

После завершения своей основной роли агент calm-city отдельным followup выполнит
четвёртую роль ai-trash по навыку: изолированная копия r6 TrashGameUpgraded,
новый SDK, небольшой игровой цикл/сохранение и отзыв по новому AI-редактору.
Это отдельный проект/дневник/архив; он не заменяет назначенные выше независимые
пары разных агентов для 54 целевых строк.

После закрытия 40/54 строк оставшиеся проверки перераспределены: tired-bomber
после завершения основной порции отдельно проверит skeletal, lod и instances
в своём проекте как второй независимый проверяющий к calm-city. Curious-city
сосредоточится на остальных 11 строках. Его прежние partial сохраняются до
собственного подтверждения; две успешные роли не превращают третью попытку в passed.
После закрытия этих трёх строк tired-bomber также получил material-blends,
shadows, imported-shadows, imported-uv: curious-city подтвердил, что собственные
GPU-сценарии по ним ещё не запускал. Curious-city завершает camera-perspective,
trace-debug и hud-layout. Все четыре игровые роли прошли основной путь;
calm-city дополнительно делает read-only review пяти исправлений r7, не добавляя
этим статусов в матрицу.
