# AI ↔ visual script link (own r7 run)

## Setup

Copied tutorial `Player.vscript` to private `Content/Scripts/CityPlayer.vscript`, changed its `Object.Find` target to `CityGuard`, and added a `FromAI` custom-event node connected to an `Action.Print` node with message “CityGuard responded to Noise”. This is an own project fixture; the shared tutorial stayed unchanged. The city FSM was edited through the actual AI editor: `NoiseAt` variable, Idle `Ai.Wait` (100 s), Investigate `Ai.CallScriptEvent` (`FromAI`), Idle→Investigate event `Noise`, and Investigate→Idle completed. The saved scene attaches the script to CityGuard and sets it as project startup.

## Run

Request/result/log: `requests/ai-script-link5.request.json`, `results/ai-script-link5.result.json`, `logs/ai-script-link5.log`. In the actual project host I started Play, warmed up 20 frames at 0.05 s, pressed E, advanced frames, released E, and advanced 20 more frames. The real visual script logged both effects:

- `[Script] /Game/Scripts/CityPlayer.vscript: Шум! Охранник идёт проверить.`
- `[Script] /Game/Scripts/CityPlayer.vscript: CityGuard responded to Noise`

The runtime AI extension showed CityGuard's `NoiseAt` vector and `Idle -> Investigate event Noise`, followed by `Investigate -> Idle task succeeded`. CityGuard 2 stayed Idle with its own Speed memory. Inspected frame: `frames/ai-script-live-cityguard2.png` (shows assigned script/FSM, `NoiseAt`, and both transitions in the selected character's live card). The CityGuard card was selected before capture and Stop followed it.

## Retained setup mistakes

- Initial `project.scene` load failed with “Unexpected or missing JSON fields” because the hand-edited object `assets` fixture only contained `script`; scene format also requires empty `model` and `texture` strings. This was fixed in the private fixture before the successful run.
- A run before `save`+`startup` used the old AiProbe scene; root cause was the test setup not marking CityNoise as startup. The proper load→save→Set Startup Scene action returned `Content/Scenes/CityNoise.scene.json`.
- One commandlet screenshot path was reused and was rejected as not new; the fresh-path retry is the result above. None of these are engine defects.
