# Battle content
Materials/ holds the material assets of the units (Vehicle.mat) and of the scenery (Building.mat).
The copies next to the exe (GameContent/Materials) are read at start-up: edit one and restart the
game. `--legacy-shaders` draws the same scene with the old InstancedMesh shader for comparison.

The unit geometry itself is procedural and uses the shared EngineContent shaders. Designer models
drop in here and plug in through the contract in Engine/docs/UNIT-RENDERING.md.
