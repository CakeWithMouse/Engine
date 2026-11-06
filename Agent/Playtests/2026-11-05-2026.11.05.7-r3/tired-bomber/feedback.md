# Feedback — tired bomber

The importer's source IDs are straightforward once I inspected the returned slot map, and the C++ map makes it possible to avoid assuming IDs are contiguous. Setting one material no longer flattens the whole model. The distinction between a per-slot path and the shared Material override is clear in the project scene result, and clearing the shared override restored the slots. Saving and reopening kept the per-slot assignment.

The importer-created Material files were useful to tune individual submeshes. On a paused, fixed camera, mask and castShadows changed the actual D3D image in a way I could judge. A mixed-material target fits this small game: one piece could disappear or stop casting a shadow without making a second scene object.

The first useful UV frame took manual work: I had to rotate the imported control plane so the orthographic camera could see its face. The API accepts the model, but the importer's frame orientation was not obvious from the asset name alone. I wanted a small import-preview thumbnail plus axes/UV hints before spending time debugging a blank or edge-on result.

The real game flow worked, but this was still more setup code than I wanted after a long day: import, scene placement, hand-tuning two material files, then launch. The game did show a real bomb hit, readable score label, frame counters and LOD panel. The scene commandlet made the save/reopen and slot assignment checks repeatable without opening the desktop editor.

Priority: show imported material-slot names and preview swatches on the model before scene placement; keep slot ID and blend/shadow state visible near each assignment; make it easy to aim/rotate the import preview. This would help me identify the right piece and catch a hidden/masked slot before I build the whole game loop.

Final .8 follow-up: after the upgrade, a fresh process restored both source-material assignments from the hit SaveGame; the scene roundtrip also preserved the variant slot. The narrow retest closes my persistence concern for this project. No Engine source was changed.
