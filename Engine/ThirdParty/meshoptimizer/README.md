# meshoptimizer (vendored subset)

Источник: https://github.com/zeux/meshoptimizer, тег **v0.22**, лицензия MIT (`LICENSE.md`).

Взяты только файлы, нужные для построения уровней детализации из LOD0:

| Файл | Зачем |
|---|---|
| `meshoptimizer.h` | заголовок библиотеки |
| `allocator.cpp` | аллокатор, которым пользуются остальные файлы |
| `indexgenerator.cpp` | `meshopt_generateVertexRemap` — сварка вершин по позиции перед упрощением |
| `simplifier.cpp` | `meshopt_simplify`, `meshopt_simplifySloppy` |
| `vcacheoptimizer.cpp` | порядок треугольников под кеш вершин |
| `vfetchoptimizer.cpp` | порядок вершин под выборку |
| `spatialorder.cpp` | нужен симплификатору |

Остальное (кодеки вершин и индексов, кластеризация, анализаторы, стрипификация) не используется и не вендорится.

Обновление: заменить файлы из нового тега, ничего больше трогать не нужно — список в `Engine/Engine.vcxproj`.

Код библиотеки не правился. Обёртка движка — `Engine/Source/Render/MeshSimplifier.cpp`.
