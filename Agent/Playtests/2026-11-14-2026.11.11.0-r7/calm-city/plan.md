# Calm City r7 — план закрытия partial

Версия кандидата: PainEngine 2026.11.11.0, main `6899a6a` (Engine.lock будет записан после upgrade). Исходный проект для upgrade: копия r6 `Engine/build/playtests/20261114-r7/calm-city/LegacyCalmCityInput`; оригиналы r5/r6 не меняются. Созданная fresh копия и upgrade-копия живут в scratch этого запуска.

## Недостающие критерии из r6

Первые 32 ID с предыдущими статусами/свидетельствами — в `baseline-gaps.json`. Основные блоки фактической работы:

1. **Город/ассеты/материалы:** импортировать или повторно использовать модель с картой UV, анимируемый skeletal объект и инстансный ряд; наблюдать LOD на разных дистанциях, material parameters/blend, point light/shadows. Отдельно проверить slots после edit, undo/redo, save/reopen; ошибочный material и исправление; граф на собственном материале save/preset/apply/revert.
2. **Рендер/кадр/сохранение:** own runtime request для Forward/Deferred, resize как минимум 2 размера, diagnostics/LOD panel, completed frame; отдельный derived SaveGame state и scene hierarchy в отдельном процессе.
3. **Тени/прозрачность:** point-shadow cubes на двух лампах, shadow cascade fit и camera motion на runtime sequence, LOD transition с возвратом; прозрачная сцена с перекрывающимися слоями, пересечением, анимированным LOD и resize, representative BMP плюс параметризованные результаты.
4. **Звук:** не считать handles pass. Уникальная кодированная PCM последовательность, WASAPI shared loopback захват endpoint до/во время/после. Correlation подтверждает 2D; 3D сравнивает одинаковый источник near/far при фиксированном listener; streaming проверяется в нескольких временных окнах и после Stop. Отсутствие устройства/тишина системы — blocked, чужой сигнал отсекается корреляцией.
5. **AI редактор:** 4 сценария в `baseline-gaps.json`; для карточки/графа/ошибки записи нужен реальный UI и save/reopen. Runtime debug — кадры движения плюс snapshots active-state/memory/history. Для внешней файловой правки фиксировать write-failure и retry отдельно.

До READY commandlets: сохранять requests, source/fixture, создать свежий проект/upgrade пока только через Launcher, build C++ project через текущий Runtime SDK. Old EngineCommandlets блокируются до явного сообщения координатора. Не менять общие Engine/Editor/SDK.

## Audio helper request

В `Engine/build/playtests/20261114-r7/calm-city/requests/audio-loopback-protocol.md` описан вариант общего read-only capture helper. Сценарии и пороги остаются независимыми для каждой роли.
