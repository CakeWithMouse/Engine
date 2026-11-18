# Repair Corridor — Mesh Physics, Engine 0.4.0 r2

## Итог

Проверен релизный диапазон `5458f022..48901772` (release merge `ffa76363`) на продолжении Repair Corridor r5. Добавлены игровые static concave Mesh-арка с отверстием, движущийся open-tray Mesh-груз и сфера-проба; в игре и отдельном native host использованы публичные API. Исходная r1-копия и архив сохранены без изменений.

Результаты текущей проверки:

| Функция | Статус | Фактическое свидетельство |
|---|---|---|
| Static concave triangles / отверстие | **passed** | В настоящем Play Mesh-груз после E прошёл зеркальную арку: z=2 → 5.52 м; узкая 2 м апертура осталась проходимой. `meshGateBody=1`, `crateMeshBody=1`; все sampled violations=0. |
| Moving convex hull | **passed** | U-tray Dynamic/Mesh создан; проба Dynamic/Sphere была запущена после появления body и в раннем sample находилась в y=1.4826 при ожидаемом контакте около tray top 1.30 + radius .18. Позже груз уехал, поэтому этот sample — контакт во время движения, не долговременный покой. |
| OBJ import + Mesh state save/reopen | **passed** | Собственный native host импортировал два OBJ, создал модели, записал static/dynamic `shape=mesh`, сохранил сцену, уничтожил session, создал новую, загрузил сцену и прочёл оба набора physics settings. |
| Импортированный OBJ collision после reopen | **partial** | Проверены import и persisted PhysicsObject state, но collision импортированной сцены после reopen не запускалась. Runtime collision подтверждена на процедурной gameplay geometry. Отрицательный scale сериализован; host не сравнивает `transform` полями после reopen. |
| Mesh Inspector combo, личная GUI-сессия | **partial** | Один запуск Editor `EngineRuntime.exe --project <r2 Project.json> --mode editor`: pid 35644; `sky.list_apps` и `sky.list_windows` не вернули окно; при последующей проверке процесс отсутствовал. Запуск не перенаправлял stdout/stderr и exit code не сохранил, поэтому нельзя отличить ранний выход проекта от ограничения window inventory. Повтор не делал. Native UI test координатора отдельно прошёл Mesh combo RU/EN; это не личный визуальный проход. |
| Gameplay replay / стратегия r5 | **passed** | Новый MSBuild Runtime `project.input` выполнил 1241 кадр, `exit=0`, выбрано 3 бойца, принято 2 приказа, итог «на месте». Охранник позже захватил отряд: `lost=1`, `won=0`; это ожидаемый проигрыш старого replay. Отряд дошёл только до прежней цели около z=0; через арку прошёл груз, не бойцы. Отдельные `barrierViolationFrames=0` относятся к старому ServiceBarrier-приказу, а не к MeshGate. |
| Release build / свежий архив / standalone | **passed** | MSBuild и CMake private project builds прошли; ZIP source-only распакован заново и собран CMake; fresh gameplay replay прошёл. CMake Runtime60 и экспортированный MSBuild `Game.exe --frames 60` завершились `GameUpdates=60`, `EngineStopped`, exit 0. |
| Export `Game.exe` | **passed** | MSBuild Export-Project прошёл, `export-msbuild-040/Game.exe` существует; экспортированный exe прошёл 60 кадров. Координатор сообщил packaged CMake Runtime export/Game.exe 60 pass (внешнее свидетельство в release report). |

## Игровая проверка

В procedural Play `MeshGateArch` использует `PhysicsShapeType::Mesh`, static body, асимметричные индексированные треугольники и отрицательный X scale. Апертуры по центру после зеркального transform — 2 м. `SupplyCrate` — open U-tray `MeshComponent`, dynamic body; его render mesh содержит неиспользуемую дальнюю вершину как адресный retest REL040R2-002. Результат: crate прошёл через отверстие и упёрся в `CrateStop` около z=5.52. Это демонстрирует сохранение отверстия у static concave mesh при реальном перемещении dynamic mesh body.

`HullProbe` — sphere с радиусом 0.18, стартует над tray. Начальная скорость задаётся единожды в первом `TickComponents` после `Game::TickComponents`, когда `HasPhysicsBody()==true`. Ранний sample показывает положение, согласующееся с контактом на hull. После импульса crate перемещается из-под пробы, поэтому позднюю координату не выдаю за контактный покой.

Повтор сохранён в `evidence/mesh-gameplay-final.*` и `evidence/fresh-cmake-play-final.*`; D3D кадр — `evidence/mesh-gameplay-final.frame.bmp`. В gameplay telemetry 41 sample; gate/body handles подтверждены, crate дошёл до z=5.52, у группы итоговый приказ на месте и `barrierViolationFrames=0`. Оба input replay завершились `exit=0`.

## Сборки и артефакты

- Финальный MSBuild Release Runtime для личных проверок: `E:\Engine\Game\build\x64\Release\Runtime`; `EngineRuntime.exe` SHA256 `BBCCF2A943C902185A397AA055F2099F8672ECC1BE6AB8B130767F075E69F674`; `EngineCommandlets.exe` SHA256 `22A1875991225DA24D44C56251B6E368572D8D5386AAD46409CA3CF8CB1D4C76`; `SDK/Lib/Engine.lib` SHA256 `D288D5DF2ACEDE3836A0D89AFD2715F3AC8C318C36E68D7F159100F8530449B3`. MSBuild SDK использовался как проверочная установка, отдельно в релизный ZIP не входит.
- Финальный CMake Release Runtime: `E:\Engine\build\cmake\editor-Release\bin\Release\Runtime`; `EngineRuntime.exe` SHA256 `CD8F4218326D91DE9322FFBA2BA093A021F20A2CA7161545442AE796C036F795`; `SDK/Lib/Engine.lib` SHA256 `A4288DC061C10C16B86470C85B97BA79C2342E4369F5BFDAC739E987DEB520CB`. Именно CMake SDK с этим hash включён в релизный ZIP.
- Ранние CMake build attempts на выданном, но затем обновлённом SDK `D0331318…52C9` сохранены отдельно; они не являются финальным свидетельством. Финальные сборки выполнены на актуальных хешах выше.
- Source ZIP: `RepairCorridor-0.4.0-r2-source.zip`, 36 entries (35 файлов и одна директория Content/Models), 28,424 байта, SHA256 `111B211E3AF7B5FD52879DE12DCC55E46B625A3EA3EA10A591B75DAD97B56321`. Root entry непосредственно содержит Game.sln/Game.vcxproj/CMakeLists.txt, исходники и Content; нет SDK, Binaries, Intermediate или SaveData. Fresh unpack: `Engine/build/playtests/strategy-040-r2/fresh-final-cmake/RepairCorridor`; активный DLL `Binaries/Release/e21ee3b848ba4e299987cfc6b3b23808/Game.dll`, SHA256 `FB565E3D8CEAA7580D03D27F3F5C8EB8AE9CA7CAD5EB012A7D63D7AB187EA9AD`.

Полный source archive и команды повторения описаны в [REBUILD.md](REBUILD.md). Свидетельства собраны в [evidence.json](evidence.json); дневник разработки в [diary.md](diary.md), обратная связь — [feedback.md](feedback.md). Координаторские тесты указаны отдельно от личных: `../2026-11-18-0.4.0-r2-release/report.md` и соседние журналы.

## Ограничения и замечания

- Импортированные OBJ модели и PhysicsObject settings прошли save/reopen между разными edit-session; повторный physics collision именно на загруженной scene не запускался.
- Личный визуальный Inspector-сеанс не состоялся: Editor не появился в списке окон после одного запуска; stdout/stderr и exit code отсутствуют. Не приписываю это дефекту Editor без лога.
- Полная визуальная проверка формы арки из top-down кадра ограничена; проходимость подтверждается численной телеметрией crate.
- Raw CMake build-tree Runtime не содержит `GameExecutable.exe`, поэтому прямой exporter вернул documented `LaunchError`; координатор подтвердил, что packaged CMake Runtime с MSBuild wrapper экспортирует успешно. Это ограничение промежуточного build tree, не дефект готового package.
- `SCENE-JSON.md`/`EDIT-SESSION-API.md` до общего исправления документировали только box/sphere/capsule, хотя публичный C ABI уже включал Mesh. Документационный gap передан координатору; он обновил документы в релизном диапазоне.
- Первый source ZIP verifier ошибочно считал `.obj` ассеты в Content compile artifacts. Попытка сохранена как `source-archive-verifier-false-positive.zip`; скорректированная проверка включила модели Content и исключила build outputs. Финальный ZIP отдельно прошёл CRC/source audit координатора.
