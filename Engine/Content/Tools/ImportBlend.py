"""Run by the editor in Blender's background process; no add-on installation needed."""
import os
import sys

import bpy


def main():
    source, destination = sys.argv[sys.argv.index("--") + 1:]
    if bpy.app.version < (4, 2, 0):
        raise RuntimeError("BLEND import requires Blender 4.2 or newer")
    bpy.ops.wm.open_mainfile(filepath=source, load_ui=False, use_scripts=False)
    # Do not silently turn a missing linked image into a white exported material.
    # Procedural node graphs need baking before export; the basic importer uses Base Color.
    used_materials = {slot.material for obj in bpy.context.scene.objects if obj.type == "MESH"
                      for slot in obj.material_slots if slot.material}
    for material in used_materials:
        if not material.use_nodes:
            continue
        for node in material.node_tree.nodes:
            if node.type != "BSDF_PRINCIPLED":
                continue
            socket = node.inputs["Base Color"]
            if not socket.is_linked:
                continue
            upstream = socket.links[0].from_node
            while upstream.type == "REROUTE" and upstream.inputs[0].is_linked:
                upstream = upstream.inputs[0].links[0].from_node
            if upstream.type != "TEX_IMAGE" or upstream.image is None:
                raise RuntimeError(f"Material '{material.name}': bake Base Color to an image before import")
            image = upstream.image
            if image.source not in {"FILE", "GENERATED"}:
                raise RuntimeError(f"Material '{material.name}': only single Base Color images are supported")
            if not image.packed_file and image.source == "FILE":
                path = bpy.path.abspath(image.filepath, library=image.library)
                if not os.path.isfile(path):
                    raise RuntimeError(f"Missing base-color texture: {path}")
    result = bpy.ops.export_scene.gltf(
        filepath=destination, export_format="GLB", export_image_format="AUTO",
        export_materials="EXPORT", export_texcoords=True, export_normals=True,
        export_cameras=False, export_lights=False,
    )
    if "FINISHED" not in result:
        raise RuntimeError("Blender could not export the model to GLB")


if __name__ == "__main__":
    main()
