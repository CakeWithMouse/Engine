# Feedback

- Imported source slots are now testable through the normal scene workflow. The city prop retains its separate material assignments after scene save/reopen and its two-slot map survives SaveGame restoration on .8.
- The import-to-game path is useful for a thrown prop: the imported model renders in-flight, and a save can restore its geometry, slot map, and trajectory. A larger default projectile scale or a capture camera closer to the item would make the UV/material review more legible.
- The .7 restore failure was concrete and reproducible: the saved derived Skeletal component had a valid model and two slot references but failed with `Material slots do not match model`. The exact same .7 save passed on .8, including a resumed hit.
- Broad assigned features without isolated r3 evidence are explicitly left untested; see `evidence.json`.
