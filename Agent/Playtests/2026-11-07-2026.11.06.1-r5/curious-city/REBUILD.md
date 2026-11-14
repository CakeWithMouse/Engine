# Curious City r5 — rebuild

## Archive contents

`CuriousCity-r5-source.zip` contains only the game module `Source/` and the project `Content/` needed by the current city scene: `Main.scene.json`, HUD, settings, materials and the small imported Suzanne model. It excludes Engine, SDK, build outputs, Intermediate, Game.dll and SaveGame slots.

The archive derives from the r3 Curious City source retained as `CuriousCity-r3-source.zip`. The source archive reflects the tested city scene, including two point lights configured with `castShadows=true`. Runtime/SDK version required for this replay: `2026.11.07.0`.

## Create a clean project

Use a Launcher distributed beside the matching `Runtime/` directory. The example below uses the checked build at `E:/Engine/Game/build/x64/Release/`. Choose a new empty path that does not exist yet, and a private engine store.

```powershell
$env:PAINENGINE_ENGINE_STORE = 'E:/scratch/CuriousCity/EngineStore'
$launcher = 'E:/Engine/Game/build/x64/Release/EngineLauncher.exe'
$runtime = 'E:/Engine/Game/build/x64/Release/Runtime'
$project = 'E:/scratch/CuriousCity/fresh-city'

& $launcher --install-engine $runtime
& $launcher --create-project $project 'CuriousCityR5'
& $launcher --enable-code "$project/Project.json"
```

Extract `CuriousCity-r5-source.zip` to a temporary folder, then merge its `Source/*` and `Content/*` into the corresponding folders in the generated project. Copy the contents into those existing folders; do not add an extra `Source/Source` or `Content/Content` layer. Keep the generated `Project.json`, `Engine.lock.json`, solution, project file, and build scripts.

```powershell
Copy-Item 'E:/scratch/CuriousCity/archive/Source/*' "$project/Source" -Recurse -Force
Copy-Item 'E:/scratch/CuriousCity/archive/Content/*' "$project/Content" -Recurse -Force
& $launcher --build-project "$project/Project.json"
& $launcher --check-project "$project/Project.json"
& $launcher --smoke-project "$project/Project.json"
```

For an existing older project, build its own game module against its pinned SDK before `--upgrade-project`; otherwise custom scene types such as `CuriousCity.House` cannot be loaded during migration. Upgrade into a new path and keep the original:

```powershell
& $launcher --upgrade-project 'E:/scratch/CuriousCity/old/Project.json' 'E:/scratch/CuriousCity/final-city'
& $launcher --build-project 'E:/scratch/CuriousCity/final-city/Project.json'
```

## Reproduction evidence

The exact `project.input`, `project.scene`, and `mesh.tangents` request/results plus before/after light scene documents are copied under `evidence/`. Runtime artifacts under `Engine/build/playtests/20261107-r5/curious-city/` contain the runnable project and its private SaveGame files. The input cycle changed the balance from 500 to 300 after two purchases, then restored the saved 300/2 state. The longer automated cycle saved 400/1, loaded 400/1, and advanced income to 410/1.

The fixed commandlet test preserves `castShadows` for a cloned point light across duplicate, save, new scene and load. Its pre-fix result remains available as `evidence/point-light-before.*`. No moving-camera cascade image, normal-map GPU frame, or point-light shadow screenshot was captured; visual behavior remains unverified.
