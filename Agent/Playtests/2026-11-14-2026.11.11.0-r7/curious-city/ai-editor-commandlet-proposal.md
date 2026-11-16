# AI editor UI commandlet proposal (read-only source review)

The command should instantiate the real project MainWindow/EditorQtHost, current GameEditModule with `Engine.Scene.AiMachine`, and real `AiMachineEditorWindow`; it should drive public widget operations rather than manually writing an `.fsm` JSON shortcut. Keep each step parameterized and return per-step observations.

## Minimal action surface

- Session/card: `select` object, `createAiCharacter`, `card.create` (uses real Create button), `card.open`, `card.assign`, `card.clear`, `card.memory.set`, `card.memory.reset`, `duplicate`, `scene.save`, `scene.new`, `scene.load`.
- Graph: `graph.addNode(type,position,properties)`, `graph.connect(fromNode,fromPin,toNode,toPin)`, `graph.setNodeProperties`, `graph.setLinkProperties`, `graph.removeNodes`, `graph.undo`, `graph.redo`, `graph.frame`.
- Dialogs: optionally `stateDialog` and `linkDialog` to drive the actual dialogs/typed `FsmBlockListEditor` controls; return accepted/cancelled, validated fields and changed properties. This lets the persona prove field types and UI operations, while graph steps still cover deterministic graph structure.
- Left panel: `variables.add`, `variables.set(rows,renames)`, `variables.remove`, `settings.set(targetKey,lastSeenKey,thinkInterval)`; then `saveNow` waits for file commit and engine check.
- Validation/runtime: `check`, `play`, `frames`, `stop`; include the card problems/live labels, exact active state/memory/recent transitions, Play state and dialogs.
- Lifecycle: `close`, `reopen`, and optionally `lockFile` for actual `.fsm` QSaveFile failure/retry. Locked-file result must keep dirty graph and later retry successfully.

## Required result schema

Top-level `ok`, `engineVersion`, project and machine path. Each `steps[]` item reports `action`, `accepted`, `error`, `selectedObject`, scene `projectData` (`stateMachine`,`machineMemory`), window (`ready`,`readOnly`,`checkCount`,`status`), graph JSON, node-keyed `problems`/`warnings`, `canUndo/canRedo`, exact on-disk `.fsm` hash/content after save, Play state, live card text and recent transitions. For visual steps save the real window/frame image; a blank/uninspected image is not visual pass.

`SaveNow()` calls atomic `QSaveFile::commit()` then queues the engine `check`; scenarios must wait until `CheckCount` increments before comparing diagnostics. `ai.editor` can use `FsmGraphWidget::AddNode/Connect/SetNodeProperties/SetLinkProperties/SetVariables/SetSettings/Undo/Redo`, but card create/memory and state/link dialog actions should route through their real widgets so they do not overstate UI coverage.

For `ai-script-link`, a generic `.fsm` edit is insufficient by itself: the scenario needs the actual `.vscript` event and a logged or rendered counter/position change after `Noise`, plus the reverse `OnCaught` effect if the fixture supports it.
