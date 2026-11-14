# Дневник — роль 4, броски хлама в ведро, r5

## Подготовка и проект

HEAD `55d3b0c`; финальный запуск по согласованию координатора на Runtime/SDK `2026.11.07.0`, Windows x64 Release, RTX 4070 Laptop GPU. Исходник r3 распакован в `Engine/build/playtests/20261107-r5/ai-trash/ImportedSource`; архив и старые отчёты r3 не менялись. Штатно создан новый CLI `TrashGame`, включён C++, затем перенесены только `Source` и `Content`; свежие Project.json/lock сохранены. Рабочие store и TEMP находятся в папке роли.

Первая пересборка завершилась LNK2019/LNK2001 для определений `ProjectGame::{RegisterHit, RegisterMiss, SerializeSaveGame, OnSaveGameLoaded}`. `CodeBuild.log` показал, что при переносе файлов из ZIP source timestamp был старше пустого scaffold Intermediate, поэтому MSBuild пропустил ProjectGame.cpp. После обновления timestamp `.cpp/.h` только в копии проекта повторная `--build-project` прошла с exit 0; успех активировал новую Game.dll. Это копирование старого проекта и timestamp-кэш, не дефект Engine/API.

## Маленький игровой фрагмент и SaveGame

С `TRASH_SAVE_TEST=save` запущен один предмет (импортированный multi-material бросок) и сохранён в полёте после elapsed=0.483334. Код протокола поменял мир вторым броском, загрузил снимок и проверил что остался ровно первый projectile: throws=1/hits=0, elapsed=0.5; `RoundSaveTest.log` помечает PASS.

Следующий отдельный процесс с `TRASH_SAVE_TEST=load` восстановил тот же полёт на elapsed=0.5; затем projectile достиг ведра и фактический игровой счётчик стал `hits=1 throws=1 PASS`. Оба процесса на Runtime .07 завершились exit 0, по 180 кадров. BMP `trash-round-save-load.bmp` просмотрен: предмет действительно виден в полёте над дорожкой к ведру, HUD показывает Hits 0 / Throws 1 до попадания. Сам hit подтверждён игровым логом после продолжения полёта.

## CPU tangent API

Без GPU вызван финальный `EngineCommandlets.exe mesh.tangents` на трёх минимальных запросах (по одному треугольнику; нормаль +Y; ребро U вдоль +X, V вдоль +Z):

- Обычный UV: все три вершины получают tangent `[+1,0,0]`, handedness `-1`.
- Зеркальный UV по U: все получают `[-1,0,0]`, handedness `+1`; изменение знака совпадает с ожидаемой зеркальной ориентацией UV.
- Вырожденный UV: все получают конечный fallback `[0,0,-1]`, `w=0` для derivative fallback.

Каждая команда exit 0 и `ok=true`. Это подтверждает CPU generation/sign/fallback; не проверяет GPU input signature, отрицательный transform scale или картинку normal map.

## Осталось непроверенным

Экспорт проекта через UI не запускал (экспорт не входит в список `EngineCommandlets --list`, UI в этой роли ведёт координатор). Новый UI/AI генератор кода, импорт ассета в новом круге и визуальный пиксельный анализ не делались. Прежний source оставлен отдельно.
