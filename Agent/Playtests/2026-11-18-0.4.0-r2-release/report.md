# PainEngine 0.4.0 r2 — повторный релизный цикл

По запросу пользователя повторён цикл на main `48901772` (PR31),
объединённом с release/v0.4.0 в `ffa76363`. Предыдущие отчёты и ZIP сохранены.
Новая функциональность — Mesh-физика: точные статические треугольники,
выпуклая оболочка для Dynamic/Kinematic, сохранение Scene JSON и C ABI,
четвёртая форма в инспекторе. Версия остаётся 0.4.0.

## Исправления

- REL040R2-001: отражённый Mesh и преобразование родителя теряли часть матрицы.
  До исправления физический raycast не попадал в зеркальный треугольник,
  после — совпадает с геометрией для трёх сценариев трансформации.
- REL040R2-002: неиспользуемая вершина `(100,100,100)` раздувала hull и массу.
  При импульсе 100 и density 10 скорость была 0.0995007 вместо 10.
  Теперь hull использует только вершины пригодных индексируемых треугольников;
  скорость 10, неверные индексы и нечисловая геометрия безопасно отвергаются.
- REL040R2-003: pid+GetTickCount64 давали одинаковые backup-папки быстрых
  операций. Неуспешный импорт оставлял каталог `original`, следующая операция
  с файлом получала AccessDenied. Создание папки теперь атомарно уникально;
  тест проверяет 32 отдельных backup для 32 последовательных переносов.

Первые два дефекта воспроизведены в `mesh-regression-before.log` (84/86),
затем устранены (`mesh-regression-after.log`, 86/86). Исходные неудачные логи
сохранены. В `cmake-final.log` также была помеха: два EngineTests одновременно
использовали общий временный каталог шейдеров. Финальные прогоны выполнены
последовательно; отдельное исправление шейдеров не требовалось.

## Финальная проверка

| Набор | Результат |
|---|---|
| MSBuild Engine | 86/86, без пропусков |
| CMake CTest | 3/3: PortableCore, ComponentGeneratorPortable, Engine86/86 |
| AI | 58/58 |
| Editor Core / Host / Ui | Все прошли |
| Native UI: AI diagnostics, camera guides | Прошли |
| Diagnostics commandlet API | Прошёл |
| Installed SDK consumer / generator | 2/2 |
| CMake Unicode workflow | Создание, enable, startup, no-op, AddClass, failure preservation прошли |
| Офлайн-справка | 7 руководств; API41 страниц,327 записей,120 заголовков |

Mesh регрессия дополнительно проверяет Scene JSON, чтение/запись формы3
через PhysicsObject v1, отклонение формы4 и выбор Mesh после смены RU/EN.
Финальные Runtime/SDK после трёх исправлений зафиксированы в JSON-хешах.
Практический прогон прежнего автора стратегии —
[отдельный отчёт](../2026-11-18-0.4.0-strategy-r2/report.md).

## Воспроизведение и границы

MSBuild: [verify-msbuild.ps1](verify-msbuild.ps1), затем финальные сборки
Engine, Runtime/SDK и Commandlets после AssetUpdate fix. CMake:

```powershell
./Engine/build-cmake.ps1 -Preset editor -Configuration Release -QtRoot E:/Engine/Editor/ThirdParty/Qt6.7 -EnableAssimp -EnableJolt -Jobs 6 -Test
./Engine/BuildSupport/tests/Test-CMakeProjects.ps1 -Runtime E:/Engine/build/cmake/editor-Release/bin/Release/Runtime
node Engine/docs/build-guides.mjs --check
node Engine/docs/api-reference/build.mjs --check
```

На этой машине CMake/Ninja добавлялись из Engine/build/tools/python в PATH.
Сборки MSVC выполнялись вне sandbox из-за C1902 в ограниченной среде.
Проверен Windows x64, Assimp и Jolt включены. Другие ОС и мониторы не проверены.
Непосредственный UI-проход агента учитывается отдельно от native/API-тестов.
GitHub Release не создаётся; готовый ZIP и его проверка фиксируются отдельно.

Повторный fetch подтвердил origin/main `48901772`. Готовая папка поставки
прошла 24/24: включая экспорт CMake Runtime и запуск Game.exe с wrapper,
добавленным из MSBuild. Проверены 15 HTML,130 локальных ссылок,182 PE x64,
отсутствующих DLL и ссылок нет. Сырая CMake build-папка без wrapper не умеет
экспортировать игру; это ограничение состава, устранённое в поставке.
Непосредственный Inspector-проход агента partial: окно недоступно CUA.
