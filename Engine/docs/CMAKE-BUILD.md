# CMake, компиляторы и переносимость

Команды выполняются из корня репозитория, рядом с `CMakeLists.txt`.
Нужны CMake 3.24+, C++17-компилятор и Ninja для готовых presets.
Система сборки не читает `.vcxproj` и не требует MSBuild для компиляции.
Старые `Engine/build.ps1`, `Game/Start.cmd` и проекты Visual Studio продолжают работать отдельно.

## Что переносимо сейчас

| Часть | Платформа и компиляторы |
| --- | --- |
| `EngineCore`: VFS, пути ресурсов, геометрические трассировки, компиляция и каталог скриптов | Windows MSVC/clang-cl, Linux GCC/Clang; macOS Clang включён в CI |
| `ComponentGenerator`: сканирование аннотаций и генерация C++/TSV | Windows, Linux; macOS включён в CI |
| `Engine`, игры, runtime, launcher, Qt-редактор и игровые DLL | Windows x64, MSVC или clang-cl |

`EngineCore` — выделенная библиотека перечисленных механизмов, а не весь движок без окна.
Полная сцена, графика, ввод, аудио и загрузчик игровых модулей ещё используют Windows API/D3D11.
Для Linux/macOS игр нужны отдельные реализации этих подсистем и удаление DirectX-типов из общего игрового API.
Один переход на CMake этого не заменяет.

На Windows clang-cl по-прежнему использует заголовки и библиотеки Windows SDK и MSVC STL.
Нужны Visual Studio/Build Tools с C++ tools и Windows SDK. Это зависимость Windows-бэкенда;
на Linux переносимое ядро собирается без установленного Visual Studio.

## Windows: runtime и игры

Из обычного PowerShell скрипт подключает окружение установленного Visual Studio:

```powershell
./Engine/build-cmake.ps1 -Test
./Engine/build-cmake.ps1 -Configuration Release
./Engine/build-cmake.ps1 -Preset clang -Test
```

Для `clang` добавьте `clang-cl` в PATH (например, LLVM tools из Visual Studio).
Результат первой команды: `build/cmake/ninja-Debug/bin/Debug/EngineLauncher.exe`.
Runtime и SDK находятся в соседнем `Runtime`, игры — в каталогах своих имён.
Скрипт разделяет каталоги по preset и конфигурации; старые MSBuild-выходы не затрагиваются.

Прямые команды из Developer PowerShell также поддержаны:

```powershell
chcp 65001
cmake --preset ninja
cmake --build --preset ninja --parallel 8
ctest --preset ninja
```

В этом случае путь — `build/cmake/ninja/bin/Debug`, без суффикса `-Debug` у каталога сборки.
UTF-8 code page нужна для корректного Ninja-учёта заголовков на локализованном MSVC;
обёртка выставляет её сама. Не меняйте компилятор в существующем CMake cache: используйте отдельный `-B`.

## Редактор, Assimp и Jolt

```powershell
./Engine/build-cmake.ps1 -Preset editor -QtRoot ./Editor/ThirdParty/Qt/6.7.3/msvc2019_64 -Test
```

Нужен Qt 6.7+ с Core, Gui, Widgets и Test; Svg используется при наличии.
CMake запускает AUTOMOC и `windeployqt`, помещая Editor.dll и Qt DLL рядом с EngineRuntime.
Для другой установки Qt передайте её корневой каталог через `-QtRoot` или `CMAKE_PREFIX_PATH`.

По умолчанию импортёр Assimp выключен; Katamari не собирается, а шесть тестов импортёра явно пропускаются.
Готовый shared-пакет Assimp можно подключить без скачиваний во время конфигурации:

```powershell
cmake --preset ninja -B build/cmake/assimp -DPAIN_ENABLE_ASSIMP=ON '-DCMAKE_PREFIX_PATH=O:/Engine/Engine/Engine/vcpkg_installed/x64-windows'
cmake --build build/cmake/assimp --parallel 8
ctest --test-dir build/cmake/assimp --output-on-failure
```

Замените путь на свою установку. Альтернатива — `-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake`,
в обёртке `-Toolchain ... -EnableAssimp`. Поддерживается shared Assimp (vcpkg `x64-windows`).
Сборка и SDK копируют DLL из каталога импортёра вместе с его соседними DLL-зависимостями.
Они должны быть размещены там пакетом зависимостей. Static Assimp отклоняется при конфигурации:
его полный граф статических зависимостей пока не упаковывается в SDK.

Физика актуального `main` подключается отдельно: `-DPAIN_ENABLE_JOLT=ON`
или `-EnableJolt` в PowerShell-обёртке. Нужен shared-пакет Jolt с CMake target
`Jolt::Jolt` (vcpkg `joltphysics:x64-windows`). Его DLL и импортная библиотека также
попадают в runtime и SDK. Без этой опции собирается предусмотренная движком заглушка,
а тест, требующий работающий Jolt, явно пропускается. Assimp и Jolt можно включать независимо.

## Linux и macOS: ядро и генератор

```sh
cmake --preset portable
cmake --build --preset portable --parallel 8
ctest --preset portable
```

Выбор другого компилятора и отдельного каталога:

```sh
cmake --preset portable -B build/cmake/portable-clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/cmake/portable-clang --parallel 8
ctest --test-dir build/cmake/portable-clang --output-on-failure
```

Зависимости ядра (GLM и nlohmann/json) уже лежат в репозитории; графические SDK не нужны.
Для кросс-компиляции передайте обычный CMake toolchain file. Если целевая сборка вызывает
`pain_generate_components`, предварительно соберите `ComponentGenerator` на машине сборки
и передайте его абсолютный путь в `PAIN_HOST_COMPONENT_GENERATOR`. Генератор целевой архитектуры
не запускается автоматически на хосте.

## Установленный SDK и C++-проекты

```sh
cmake --install build/cmake/portable --prefix build/sdk
cmake -S Engine/tests/portable/InstalledSdk -B build/sdk-consumer -DCMAKE_PREFIX_PATH=/absolute/path/to/build/sdk
cmake --build build/sdk-consumer
ctest --test-dir build/sdk-consumer --output-on-failure
```

Внешний CMake-проект подключает `find_package(PainEngine CONFIG REQUIRED)` и
`target_link_libraries(MyTarget PRIVATE PainEngine::Core)`. Desktop SDK дополнительно
экспортирует `PainEngine::Engine`. Пакет содержит публичные заголовки, GLM/json, библиотеки,
нативный генератор и CMake-конфигурацию с путями относительно SDK.
SDK предназначен для своей ОС, архитектуры, конфигурации и совместимого C++ ABI;
Windows `.lib` нельзя подключить к Linux-бинарнику. Устанавливайте Debug и Release в разные каталоги.

Полная сборка автоматически публикует desktop SDK в `Runtime/SDK` (target `ProjectSdk`).
Редактор распознаёт `buildSystem: cmake` в Sdk.json: «Включить C++» создаёт CMakeLists.txt,
сборка идёт через Ninja, сохраняются проверка DLL, тестовый запуск и активация только успешной версии.
Изменения исходников и SDK проверяются по содержимому, включая файлы с восстановленной старой датой.
Старые MSBuild SDK продолжают создавать и собирать `.vcxproj`.

В `SDK/share/PainEngine/ProjectTemplate` есть минимальный внешний игровой модуль:

```powershell
cmake -S path/to/SDK/share/PainEngine/ProjectTemplate -B build/my-game -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=path/to/SDK
cmake --build build/my-game
```

Используйте совместимый с SDK компилятор того же семейства и согласованные инструменты.
Для Clang это `-DCMAKE_CXX_COMPILER=clang-cl`; редактор берёт выбор из Sdk.json автоматически.
Для MSVC редактор выбирает совместимый установленный toolchain целиком:
`cl.exe`, STL-заголовки и библиотеки относятся к одной версии. Абсолютный путь
к компилятору машины, выпустившей SDK, не смешивается с заголовками другой версии.
Сценарий интеграционной проверки запускает тот же Windows PowerShell 5.1, что и редактор:

```powershell
./Engine/BuildSupport/tests/Test-CMakeProjects.ps1 -Runtime ./build/cmake/ninja-Debug/bin/Debug/Runtime
```

## Проверки и сопровождение

Проверка ветки 0.4.0 от 18.11.2026: Windows x64, MSVC 19.40, Qt 6.7,
Assimp и Jolt — полная CMake Release-сборка и CTest 3/3, без пропусков.
Внешний C++-проект с Unicode-путём прошёл создание, компиляцию MSVC 19.44,
валидацию/запуск, повтор без изменений, добавление класса и сохранение активной
DLL при намеренной ошибке исходника со старой датой. Результаты других
компиляторов и ОС ниже относятся к предыдущей проверке main; в этом прогоне
Linux, macOS и clang-cl не запускались.

Локально проверены Windows MSVC 19.50 и Clang 22; полный вариант с Qt, Assimp и Jolt
проходит 83/83 теста движка на обоих компиляторах. На Linux проверены GCC 14 и Clang 20.
Portable-тесты проверяют VFS с Unicode-путями, математику, скрипты и работу генератора.
Внешний потребитель проверяет заголовки, линковку и генератор установленного SDK.
Сценарий C++-проекта проверяет создание, запуск, повторную сборку без изменений, добавление класса
и сохранение активной DLL при намеренной ошибке компиляции.

`.github/workflows/cmake.yml` задаёт Linux GCC/Clang, macOS Clang и Windows MSVC/ClangCL.
macOS локально не проверен; результат CI нужно получить после публикации изменений.
При добавлении файлов desktop-движка обновляйте явные списки `cmake/Sources.cmake`;
новый код без платформенных зависимостей включайте в `PAIN_CORE_SOURCES` в корневом CMakeLists.txt.
Справочник по упаковке: [CMake install](https://cmake.org/cmake/help/v3.24/command/install.html).
