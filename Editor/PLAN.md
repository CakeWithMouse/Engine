# План разработки PainEngine Editor v2
> **Ветка:** `rewrite/editor-v2`  
> **Архитектор:** Юрий  
> **Инженер-исполнитель:** Antigravity  
> **Протокол:** `engine-work-protocol`

---

## Чек-лист среза S1 (Каркас Core/Host/UI, сервисы Core, check.ps1)

- [x] **Шаг 1.1 (fix):** Исправление путей библиотек Qt в `Editor/EditorModule.vcxproj` через прямые пути `$(QtInstallDir)\lib\Qt6*$(QtLibSuffix).lib` для обхода проблемы запятых в пути репозитория.
- [x] **Шаг 1.2 (chore):** Создание и ведение `Editor/PLAN.md` с эталоном тестов, разделами Решения, Backlog, Нужно в ABI.
- [x] **Шаг 1.3 (chore):** Создание `Editor/tools/check.ps1` с режимами `-Quick` (пошаговая валидация Debug, слои, wc -l, запретные зоны) и `-Full` (Debug+Release, полный прогон).
- [x] **Шаг 1.4 (refactor):** Формирование структуры каталогов `Editor/Core`, `Editor/Host`, `Editor/UI`, `Editor/tools` и обновление `Editor/EditorModule.vcxproj`.
- [x] **Шаг 1.4b (chore):** Защита `check.ps1`: таймауты процессов, динамическое чтение версии из `Engine/Version.json`, очистка кэша версий лаунчера, завершение зависших процессов.
- [x] **Шаг 1.5 (feat):** Реализация фундаментальных сервисов `Core`: `CommandRegistry`, `IUserPrompt`, `Logger`, `FileService` (песочница проекта) + юнит-тесты без виджетов.
- [x] **Шаг 1.6 (test):** Тестовый каркас UI (`objectName` локаторы, хелперы взаимодействия).

---

## Чек-лист среза S2 (Host-слой, развязка QtHost и MainWindow, декомпозиция DrainCommands, Checkpoint coalescing)

- [x] **Шаг 2.1 (refactor):** Создание `Editor/Host/IEditorHost.h` с интерфейсами `IEditorHost` и `IEditorUiListener`. Развязка `QtHost` от `MainWindow.h` (удалено поле `window_`, удалён прямой `#include "MainWindow.h"`, связь событий через `IEditorUiListener`).
- [x] **Шаг 2.2 (refactor):** Выделение `Editor/Host/EditorHistory` (`QTemporaryDir`, `Checkpoint()`, `RecordChange()`, Undo/Redo стек max 64, coalescing жестов перетаскивания и nudge).
- [x] **Шаг 2.3 (refactor):** Выделение `Editor/Host/ViewportCommandHandler` (ввод, HWND, Play/Pause/Stop, камера) и `Editor/Host/AssetCommandHandler` (импорт ассетов, hot reload, фоновые задачи).
- [x] **Шаг 2.4 (refactor):** Выделение `Editor/Host/SceneCommandHandler` (мутации сцены, snapshot, иерархия, параметры объектов). Сокращение `QtHost.cpp` с 641 до 298 строк.
- [x] **Шаг 2.5 (chore):** Регистрация новых файлов в `Editor/EditorModule.vcxproj`, интеграция через `Editor/UI/EditorApp.cpp`, валидация через `check.ps1 -Quick` (сборка Debug, CoreTests 47/47, UiTests 15/15, слои, wc -l).

---

## Эталон тестов (Baseline S0 / S1 / S2)

| Тест / Скрипт | Результат на `main` | Результат после fix(build) | Примечание |
|---|---|---|---|
| Сборка `Editor` (Debug) | **FAIL (LNK1104)** | **PASS** | 0 ошибок, 0 новых warnings |
| Сборка `Editor` (Release) | **FAIL (C1083 LTCG)** | **PASS** | Собрано из пути без запятых (`Documents\GitHub\Engine`) |
| `Editor/tests/CoreTests.exe` | — | **47/47 PASS** | Юнит-тесты Core без виджетов |
| `Editor/tests/UiTests.exe` | — | **15/15 PASS** | Самопроверка тестового каркаса UI |
| `Engine/tests/run.ps1` | **43/43 PASS** | **43/43 PASS** | Тесты ядра Engine |
| `Engine/tests/component-generation/run.ps1` | **20/20 PASS** | **20/20 PASS** | Генератор компонентов и контракты |
| `Engine/tests/edit-session/run.ps1` | **609/609 PASS** | **609/609 PASS** | Edit session API (без GUI) |
| `Engine/tests/run-viewport.ps1` | **7/7 PASS** | **7/7 PASS** | Рендеринг и вьюпорт |
| `Engine/tests/scene-module/run.ps1` | **109/109 PASS** | **109/109 PASS** | C-ABI сцены |
| `Engine/tests/skeletal/run.ps1` | **8/8 PASS** | **8/8 PASS** | Скелетная анимация |
| `Engine/tests/code-projects/class-names.ps1` | **27/27 PASS** | **27/27 PASS** | Имена классов |
| `Game/tests/run-games.ps1` | **27/27 PASS** | **27/27 PASS** | Игры и Launcher |
| `Editor/tests/run.ps1` | **LNK1104 FAIL** | **35/67 (FAIL на L394)** | Сборка успешна; остановка на проверке экранного захвата превью `grabWindow` |
| `Engine/tests/run-runtime.ps1` | **LNK1104 FAIL** | **25/25 PASS** | Ранее зависел от `Editor.dll` |
| `Engine/tests/compatibility/run.ps1` | **LNK1104 FAIL** | **88/88 PASS** | Ранее зависел от `Editor.dll` |
| `Engine/tests/project-workflow/run.ps1` | **LNK1104 FAIL** | **262/262 PASS** | Ранее зависел от `Editor.dll` |
| `Engine/tests/run-editor-viewport.ps1` | **FAIL** | **FAIL** | Остановка при закрытии редактора |
| `Engine/tests/assets/run.ps1` | **FAIL (PS parse)** | **FAIL (PS parse)** | Эталон: не запускается в PS5.1 (нет BOM) |
| `Engine/tests/code-projects/portable.ps1` | **FAIL (PS parse)** | **FAIL (PS parse)** | Эталон: не запускается в PS5.1 (нет BOM) |
| `Engine/tests/code-projects/run.ps1` | **FAIL (PS parse)** | **FAIL (PS parse)** | Эталон: не запускается в PS5.1 (here-string `@'`) |
| `Engine/tests/input/run.ps1` | **FAIL (PS parse)** | **FAIL (PS parse)** | Эталон: не запускается в PS5.1 (C++ строки) |

---

## Решения

1. **Разделение слоев архитектуры:**
   - `Core/`: только QtCore/QtGui без виджетов (`QWidget`, `QAction`, `QDialog`, `QMessageBox` запрещены). Диалоги только через интерфейс `IUserPrompt`.
   - `UI/`: тонкие виджеты, вызывают команды исключительно через `CommandRegistry`.
   - `Host/`: мост к движку через C-ABI `IEditorHost`. UI не знает про `Host`, `Host` не знает про UI.
2. **Безопасная линковка Qt:**
   - Подключение библиотек Qt выполняется явным указанием путей к файлам `$(QtInstallDir)\lib\Qt6*$(QtLibSuffix).lib` в `AdditionalDependencies`, что исключает разрушение путей с запятыми в `/LIBPATH`.
3. **Режимы верификации в `check.ps1`:**
   - `-Quick`: сборка Debug, проверка слоёв, `wc -l`, запретные зоны, быстрый юнит-тест. Запускается после каждого атомарного коммита.
   - `-Full`: сборка Debug + Release, полный эталон тестов. Запускается в конце каждого среза.
4. **Защита от блокировок модальными окнами лаунчера:**
   - Модальные окна лаунчера блокируют прогоны, лечится очисткой кэша (`%LOCALAPPDATA%\PainEngine\Engines\<версия>`, читается динамически из `Engine/Version.json`) и таймаутами процессов в `check.ps1`.
5. **Конфигурация CRT для тестов:**
   - Debug-сборка тестов использует `/MD` + `NDEBUG` (как `EditorModule.vcxproj`), потому что Qt6 в поставке только релизный; CRT-проверки кучи в Debug отключены.
6. **Статическая компоновка Core и границы типов Qt:**
   - Модуль `Core` не экспортируется как отдельная динамическая библиотека, а компилируется статически прямо в потребители (`Editor.dll`, `CoreTests.exe`).
   - Типы Qt (`QString`, `QStringList`, `QByteArray`) допустимы только внутри бинарника `Editor.dll`. На границе `Editor.dll` <-> `Engine` (C-ABI `ModuleApi.h`, `SceneModuleApi.h`) передача объектов Qt строго запрещена; используются только C-совместимые типы (указатели, POD, C-строки).
7. **Тестовый каркас UI (`UiTestHelper`):**
   - Локация элементов строго по `objectName` (`Find`, `Require`), поиск по тексту запрещён.
   - Хелперы взаимодействия (`ClickButton`, `TriggerAction`, `SetLineEdit`, `SetSpinBox`, `SelectComboBox`, `WaitUntil`) гарантируют nullptr-безопасность, проверку `isEnabled` и вызов исключений при отсутствии элементов.
   - Самопроверка каркаса запускается через `EditorUiTests.exe --self-test` (15 проверок).
8. **Развязка Host и UI слоев (`IEditorHost` и `IEditorUiListener`):**
   - Хост `EditorQtHost` реализует интерфейс `IEditorHost` и не включает заголовок `MainWindow.h`, не держит указатель на окно и не зависит от UI-классов.
   - Окно редактора `EditorMainWindow` реализует `IEditorUiListener` (`OnLogMessage`, `OnCommandFinished`) с потокобезопасной доставкой через `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
   - Запуск приложения вынесен в слой UI (`EditorApp.cpp`), регистрирующий bootstrap-фабрику окна.
9. **Политика Checkpoint и coalescing жестов в `EditorHistory`:**
   - Непрерывные жесты перетаскивания гизмо обрамляются `BeginGizmoDrag` / `EndGizmoDrag`. Первый тик создает ровно 1 файл контрольной точки `N.scene.json` в `QTemporaryDir`, последующие тики в течение жеста коалесцируются (0 файлов).
   - Одно нажатие `Undo` возвращает объект в состояние до начала жеста.

---

## Backlog

- [ ] **Release Editor (LTCG, Editor.iobj) в путях с запятой:** Release Editor не собирается из каталога с запятой в пути (`fatal error C1083: Не удается открыть промежуточный файл компилятора: ...Editor.iobj: No such file or directory`); гипотеза подтверждена фактом успешной сборки из `C:\Users\Yuri\Documents\GitHub\Engine`. Требуется правка `EditorModule.vcxproj` (настройка `IntermediateOutputPath` без макросов, ломающихся на запятых) и проверка `-PathStress`.
- [ ] **Скрипты тестов PS 5.1:** 4 скрипта на `main` не парсятся (`assets/run.ps1`, `code-projects/portable.ps1`, `code-projects/run.ps1`, `input/run.ps1`). Эталон = не запускается, причина: нет BOM / несовместимый синтаксис PowerShell 5.1. Требуется согласование фикса кодировки/форматирования в `Engine/tests/`.
- [ ] **Тест `EditorUiTests.cpp:394`:** падение `preview clears to its background` связано с `grabWindow` в виртуализированном/фоновом графическом окружении. Оставить красным до этапа переработки окна материалов.
- [ ] **Content Browser Sandbox:** замена открытого `QFileSystemModel` на изолированную модель на базе `FileService`, запрещающую выход за корень проекта.
- [ ] **Checkpoint при ошибке setTransform:** Если `setTransform` вернул ошибку на первом тике жеста, `gizmoDragCheckpointed_` не выставляется и следующий тик создаст лишний Checkpoint.

---

## Нужно в ABI

1. **Режимы шейдинга вьюпорта:** Wireframe / Unlit / Lit (переключение конвейера через C-ABI).
2. **Служебная отрисовка вьюпорта:** вкл/выкл сетки (grid), вкл/выкл теней, backface culling.
3. **Выделение:** контурная подсветка (outline) выбранного объекта; поддержка мультивыделения (множественный выбор ID).
4. **Пространственные запросы:** рейкаст курсора в полигональную геометрию сцены с возвратом мировой точки пересечения и нормали поверхности.
5. **Габариты геометрии:** запрос Bounding Box / Bounding Sphere объекта сцены по его `id`.
6. **Иерархия сцены:** концепция пустого узла (папка/группа объектов) в `createObject` без привязки к компонентам меша.
7. **Параметрические меши:** API чтения параметров примитива (радиус, сегменты, размеры) и запрос их перегенерации в рантайме.
8. **Материалы в `SceneModuleApi`:**
   - метод `setMaterialAsset(void* context, uint64_t id, const char* path)`;
   - API управления материалом: компиляция шейдера, чтение/запись скалярных и векторных параметров без прямого дискового ввода-вывода в обход движка.

---

## Не проверено в рантайме

- Интерактивная работа GUI Editor в реальной пользовательской сессии (клики мыши, хоткеи, навигация вьюпорта, манипуляторы гизмо).
- Поведение hot reload (горячая перезагрузка ассетов и шейдеров): тесты `UiTests` проверяют только хелперы взаимодействия тестового каркаса (`UiTestHelper`), а реальный жизненный цикл перезагрузки ассетов и перекомпиляции шейдеров в рантайме НЕ проверен.
