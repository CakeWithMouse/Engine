# Границы покрытия r3

Матрица содержит 85 групп возможностей. Попытка двух разработчиков не равна двум успешным проверкам; partial сохраняет ограничения конкретной пробы.
Два разработчика практически попробовали 67/85 групп; два успешных результата есть у 50/85. Совсем без попытки осталось 0 групп.

Общие тесты координатора не прибавляют автоматически четыре пользовательских результата. Большинство основного круга относится к .7; .8 получила прицельные игровые повторы и регрессии исправления. Ручной Inspector не проверялся всеми персонами; native UI tests записаны отдельно.

## Менее двух разработчиков

| ID | Возможность | Попыток | Успешно |
|---|---|---:|---:|
| class-wizard | Мастер классов Game/Component/Object/Character | 1 | 0 |
| hot-reload | C++ Hot Reload с сохранением edit-сцены | 1 | 1 |
| skeletal | Скелетная модель и клипы анимации | 1 | 1 |
| mesh-tools | Проверка мешей и упрощение CPU LOD | 1 | 1 |
| material-params | Параметры/текстуры материала | 1 | 1 |
| material-errors | Ошибка шейдера/fallback/восстановление | 1 | 1 |
| material-blends | Прозрачность/masked/two-sided | 1 | 0 |
| light-point | Точечный свет/радиус/интенсивность | 1 | 1 |
| sky | Небо/cubemap/отражения | 1 | 1 |
| particles | Искры/дым и пауза | 1 | 1 |
| particle-collision | GPU particles depth collision/sorting | 1 | 0 |
| hud-project | Назначение .ui HUD проекта | 1 | 1 |
| hud-events | События кнопок и input capture | 1 | 1 |
| hud-editor | Редактор UI/сохранение/Hot Reload | 1 | 0 |
| vfs | VFS чтение/запись/mount через host API | 1 | 1 |
| compatibility | Закрепление версии/обновление копии/старый ABI | 1 | 0 |
| sdk-examples | Компиляция примеров API Reference | 1 | 1 |
| render-host | Публичные D3D/render hooks пользовательского компонента | 1 | 1 |

## Двое попробовали, но двух полных успехов нет

| ID | Возможность | Успешно |
|---|---|---:|
| create-cpp | Создать сразу C++ проект | 0 |
| failed-build-rollback | Отказ сборки сохраняет рабочую DLL | 1 |
| scene-codec | Пользовательский кодек и восстановление | 1 |
| scene-object | Произвольный ENGINE_OBJECT и адаптер | 1 |
| scene-select | Picking/иерархия/рамка/фокус | 1 |
| session | Сохранение subsystem между сценами сессии | 0 |
| trace-screen | RaycastCursor/Screen и viewport | 1 |
| trace-debug | Отладочный луч/нормаль/статистика | 1 |
| lod | LOD/culling/impostor | 1 |
| material-editor | Редактор HLSL/preview/сохранение/откат | 1 |
| light-sun | Направленный свет | 1 |
| shadows | Тени/дальность/cache | 1 |
| hud-layout | Якоря/layout/resize/кириллица | 0 |
| render-modes | Forward/Deferred/G-buffer | 1 |
| render-resize | Изменение размера viewport | 1 |
| host-services | Сервисы BaseEngine: lifecycle/VFS/active session | 0 |
| imported-uv | UV: BaseMap и texture объекта | 1 |

Конкретные недостающие действия описаны в evidence.json каждой роли: shader/material UI, широкие возможности renderer/particles, точный набор SDK-примеров, полнота visual UV/LOD и ввод через GUI не объявлены полностью проверенными по одному smoke.
Новые широкие проверки остановлены при общем недельном счётчике 48%, с запасом до пользовательского потолка 50%.
