# Исправления и проверка r5

| Функция / BUG | Воспроизведение | До | После |
|---|---|---|---|
| BUG-R5-001, доставка TANGENT | EngineTests / MaterialMirroredNormalMap | Magenta sentinel, w=0 | Настоящий tangent в PS |
| BUG-R5-002, зеркальный scale | Тот же тест, старая MaterialTemplate после исправления signature | Пиксель +V меняется от отражения X | Равенство пикселей при (-1,1,1), (-2,1,0.5); отражение Z меняет +V |
| CPU tangent API | mesh.tangents; TangentCommandletTests.ps1 | Новая команда | 7 сценариев: normal/mirror/degenerate и отказ некорректных данных |
| BUG-R5-003, clone лампы | PointLightCloneCommandletTests.ps1 | Копия теряет castShadows | Сохранён true у оригинала/копии до и после Save/New/Load |
| SaveGame tangent | SaveGameTests, реальный Capture/Restore | Ранее не было проверки | 103 checks + отдельный процесс; basis восстановлен |

Минимальные production-правки: добавить TANGENT в сигнатуру layout и учитывать
знак determinant world/instance matrix. Команды используют настоящий GenerateTangents;
это отдельный уровень доказательства от GPU-теста.

## Команды

Из корня репозитория (PowerShell):

```powershell
& Engine/BuildSupport/Invoke-MSBuild.ps1 -Project Engine/tests/EngineTests.vcxproj -Configuration Release
& Engine/tests/run.ps1 -Configuration Release -NoBuild
& Editor/tests/TangentCommandletTests.ps1
& Editor/tests/PointLightCloneCommandletTests.ps1
& Engine/tests/savegame/run.ps1
& Engine/tests/assets/run.ps1 -Configuration Release -EnableAssimp
& Editor/tests/run.ps1 -Configuration Release -EnableAssimp
& Engine/tests/compatibility/run.ps1 -IncludeUi
node Engine/docs/api-reference/build.mjs --check
& Engine/build.ps1 -Configuration Release -Target All
```

Для изолированного теста TEMP/TMP перенесены в `Engine/build/playtests/20261107-r5`,
PATH дополнен Runtime и установленным vcpkg bin. MSVC собирался вне sandbox после
зафиксированного отказа службы PDB. Предыдущие пользовательские данные не менялись.

Логи [logs](logs): core-after 60/60, SaveGame 103 + restart, assets 229,
native EditorUi 5036, compatibility 116 + legacy UI 387. `core-before.log`
содержит первоначальную слабую проверку зеркального масштаба, которая проходила
через derivative fallback. `tangent-before.log` и `mirror-before.log` — уточнённые
независимые воспроизведения. Тест не ослаблялся для успешного результата.

## Визуальные свидетельства

В [visual](visual) сохранены и осмотрены Battle кадры 1000×500: `--static --units 1000
--camera 120 --frames 65 --dump-frame ...`, default Deferred, `--forward`,
`--no-point-shadow`. Тень лампы меняет освещение близких зданий/машин; обе схемы
рендера отображают сцену. Это статический пример, не проверка дрожания камеры.
`editor-light.png` доказывает раскладку настоящего Qt инспектора; D3D child viewport
в Qt grab чёрный, поэтому эта картинка не доказывает edit-rendering.

## Пакет

`package.ps1` копирует текущие Release приложения/ресурсы/SDK в новый каталог,
сохраняет библиотеки SDK и исключает отладочные символы/тестовые exe/кэши.
Исторического baseline архива в рабочем каталоге нет, поэтому пакет собран из
проверенных текущих output, а не через New-Release.ps1 с историческим baseline.
Он не включает прошлые runtime; старые бинарные fixtures проверены отдельным suite.
Пакетные smoke и новый C++ проект проверены до создания ZIP; результаты приложены
в `logs/package-checks`.

Архив создаётся `python fixes/archive.py <каталог-пакета> <новый.zip>`: UTF-8 имена,
CRC и сверка всех файлов со staged SHA256SUMS. Windows tar в этом окружении записал
русские имена в OEM-кодировке без Unicode-флага; этот промежуточный ZIP заменён.
Финальный архив прошёл проверку 970 файлов.
