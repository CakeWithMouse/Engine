# Engine
New engine better than UE5

Учебный движок на C++17 и Direct3D 11. Устройство проекта — в «Описание проекта.md», список доработок и их статус — в «Правки.md».

## Подготовка

1. Visual Studio 2026 (toolset v145) с рабочей нагрузкой «Разработка классических приложений на C++» и Windows SDK 10.
2. vcpkg (используется в manifest-режиме, зависимости перечислены в `1_Lab/vcpkg.json`):

   ```bash
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   ```

   ```bash
   C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
   ```

   Если vcpkg лежит в другом месте, задайте переменную окружения `VCPKG_ROOT`.

При первой сборке MSBuild сам установит Assimp для нужной разрядности в `1_Lab/vcpkg_installed` и скопирует runtime DLL рядом с `.exe`.

## Сборка

Откройте `1_Lab/1_Lab.sln` или соберите из командной строки:

```bash
MSBuild 1_Lab/1_Lab.sln /p:Configuration=Debug /p:Platform=Win32
```

Поддерживаются Debug/Release для Win32 и x64. В Release шейдеры компилируются с оптимизацией, в Debug — с отладочной информацией.

Проверки ядра и GPU-шейдера частиц: `./tests/run.ps1` из корня репозитория.
Они собираются отдельным проектом без Assimp и запускаются на D3D11 WARP.
Требования, Release-запуск и покрытие описаны в [tests/README.md](tests/README.md).

## Запуск

Программа сама находит каталог `1_Lab` (по `Source/Shaders`) — от рабочей директории или от расположения `.exe`, так что её можно запускать из Visual Studio, из папки сборки или из консоли. Переопределения:

- `ENGINE_PROJECT_DIR` — каталог с `Source/Shaders`;
- `ENGINE_CONTENT_ROOT` — корень ассетов сцен (по умолчанию корень репозитория). Изображение неба `cosy.jpg` и `.obj`-модели в репозиторий не входят; без них сцены работают, но без соответствующих объектов.

Сцена и режим рендера выбираются полями `CurrentGameType` и `RenderingType` в `1_Lab/Source/Public/Base/Config/BaseGameConfig.h`.

| Клавиша | Действие |
|---|---|
| Esc | Выход |
| F1 / F2 | Свободная / орбитальная камера |
| F3 | Отладочные окна G-buffer (deferred) |
| F4 | Вкл/выкл VSync |
| F5 | Переключение Forward ↔ Deferred |
| Space | Пауза симуляции |

В заголовке окна: режим, FPS, CPU и GPU время кадра, число объектов и инстансов, draw calls, отсеченные объекты.


## Развитие движка

Долгосрочная цель и план по отделению ядра от игр, созданию Engine/GameExample и переносу SunGame ведутся в [таск-трекере](Задачи/027-engine-game-separation-roadmap.md). Контекст проекта и правила для помощников описаны в [AGENTS.md](AGENTS.md).
