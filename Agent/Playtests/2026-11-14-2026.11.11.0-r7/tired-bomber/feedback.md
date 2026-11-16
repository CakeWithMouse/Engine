# Feedback — r7 preparation

The AI-FSM file format and tutorial give a useful end-to-end example: a guard’s hierarchical states, typed memory, event transitions, script-side Noise input and script callback on capture. The `Engine.Scene.AiMachine` extension exposes the state, memory and history needed to check whether the debugger matches gameplay. An invalid `Ai.Wait` node can be a targeted validation probe with a direct repair path.

Audio checks must measure loopback output. Play2D/Play3D/PlayStream success handles alone do not establish audible playback, and the 3D case also needs a near/far A/B with a fixed listener and source. I will use separate coded WAV parameters and preserve baseline/capture results.

The old project upgraded into a new copy, but its copied code metadata still identified SDK .0. An explicit .11 `--build-project` was required before the project became a current-r7 compatibility check. The source project remained unchanged.

## Root fix review: AI tests on clean checkouts
un.ps1 -Configuration Release -NoBuild passed 52/52 in the existing binary. The checked-in guide-generated translation unit remains in AiTests.vcxproj. Default run does not require unavailable docs source; explicit -RefreshGuide still requires it and fails during Resolve-Path if absent. The script invokes the test executable with the optional Filter only and throws on nonzero exit; no coverage bypass was found. Log: evidence/ai-run-release-nobuild.log. Debug NoBuild could not run because Debug/AiTests.exe is absent; no build performed.

AI editor save/retry observation: actual write lock yields an access-denied status with unchanged disk state; later retry succeeds and reopen sees updated hash/settings. Hidden ai.editor E injection did not trigger vscript; this is recorded as unverified input behavior, not an engine bug.

### Audio3D startup A/B
Private source-only A/B supports a startup ordering issue: without an immediate AudioEngine::Update(), first spatialization is delayed until the first game frame (~1.2 s after Play3D) and near/far captures are similar; immediate Update makes near measurably attenuated and far2000 silent. Root is fixing Play3D order; follow-up evidence pending.

### R7-BUG-004 independent repeat
After root's Play3D-before-Start fix, two independent no-workaround near2m/far2000 startup pairs showed a near coded signal and silent far source. A steady-state case started at frame10 after listener update; Player=(0,24,0), near emitter (0,24,2) yielded 14 correlated 100ms windows, far (0,24,2000) yielded none. Zero baseline and capture tail. Startup QPC timing still records ~1.2s to first PreUpdate; spatial attenuation is now applied before audible start. Raw waveforms and analysis are in evidence/.

### Independent graphics checks

- Skeletal animation: direct public time setting plus `Tick(0)` produced stable known-pose captures; reset-to-zero returned both bone-palette checksum and character pixels exactly to baseline. The screenshots show pose changes on the imported CesiumMan.
- Instanced mesh: moving and hiding distinct entries changed separate pixel regions and the component count; restoring returned the captured scene and counts exactly. The moved marker is visible outside the editor overlay.
- LOD: the project markers selected LOD2/LOD1/mixed LOD0+1 at the recorded camera positions and restored the LOD2 image on return. Current captured threshold panel/counts are useful when diagnosing an apparent LOD issue; the frames intentionally freeze simulation time to isolate camera/instance effects.

Analysis script and outputs are in `evidence/analyze-graphics-followup.py` and `evidence/graphics-followup-analysis.json`; runtime logs and inspected BMPs are listed under the new `skeletal`, `instances`, and `lod` feature IDs.

### Independent B graphics observations

Confirmed masked transparency has a visible half-texture hole, and the same real material layers produce exact source-over center RGB in both supported Forward and Deferred paths. Point-light shadow A/B is visible on the receiver; moving the light shifts the measured shadow mask and returning restores an identical frame. The imported animated model casts its own measurable receiver shadow. An imported UV mesh using its existing imported BaseMap binding shows a known four-color chart in a project runtime frame. These scenarios completed without engine-side failures or new defect reports; their numbers and captures are indexed in evidence.json.
