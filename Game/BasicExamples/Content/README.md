# BasicExamples content
The scenes use procedural geometry and EngineContent shaders. No external game assets are required.

Materials/ holds the material assets (.mat + surface .hlsl) of the "Материалы" scene. The copies next
to the exe (GameContent/Materials) are read at start-up: edit one and restart the game.

Scripts/ holds the visual scripts (.vscript) of the "Скрипты" scene: Spin turns a cube, Bob floats a sphere,
Move moves a cube with the Left/Right arrows and returns it on Space. They are read at start-up too
(GameContent/Scripts next to the exe); the format is described in Engine/docs/VISUAL-SCRIPTING.md.
