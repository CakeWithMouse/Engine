# Curious City r7 feedback

The project’s ordinary startup is ready to play: the clean Main scene opens with its intended city HUD, the house button charges the displayed 100, and saving/loading restores the purchased house and balance. The test model used for material-slot persistence is isolated in `SlotRenderProbe.scene.json` so it does not dominate the normal city view.

I could verify the authoring-to-runtime path in several useful ways. A real imported triangle had matching measured bounds after scale was applied; the Material Editor’s native preview showed visibly different saved WarmStone A/B materials; and the Inspector’s material-slot change survived Undo/Redo and Save/Load. OBJ MTL auto-transfer is documented as unsupported, so the imported OBJ received an existing material through its supported slot assignment.

Camera ownership is clear when edited at the source: the controlled Character’s camera settings drive actual Play projection. Changing its FOV from 40° to 80° changed the measured projection at both viewport sizes, and resizing from 4:3 to 16:9 changed aspect while preserving vertical FOV. Right-drag in the native viewport also changed the camera view while leaving scene objects fixed. The retained trace was visible in a real frame and disappeared in the matched clear-trace run.

The latest source-only archive was extracted and rebuilt from a new root. Its clean Main scene rendered the default CityHud, completed a purchase and Save/Load cycle, and mounted `/Game` from that new root’s `Content`. The separate slot-render scene preserves the multi-material test. OBJ MTL transfer remains a documented product limit; seven independent GPU matrix checks are still assigned to the second author.
