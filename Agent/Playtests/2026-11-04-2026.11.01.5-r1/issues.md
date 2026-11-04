# Журнал прогона 2026-11-04 / 2026.11.01.5

## ENV-001 — MSVC C1902 в sandbox
- Категория: environment-blocker; приоритет P1; координатор; Release x64/v143.
- Шаг: `Engine/build.ps1 -Configuration Release -Target EngineLauncher`.
- Ожидание: подготовка runtime/SDK; факт: массовые C1902 при компиляции Engine/Editor.
- Лог: `Engine/build/playtests/20261104-r1/prepare-build.log`.
- Повтор той же команды вне sandbox прошёл, лог `prepare-build-retry.log`. Исходники движка для этого не менялись. Статус: обойдён, вероятное ограничение окружения; не подтверждённый дефект движка.

## ENV-002 — Валидатор навыка не имел PyYAML
- Категория: environment-blocker; P3; координатор.
- `quick_validate.py` завершился ModuleNotFoundError: yaml. Sandbox pip не разрешил DNS.
- PyYAML установлен в изолированный `Engine/build/playtests/20261104-r1/python-deps` вне sandbox; штатный валидатор: `Skill is valid!`.
- Логи: `skill-validator-setup.log`, `skill-validator-setup-retry.log`. Статус: устранён в окружении; не дефект движка.

## BUG-001 — Поля Z обрезаются в исходном размере редактора
- Категория: defect/ui; P2; роль 3; Release x64/v143, RTX 4070 Laptop, окно 1282×752.
- Шаги: открыть новый проект, выбрать Cube в иерархии. Ожидание: доступны все X/Y/Z позиции, поворота и масштаба. Факт: правые поля Z и кнопки инспектора выходят за правый край; горизонтального доступа в снимке нет.
- Свидетельство: `Engine/build/playtests/20261104-r1/ui-evidence/bomber-selected-clipped-inspector.png`.
- Обход: развернуть окно, тогда все поля видны (`bomber-maximized.png`). Статус: открыт для навыка исправления.

## BUG-002 — Ошибка формирования таблицы Qt-стилей
- Категория: defect/ui; P2; роль 3; первый запуск редактора нового проекта.
- Ожидание: QSS с подставленными цветами/иконками. Факт: `QString::arg: 1 argument(s) missing` и `Could not parse application stylesheet`.
- Полный лог: `Engine/build/playtests/20261104-r1/tired-bomber/ui-editor-retry-error.log`.
- Участок: `Editor/UI/EditorStyles.cpp` блок QSpinBox/QComboBox начиная с 429: четыре различных placeholder (%1,%2,%3,%5), передано пять значений arg; подстановка отказала, в логе остались %1..%5. Это диагноз по коду и зарегистрированному сообщению; визуальный ущерб именно этой причине отдельно не изолирован.
- Повтор в live запуске не записал предупреждение. Статус: открыт; проверить первый запуск/тему и исправить отдельным навыком.
## BUG-003 — UI Save не применяет активное текстовое поле
- Категория: defect/ui; P2; роль2; редактор CityHud.ui.
- Шаги: выбрать Label status, заменить Income на Доход города в поле Текст, не нажимать Enter, нажать кнопку Сохранить на панели.
- Факт: строка состояния сообщает Сохранено, но preview и XML содержат старое Income; активное поле продолжает показывать новую строку. Ожидание: сохранить видимую правку либо явно указать, что она не применена.
- Enter применяет текст (появляется *), следующий Save записывает кириллицу в XML. Обход подтверждён. Screenshot curious-ui-editor-cyrillic.png показывает успешный обход; исходный факт подтверждён прямым чтением XML сразу после первой кнопки.
- Статус: открыт для отдельного навыка исправления.
## BUG-004 — Авария Qt после UI Editor и Ctrl+Alt+R
- Категория: defect/crash; P1; роль4; runtime Release.
- Последовательность: Play с бросками → Stop → UI Editor открыть Hud.ui, изменить кнопку, Save, закрыть → Ctrl+Alt+R. Editor исчез, manifest не изменился, exec завершился1.
- Windows Application Error 2026-11-04 13:33:08: EngineRuntime.exe, Qt6Gui.dll 6.7.2, 0xc0000005, offset 0x3f2601, report 6950f2e5-25bb-4e3d-ae3a-d99f5c2e4942.
- Лог: ai-trash/ui-editor.log обрывается без сообщения. В свежей сессии без UI Editor CtrlAltR не упал, а выдал BUG-005. Причина аварии не установлена; однократный факт, требуется повтор последовательности. Не считать подтверждённым диагнозом Hot Reload.

## BUG-005 — C++ сборка из редактора не находит Get-FileHash при запуске из PowerShell 7
- Категория: defect/build-blocker; P1; роль4. Windows PowerShell5.1, редактор запущен из PowerShell7 Codex shell.
- CtrlAltR в чистой сессии: ProjectTools.ps1 CommandNotFoundException Get-FileHash, exit1, прежняя DLL сохранена. Console русская диагностика повреждена кодировкой (отдельный UI недостаток).
- Повтор через System.Diagnostics.Process с унаследованным PSModulePath дал тот же отказ. При удалении PSModulePath из окружения дочернего процесса Get-Command Get-FileHash успешно находит Microsoft.PowerShell.Utility3.1. Прямой native launch из pwsh также успешен: он сам нормализует окружение.
- Исключение пользователя применено: Editor/Source/CodeTools.cpp задаёт дочернему Windows PowerShell окружение без PSModulePath, чтобы тот построил подходящие пути своих модулей. Прочие переменные сохраняются.
- Editor.dll Release v143 пересобран успешно; UI повтор после правки в работе. Свидетельства: ui-evidence/trash-build-powershell-error.txt/.png, ai-trash/build-windows-powershell.log, build-editor-powershell-fix.log.
## BUG-006 — Русская диагностика C++ повреждена в Console
- Категория: defect/ui; P2; роль4.
- Шаг: собрать игровой код с ошибкой C2665 из редактора. Compiler file/line/code читаются, русский текст приходит как РЅРё... либо OEM-мусор. WindowsPS использует разные кодировки stdout для компилятора и собственного error stream, CodeTools декодирует fromLocal8Bit.
- Свидетельство: ui-evidence/trash-ui-build-user-error.txt/.png; Binaries/Release/ed38389df95048f5871f76caaba12980/build.log (проект роли4). Статус: открыт для другого навыка; не препятствует сборке после исправления игрового кода.

### BUG-004 уточнение
Повторилось ещё в свежей сессии без UI Editor и через меню, exit -1073741819. Дамп 32108: Qt6Gui QAction::setEnabled, QMenu::changeEvent, recursive QWidgetPrivate::setEnabled_helper, EditorMainWindow::RunCodeTool. `dump-stack-scan.py` — скан candidate return addresses, не полный unwind; symbols+actual crash локализуют путь global setEnabled(false). Перенос QTimer0 не помог и отменён. Минимальная правка убирает global disable/enable, сохраняет sceneBusy guards/RefreshSceneUi и UpdatePlayButtons, добавляет busy guard для CodeTools. Первый повтор дошёл до обычной C2665 пользовательского кода, сохранил предыдущую DLL и редактор жив. Окончательный успешный build/reload проверяется после исправления user-code.
## BUG-007 — После успешного Hot Reload игровой ввод не действует до переоткрытия
- Категория: defect/input; P1; роль4.
- Шаги: build 5307... → исправить TrashProjectile source → CtrlAltR успешно активирует4c74..., окно то же → Play → несколько кликов Бросить и T во viewport.
- Факт: HUD остаётся0/0; три объекта не появляются. Игровой кадр и FPS обновляются. Stop, закрыть редактор exit0, открыть тот же manifest4c74... → Play → кнопка/T работают, Hits3/Throws3, короткая надпись корректна.
- Ожидание: после успешной горячей замены ввод работает как после reopen. Причина не установлена. Свидетельства: ui-evidence/trash-after-reload-input.png, trash-hotreload-fixed.txt, trash-final-hits3.png; ai-trash/ui-editor-busy-guards.log и ui-editor-final.log.
- Статус: открыт для следующего навыка; обход — переоткрыть редактор, исходники игрового кода между этими двумя запусками не менялись.

### BUG-004 и BUG-005 — итог проверки исключения
После удаления global setEnabled и очистки дочернего PSModulePath реальный CtrlAltR: failed user C2665 → сохранён прежний модуль → исправление user source → новая4c74a1b450764b5a9aaa2df6f3a1fc42 DLL compiled/validated/activated и Hot reload log в том же окне. Fresh reopen этой DLL дал3/3 успешных броска. Оба build-blocker исправлены; BUG007 остаётся отдельно.

## BUG-008 — Привязка игнорируется при переносе за центральный маркер
- Категория: defect/editor; P2; роль4, свежий Trash Direct Cpp.
- Включить магнит и шаг0.5; Cube из x0 перетащить за центр примерно120px: x=-1.6378729. Перенос за X-ось затем меняет x ровно на-1.5, до-3.1378729. Перемещение за центр обходит включённое округление.
- Ожидание: выбранный шаг применяется ко всем режимам переноса либо UI явно объясняет исключение. Причина подтверждается Editor/Source/ViewportWidget.cpp: ветка Center handle не проверяет snapEnabled_, в отличие от Axis/Plane.
- Свидетельства: ui-evidence/trash-snap-center-nonquantized.png, trash-snap-axis.png. Статус: открыт для отдельного навыка исправления.

### Уточнения окружения и UX
- CIM/Get-Process в sandbox не видят повышенный тестовый процесс; elevated read идентифицировал PID17396 по пути/заголовку. Это ограничение автоматизации, не баг движка.
- Стрелки gizmo появляются после фокусировки viewport, а при выборе из hierarchy сначала видна только рамка. Повторено роли1/4; записано как UX-наблюдение, не отдельный подтверждённый дефект.
- BUG005 также воспроизведён в Launcher Create C++ для роли2. Launcher RunProjectCodeTool теперь исключает PSModulePath из копии Unicode child environment. Повтор --enable-code роль2: exit0, DLL076dd01; свежая GUI Create C++ роль4: DLL25926d6 и открытая сцена. Engine Launcher/Editor собраны Release успешно.


### Итог артефактов и ограничений инструмента
- Итог: 8 дефектов; BUG004/005 исправлены, остальные шесть открыты. Последующий HotReload новой DLL города8d5c3a сохранил несохранённое здание и не упал.
- Валидатор Python сначала использовал Windows cp1251 и выдал UnicodeDecodeError на UTF-8 SKILL.md; повтор с `python -X utf8` прошёл. Аналогичная ошибка чтения coordinator JSON исправлена явным encoding=utf-8. Это ошибки процедуры/окружения.
- Предпроверка архивов выявила необходимость Project.code.json для SDK Build; файл добавлен во все итоговые архивы до передачи. Не дефект движка.
- Controller в ролях2/3: короткие W/D не дают движения, непрерывное удержание инструмент не поддерживает; B-событие роли3 подтверждено. Требуется отдельная проверка удержания, причина не установлена.
- Оставшийся без окна тестовый Bomber PID48680 закрыт после сверки executable/project; причина его состояния не исследовалась и не объявлена Engine-дефектом.
