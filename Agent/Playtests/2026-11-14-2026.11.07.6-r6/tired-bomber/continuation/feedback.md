# Feedback — tired bomber r6 continuation

The parameterized commandlets exposed both broad Editor workflows and focused game APIs. `project.scene` snapshots, `project.build` results, commandlet requests, and emitted images provide concrete evidence of each operation. Two initial mistakes were mine: unsupported scene type IDs and invalid C++ drafts; the engine rejected them with clear errors, and the corrected calls worked.

The real class wizard and import dialog gave clear validation. Three scale presets returned 1, 0.1, and 0.01; accepting a custom 0.025 scale wrote a sidecar and spawned at that scale, while Cancel left the scene unchanged. Startup scene choice and unsaved recovery persisted across separate processes.

A custom SDK game verified a moving character/controller, an orthographic screen ray that hit the ground after adjusting the camera, subsystem lifecycle, a D3D render hook with device/context, VFS reads, WAV decoding, audio load/play/stream calls, HUD callback, and SaveGame save/reopen. Audio APIs accepted requests, but hidden playback did not establish audible output. The first SaveGame attempt correctly rejected a raw component lacking a registered type; the retry removed transient objects and used catalog-backed components, then saved and loaded successfully.

Custom GPU particle captures showed identical early frames with collision on/off, different late frames after surface contact, and identical no-surface controls. Transparency and moving point-shadow cases were also captured in this role’s own requests. These are hidden GPU commandlet renders, not a visible editor usability check.

The actual editor Hot Reload action preserved the previous DLL after an injected compiler error, then activated a new module after source repair. The failed compiler request and successful retry each record the build identity and DLL hash. Sandbox MSVC linking can fail with `LNK1101`/MSPDB140; the permitted isolated retry completed successfully.

No visible editor session was run in this continuation. Hidden widget/action checks prove state changes, not visual discoverability or manual usability. The evidence matrix marks single-profile SDK checks partial where another independent practical configuration is still needed.
