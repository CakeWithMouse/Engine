# AI graph editing scenario (own r7 run)

Request: `Engine/build/playtests/20261114-r7/curious-city/requests/ai-graph-edit2.request.json`. Result: `Engine/build/playtests/20261114-r7/curious-city/results/ai-graph-edit2.result.json`. The request used the current 2026.11.11.0 `EngineCommandlets.exe` against the upgraded city copy.

I created `CityGate2.fsm` through the production `EditorMainWindow::CreateAiMachine` path, not by writing the graph JSON. In the actual `AiMachineEditorWindow`, I added a Patrol state with an `Ai.Wait` task, linked Idle to Patrol with a completed trigger, declared Speed=1.75 and LastSeen, and changed target/last-seen/think interval settings. Save and graph undo/redo returned status 0 with no keyed errors; I then closed and reopened the window. Reopen retained two states, one link, all three variables and settings. The final on-disk JSON is at `upgraded-city/Content/AI/CityGate2.fsm`; reopened-window image is `frames/ai-citygate2-reopened.png` and was visually inspected: both state nodes and connection are visible, variable table/settings agree with the file, and the problem strip reports no errors.

One setup attempt is retained: `ai-graph-edit.request.json` set top-level `machine` before the first `create-file` step, so the commandlet opened the absent file first and got a read-only/missing-file window; later graph edit correctly failed `ready graph required`. The fixture creation itself did succeed. This was a request-order error; the retry removed the initial machine and created `CityGate2`, after which the scenario passed. The failed attempt is not counted as a product defect.

Boundary: actual node/link dialog editing is pending the coordinator's dialog action relink. This scenario exercises the real graph widget operations and real AI editor window, not modal state/link dialogs.

## Actual state/link modal edits

Request/result/log: `requests/ai-dialog-edit.request.json`, `results/ai-dialog-edit.result.json`, `logs/ai-dialog-edit.log`; UI frames: `frames/ai-state-dialog-cancel.png`, `frames/ai-state-dialog-accepted.png`, `frames/ai-link-dialog.png`, `frames/ai-dialog-reopen.png`.

Through real QGraphics double-clicks and modal dialogs, I cancelled a temporary state rename (the graph remained `Patrol`), then renamed it `RouteB`, added an `Ai.Wait` task, and set the Idle→RouteB completed link delay to 0.25 and priority to 3. Each result step returned status 0; the final save/reopen retained the route name, two tasks, link delay/priority, and no diagnostics. I inspected both dialog captures and the reopened editor frame. This adds actual dialog input to the earlier widget-level graph scenario. The second added Wait serialized with no explicit `seconds` parameter; I did not treat that alone as a defect because the block schema supplies a default and the machine checker reported no errors. Runtime behavior remains to be verified in an assigned AI host scenario.
