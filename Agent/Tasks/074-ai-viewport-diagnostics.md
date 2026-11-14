# 074 — AI-диагностика во вьюпорте

Выполнено 2026-11-14. Кнопка AI над вьюпортом открывает панель с общим выключателем и восемью независимыми слоями: сетка, стоимость, clearance, пути агентов, flow field, восприятие, строй, пробный A* A→B. Есть выбор контроллера, автоматический/ручной радиус, координаты и выбор A/B мышью, высота плоскости, область отображения, x-ray, информация о клетке и Debugger::Describe.

В Edit используется отдельный World с теми же правилами bake NavVolume, без запуска контроллеров. Изменение объёмов инвалидирует preview. В Play/Pause читаются действующие контроллеры и поле группы. Данные и геометрия остаются на scene thread; Qt получает JSON через необязательное расширение Engine.Scene.AiDiagnostics v1. Frozen edit ABI не изменён. Геометрией владеет Game; она объединяется с остальными overlay.

Проверено: Debug EngineRuntime/Editor, диагностический Game.dll, EditorUiTests --ai-diagnostics; 52/52 AiTests. UI-проверка охватывает версию расширения, Edit без контроллеров, rebake после перемещения NavVolume, найденный A*, переход Play/Pause/Stop, устойчивость списка при обновлении, действующее поле и слоты группы, выключение слоёв. Реальные снимки Qt/D3D-редактора получены через Windows Graphics Capture.

Отчёт в репозитории: [HTML с 11 реальными скриншотами](../../Engine/docs/reports/ai-diagnostics.html). Локальные артефакты: O:/Engine/Artifacts/AiDiagnostics/report.html, соседние JPEG и тестовая сцена. Логи: O:/Engine/Artifacts/ai-diagnostics-final-ui.log, ai-regression-tests.log, ai-final-runtime-build.log, ai-final-module-build.log, ai-ui-final2-build.log.

Ограничения: текущая навигация в плоскости XZ; поле Play относится к выбранной группе. Отрисовка сетки прореживается примерно до 6000 клеток, линии ограничены 50000; A* ограничен 30000 узлами. Нагрузочное тестирование огромных карт не выполнялось. Воспроизведение: Engine/tests/ai/diagnostics/README.md.

Интеграция в main (база 87a9cf8): сохранены новые таблицы расширений при hot reload. Повторно прошли Debug EngineRuntime/Editor с Jolt и Assimp, диагностический модуль, EditorUiTests --ai-diagnostics и 56/56 AiTests. Логи O:/Engine/Artifacts/ai-main-*.log. HTML фиксирует исходную съёмку и результат 52/52 на её версии.
