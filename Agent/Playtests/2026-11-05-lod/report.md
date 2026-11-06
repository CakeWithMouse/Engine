# Статистика кадра и LOD — 2026-11-05

Версия SDK 2026.11.05.1. Реализованы снимок завершённого render кадра и готовая панель
диагностики для standalone/embedded. Движок не содержит механики бомбардировки.
Снимок отражает CPU submissions до Present, без собственной панели; это не GPU timing.

## Проверки координатора

- RenderDiagnosticsTests: 74 проверки, embedded и standalone D3D. PreUpdate видит предыдущий
  завершённый кадр при сброшенных текущих Stats. Проверены начальный valid=false, LOD0→LOD1,
  причины hidden/distance/frustum, fade с двумя bucket/одним экземпляром, Deferred/Custom,
  пустой mesh и устаревшие данные, выбор компонента, пауза/minimize/resize/удаление,
  сброс шейдерного кэша при видимой панели.
- Фактические BMP панели 640×480 и 320×240 осмотрены. На узком viewport текст обрезается
  с многоточием, по высоте есть явная подсказка; панель не выходит за края.
- SaveGame: 73 проверки и чтение новым процессом прошли.
- ViewportTests: PASS GameSettings/standalone/embedded/FXAA/resize/render modes/close isolation,
  animated pixels/minimize/reattach/borrowed HWND.
- Launcher/Runtime/SDK собраны с Assimp; библиотека SDK совпадает с Engine.lib по SHA256.
- API Reference: 39 страниц, 306 API entries, 86 headers; build --check прошёл.

Логи в Engine/build/render-diagnostics-tests.log, lod-viewport-tests.log,
lod-savegame-regression.log, lod-runtime-build.log; компактные копии лежат рядом с отчётом.
Новых ошибок компиляции координаторской реализации не было. Первые standalone assertions LOD
не прошли из-за тестовой камеры: SetViewDirection задаёт матрицу, но свободная камера
с InputDevice обновляет её по Euler-углам. Fixture теперь согласует Euler и направление,
обнуляет фоновый MouseOffset. Контракт LOD/движок ради теста не изменялся.
При финальном review обнаружено: новый DiagnosticsHud не участвовал в OnShadersInvalidated,
поэтому мог сохранять старые resolved shader slots. Добавлен сброс рядом с ProjectHud
и повторный render после InvalidateAllShaders в обоих вариантах теста; финальный SDK пересобран.

## Проверка разработчика

Запущен один явно запрошенный агент GPT-6-Luna / medium: bomber_diagnostics.
Изолированная копия прежней игры: Engine/build/playtests/lod-2026-11-05/tired-bomber.
Общие SDK/Runtime обновляет только координатор. Дневник, feedback, исходники и кадры
находятся в tired-bomber/.

Финальная сборка проекта и повтор на окончательном SDK прошли: 220 runtime frames, exit 0,
stderr пуст. В PreUpdate кадра 60 доступен снимок 59 с DrawCalls=8; в кадре 110 — снимок
109 с DrawCalls=9. На Y=24: total=13, drawn=11, hidden=1, frustum=1, distance=0,
perLOD=(0,0,11). На Y=8: те же причины отсечения, perLOD=(0,11,0). Кадры обеих
дистанций осмотрены координатором и агентом; текст читается, текущий LOD виден.

Агент прямо подтвердил достаточность для двух исходных вопросов — нулевая статистика
в PreUpdate и непонятные 11/13 закрыты. [Отзыв](tired-bomber/feedback.md),
[машинные свидетельства](tired-bomber/evidence.json), [исходники](tired-bomber/source.zip).
Остались необязательные UX-пожелания: компактный режим панели, подпись о составе диапазона
дистанций, интервалы LOD вместо чтения соседних порогов. F7/F8 вручную не проверялись;
панель включалась и выбиралась через публичный API. Это не скрытое подтверждение GUI-ввода.

Первая сборка агента встретила известный sandbox LNK1101/MSPDB140; повтор того же
Build.ps1 в штатном окружении прошёл. Новых воспроизводимых движковых дефектов в финальном
прогоне агент не обнаружил. Исходная библиотека и SDK после финального shader-cache fix
совпадают: SHA256 B5D6DEF134FC5A54B039646383D2BBABE5660C56D5D53AF42459F6A0AB6CAB7E.

## Бюджет и ограничения

Начальный счётчик недельного окна 26%, после основных проверок 28%; он общий с другими чатами.
Кредиты сброса не использовались. Панель показывает существующий CPU выбор LOD,
не новую систему LOD или occlusion query. Она выключена по умолчанию. F7/F8 документированы.
Ручной GUI-ввод не заявляется проверенным на основании вызова API.
