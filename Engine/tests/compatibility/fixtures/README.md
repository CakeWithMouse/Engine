# Frozen release modules

These ZIPs contain actual shipped Windows x64 Release DLLs and their Assimp
dependencies. Tests extract them; they must never rebuild or refresh these DLLs.

- `pre-lights.zip`: `PainEngine-2026-10-24-win64.rar`, SceneProject Game.dll
  SHA256 `0B26410EF32692CAD532CAB3FA6AE8265B702FA5C89534DA8D58B78CC88F1500`.
  Edit API 4, Scene API 2. Source baseline: `6f72ec3d`.
- `lights.zip`: `PainEngine-2026-10-24-win64-lights.rar`, SceneProject Game.dll
  SHA256 `59E5932B978BC8200CACAC806B63CA1307962C1D548613C9A2C6B14B301637EF`.
  Edit API 5, Scene API 3. Source: `83aca8b6`.

Each archive includes SHA256SUMS.txt for all DLLs. Preserve both fixtures when
adding another release. Dependency notices are included in `Licenses`.
