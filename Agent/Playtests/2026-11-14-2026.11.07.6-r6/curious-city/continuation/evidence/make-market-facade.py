import bpy
from mathutils import Vector
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
mesh=bpy.data.meshes.new("FacadeMesh")
mesh.from_pydata([(-1,0,0),(1,0,0),(0,2,0)],[],[(0,1,2)])
mesh.update()
uv=mesh.uv_layers.new(name="UVMap")
for poly in mesh.polygons:
    for li in poly.loop_indices:
        vi=mesh.loops[li].vertex_index
        uv.data[li].uv=[(0,0),(1,0),(.5,1)][vi]
obj=bpy.data.objects.new("MarketFacade",mesh)
bpy.context.collection.objects.link(obj)
material=bpy.data.materials.new("MarketFacadeMaterial")
material.use_nodes=True
nodes=material.node_tree.nodes
links=material.node_tree.links
bsdf=nodes.get("Principled BSDF")
image=bpy.data.images.load(r"E:\Engine\Engine\build\playtests\20261114-r6\curious-city\continuation\city-copy\Content\Models\blend-suzanne-multiple\Textures\BaseColor_0.png",check_existing=False)
image.pack()
texture=nodes.new("ShaderNodeTexImage")
texture.image=image
links.new(texture.outputs["Color"],bsdf.inputs["Base Color"])
mesh.materials.append(material)
bpy.context.view_layer.objects.active=obj
obj.select_set(True)
bpy.ops.wm.save_as_mainfile(filepath=r"E:\Engine\Engine\build\playtests\20261114-r6\curious-city\continuation\blend-source\MarketFacade.blend")
