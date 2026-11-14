# Calm City continuation feedback

This continuation targets features the r6 matrix still marks below two independent persona attempts. The CLI reference helps locate callable APIs, but coverage is broad: several named rows require multiple distinct runtime assertions (for example audio decode vs playback vs streaming, and CPU tangent generation vs GPU/world reflection/SaveGame). A compact probe can verify concrete outcomes without implying full subsystem coverage.

The current VFS C ABI explicitly reserves mutating operations and returns `ENGINE_VFS_NOT_SUPPORTED`; that is a real documented limit for VFS writes/mount-through-host, not a failed read check. UI-native material graph and import workflows depend on hidden Qt/D3D; this persona will not use desktop UI.

## Findings and limits

- **Material graph numeric registers — fixed and verified.** Before the fix, 122 texture nodes emitted duplicate `t127`. After the fix, the 122-node overflow rejects without changing the graph; 121 unique bindings across t6..t127 (excluding t32) are accepted.
- **Sky cubemap — verified after shader fix.** With the stock Environment.Sky component, clearing the cubemap returns the gradient; Studio and Space produce distinct inspected frames.
- **Audio playback — partial.** Decode is fully observed. Play2D/Play3D/stream APIs return accepted handles and the stream update/stop lifecycle ran; no one listened to the output or confirmed spatial falloff.
- **Export — pass for validation/package smoke.** Relative project paths resolve in the staged package; runtime log mounts `/Game` under exported `Game/Content` and reads the model from that VFS mount. Source code contains no `__FILE__`. Manual source-directory removal test was not run.
- Invalid material error rendering, particle contact, tangent reflection and HUD event observations are in the evidence bundle with their actual limits; no other Engine issue was inferred from test-wrapper mistakes.
