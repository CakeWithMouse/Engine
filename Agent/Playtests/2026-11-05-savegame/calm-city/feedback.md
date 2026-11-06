# Calm City SaveGame feedback

The game-side API is compact for a small project: one serializer override stores the car's own speed/motion fields, while the engine handles its position and the scene. F6/F9 plus the `manual-1` slot provide a simple control path; actual persistence was verified through the same service in two separate processes. The saved route direction continued after loading.

Manual editor/game keyboard input was not confirmed because the desktop control request timed out. The test did not claim that a headless smoke alone proves visual behavior: FrameDump captures from both same-process and new-process runs show the city. There is no in-game save status yet, so a player cannot see whether a request was accepted or completed. F5 also conflicts with an existing engine hotkey; this sample uses F6. A shared, documented hotkey configuration would make conflicts easier to avoid.
