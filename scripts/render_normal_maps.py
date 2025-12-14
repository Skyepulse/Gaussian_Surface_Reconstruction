# This is another blender script intended to render the normal maps of all the meshes in the scene
# (they must be properly aligned and centered already). The script generates
# a set of camera views placed on a sphere around the origin, and for each mesh,
# it renders the normal map from each view using a special material that encodes
# the normals in camera space. The output normal maps can then be compared across meshes.

import bpy
import os
import math
import random
from mathutils import Vector

NUM_VIEWS = 10
IMG_RES = 800
BASE_RADIUS = 14.0
RNG_SEED = 42

OUTPUT_ROOT = bpy.path.abspath("//tmp/per_mesh_normals")

random.seed(RNG_SEED)

scene = bpy.context.scene

scene.render.engine = 'CYCLES'
scene.cycles.device = 'GPU'

scene.render.resolution_x = IMG_RES
scene.render.resolution_y = IMG_RES
scene.render.resolution_percentage = 100

scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.color_depth = '16'

scene.view_settings.view_transform = 'Raw'

# =============================================================
# NORMAL MATERIAL (CAMERA SPACE)
# =============================================================
def make_normal_material():
    mat = bpy.data.materials.new("NORMAL_CAM_MAT")
    mat.use_nodes = True

    n = mat.node_tree.nodes
    l = mat.node_tree.links
    n.clear()

    geom = n.new("ShaderNodeNewGeometry")
    xform = n.new("ShaderNodeVectorTransform")
    mul = n.new("ShaderNodeVectorMath")
    add = n.new("ShaderNodeVectorMath")
    emit = n.new("ShaderNodeEmission")
    out = n.new("ShaderNodeOutputMaterial")

    xform.vector_type = 'NORMAL'
    xform.convert_from = 'WORLD'
    xform.convert_to = 'CAMERA'

    mul.operation = 'MULTIPLY'
    mul.inputs[1].default_value = (0.5, 0.5, 0.5)

    add.operation = 'ADD'
    add.inputs[1].default_value = (0.5, 0.5, 0.5)

    emit.inputs["Strength"].default_value = 1.0

    l.new(geom.outputs["Normal"], xform.inputs["Vector"])
    l.new(xform.outputs["Vector"], mul.inputs[0])
    l.new(mul.outputs["Vector"], add.inputs[0])
    l.new(add.outputs["Vector"], emit.inputs["Color"])
    l.new(emit.outputs["Emission"], out.inputs["Surface"])

    return mat

NORMAL_MAT = make_normal_material()

# =============================================================
# HELPERS
# =============================================================
def random_spherical(radius):
    phi = random.uniform(0.0, 2.0 * math.pi)
    costheta = random.uniform(-1.0, 1.0)
    theta = math.acos(costheta)

    x = radius * math.sin(theta) * math.cos(phi)
    y = radius * math.sin(theta) * math.sin(phi)
    z = radius * math.cos(theta)

    return Vector((x, y, z))

def override_material(obj, mat):
    old = list(obj.data.materials)
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    return old

def restore_material(obj, mats):
    obj.data.materials.clear()
    for m in mats:
        obj.data.materials.append(m)

if "LookAt" not in bpy.data.objects:
    tgt = bpy.data.objects.new("LookAt", None)
    bpy.context.collection.objects.link(tgt)
    tgt.location = (0, 0, 0)
else:
    tgt = bpy.data.objects["LookAt"]

# HEre is wre we gather all the meshes in the scene
meshes = [
    obj for obj in bpy.context.scene.objects
    if obj.type == 'MESH'
]

if not meshes:
    raise RuntimeError("No mesh objects found in scene")

os.makedirs(OUTPUT_ROOT, exist_ok=True)

print(f"Found {len(meshes)} meshes")

# Generate the N camera positions once for all meshes
CAMERA_POSITIONS = [
    random_spherical(BASE_RADIUS)
    for _ in range(NUM_VIEWS)
]

print(f"Generated {NUM_VIEWS} shared camera positions")


for mesh_idx, mesh in enumerate(meshes):
    print(f"\nProcessing mesh {mesh_idx+1}/{len(meshes)}: {mesh.name}")

    mesh_out = os.path.join(OUTPUT_ROOT, mesh.name)
    os.makedirs(mesh_out, exist_ok=True)

    # Enable only this mesh for rendering
    for obj in meshes:
        obj.hide_render = (obj != mesh)

    old_mats = override_material(mesh, NORMAL_MAT)

    for i, loc in enumerate(CAMERA_POSITIONS):
        cam_data = bpy.data.cameras.new(f"Cam_{i}")
        cam_obj = bpy.data.objects.new(cam_data.name, cam_data)
        bpy.context.collection.objects.link(cam_obj)

        cam_obj.location = loc
        cam_obj.rotation_mode = 'XYZ'

        c = cam_obj.constraints.new("TRACK_TO")
        c.target = tgt
        c.track_axis = 'TRACK_NEGATIVE_Z'
        c.up_axis = 'UP_Y'

        scene.camera = cam_obj
        scene.render.filepath = os.path.join(
            mesh_out, f"normal_{i:03d}.png"
        )

        bpy.ops.render.render(write_still=True)
        print(f"  Rendered view {i+1}/{NUM_VIEWS}")

        bpy.data.objects.remove(cam_obj, do_unlink=True)

    restore_material(mesh, old_mats)

print("\nAll meshes processed. Normal maps saved in:", OUTPUT_ROOT)