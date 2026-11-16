# Calm City r7 feedback

## What worked well

- The AI editor flows were inspectable when real windows, modal states, transitions, save/reopen and actual GPU output were recorded together. Graph serialization was stable across reopen.
- Import settings scale and reimport produced a predictable 0.01 scene scale. The imported UV chart made mapping errors easy to see, and the actual material-slot Inspector field supported Undo/Redo and save/reopen.
- Point-shadow slot availability and pixel-height LOD transitions were observable from actual frame readbacks. The LOD sequence correctly returned to its original bucket and shadow slot after moving the camera back.
- The transparency review became decisive once it used own crossing-plane geometry, camera rays, geometric depth sorting, source-over RGB expectations and multiple interior pixels. The test deliberately does not claim same-mesh self-intersections.
- The corrected BasicSky cubemap path, graph application and 3D audio start fix were exercised in the wider r7 campaign. Audio checks use external loopback/correlation; no claim of subjective listening is made here.

## Project issue found and fixed

My game module used `__FILE__` to locate SaveData, audio logs and frame dumps. It therefore wrote to the source project after the project was relocated even though `/Game` mounted the new Content directory. The r7 project now resolves `/Game` via the public module VFS and writes beside that content root. A new process launched from a separate root saved, loaded, rendered and restored both material slot `/Game` paths and `CalmScoreSubsystem` state.

## Coverage limits

The point-shadow skeletal check used the real animated `CesiumMan.glb` fixture. The transparency suite checks crossing planes, not self-intersection inside one mesh. Audio values are loopback measurements only; they do not certify subjective sound quality.
