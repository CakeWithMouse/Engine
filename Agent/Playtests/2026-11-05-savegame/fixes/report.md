# Фикс движка после разработчиков

Навык: `C:/Users/serge/.codex/skills/fix-engine-after-developers/SKILL.md`.

| ID | До исправления | Причина / изменение | Регрессия |
|---|---|---|---|
| BUG-SAVE-001 | Runtime upgrade/check не запускался, 0xC0000135; Assimp DLL отсутствовали в Runtime. После сборки commandlet проблема повторилась. | Общая Runtime-папка нескольких target; добавлен AssimpRuntime.targets для Runtime и EditorModule, копирующий замыкание DLL после каждой сборки. | Агент повторил исходный upgrade (exit 0); commandlet rebuild и SaveGameCommandletTests запускаются без дополнительного PATH. |
| BUG-SAVE-002 | SDK copy прерывался на заблокированной Engine.lib после копирования заголовков; следующий candidate падал 0xC0000005. | New-ProjectSdk собирает полный staging SDK и публикует каталог целиком с резервом/откатом. | SdkPublishTests удерживает Windows directory handle: отказ сохраняет старые bytes/marker; после unlock повтор публикует matching Engine.lib и headers. Агент подтвердил startup после согласованного SDK. |

Новые операции проверяются через поддерживаемые `savegame.save/load/validate/list/delete`
EngineCommandlets и SaveGameTests, а не копию игровой логики. Предыдущие исправления
отката графа материалов в рабочем дереве сохранены. Итоговый проект агента собран
(`31163a5972644095b82166d68dcd2776`), validate и два процесса Save/Load завершились с exit 0.
Оба подтверждённых дефекта закрыты повторной проверкой. Дампы города совпали; подозрение
на чёрный кадр оказалось ошибкой MIME в tool preview. Пожелания об индикации сохранения
и настройке горячих клавиш записаны в отзыве и не считаются дефектами SaveGame.
