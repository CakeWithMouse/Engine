# Curious City feedback — continuation

The workflow now supports a useful path from choosing an import scale to writing a sidecar and placing the model. The three scale presets are easy to distinguish, and Cancel leaves both the project assets and current scene alone. The actual UI Editor and module reload also let me change a HUD label without breaking the build-house click.

The scene tools were enough for a small city prototype: primitives, grouping, parent changes, snapping, Save/New/Load, startup scene, and recovery behaved predictably. A generated project object survived duplication and reload. The project API makes a lot of this possible with short, direct code rather than a separate city framework.

Material authoring has a useful recovery story: a failed shader is visible as a checker, and fixing the shader restores the model without changing its assignment. The graph editor also kept the applied graph intact when its sidecar was locked and let me retry after unlocking. The large texture graph boundary behaved cleanly at 121 slots and refused the 122nd before changing the graph.

The main remaining friction is interaction discoverability: this persona could verify named selection and slot editing through host operations, but did not get evidence for viewport picking/marquee/focus or for changing a material slot through the Inspector itself. The imported UV model was in the game scene but not visible in the captured frame, so a clear small import preview scene would help the author confirm mapping. The HUD label edit was saved and its action worked; the label layout still deserves a visual check at normal editor/gameplay width.
