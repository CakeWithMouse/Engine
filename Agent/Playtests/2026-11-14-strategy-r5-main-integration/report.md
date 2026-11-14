# Проверка r5 после объединения со свежим main

Правки r5 сохранены в 3e5bb440, затем объединены с origin/main 314b991b
(направляющие камеры и настройки расстояния в инспекторе). Merge f0cc2e1f
прошёл без текстовых конфликтов. Повторный fetch после проверок не нашёл
новых коммитов main.

Проверено на объединённом коде:

- CoreTests Debug: 1105 проверок.
- AiTests Release: 58/58, включая оба случая STRATEGY-020.
- Runtime/Editor, диагностический fixture, EditorUiTests и Commandlets собраны.
- Native UI: --camera-guides и --ai-diagnostics прошли.
- Диагностический API: preview/live/pause/clear/resume/stop и неверный request.
- Финальный исходный ZIP r5 распакован в новый каталог, собран и активирован.
- Реальный Play повторил смену строя и Arrived; radius=.4, field=ready,
  path=found, barrierViolationFrames=0. Позднее поражение от охранника —
  ожидаемый игровой исход сценария без атаки.
- Standalone Runtime: exit0, GameUpdates=60, EngineStopped.

SDK Engine.lib SHA256:
`4202492B1EFB832315DA1CD86281B9A540B5D3641011AB33BB76D6D1851B3FC2`.
Архив: ../2026-11-14-2026.11.11.0-strategy-r5/strategy/RepairCorridor-r5-source.zip,
SHA256 `8AF67B7BD0B20F24830436187B5786C66B5AA8EAC498075626E6344AD85A7C03`.
Чистая копия: Engine/build/playtests/strategy-r5-main-integration/RepairCorridor.

Логи и коды выхода сохранены рядом. Начальный diagnostics-api.exit=1 относится
к ошибке обёртки: успешный тест завершался ожидаемым native отказом с кодом2,
а обёртка принимала его за сбой. Исправлен только учёт результата тестового
скрипта. Повтор diagnostics-api-confirm и все последующие этапы прошли.
Исходные лог/код не перезаписаны. Полный сценарий повторения: verify.ps1
(передать абсолютные Output и новый Scratch; требуется локальный SDK выше).
