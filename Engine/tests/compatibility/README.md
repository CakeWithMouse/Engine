# Published-release compatibility

Run `./Engine/tests/compatibility/run.ps1 -IncludeUi` from the repository root.
The default configuration is Release. The test extracts frozen binaries into
an isolated directory, verifies SHA-256, and tests the original pre-light and
light-enabled DLLs without rebuilding them. Fixture provenance and licenses
are stored in `fixtures`.

Coverage: ABI negotiation, snapshot translation, editing and saving, viewport,
Play/Stop, optional extension negotiation, engine pins and installation,
copy-only project upgrade, v1/v2 lights, rejection without loss, custom DLL
preservation. The Qt branch checks an actual legacy project through widgets,
Undo/Redo, saving and reopening.

The complete package gate is `Engine/BuildSupport/New-Release.ps1`; it also
runs the current editor suite and launches projects on bundled old runtimes.
