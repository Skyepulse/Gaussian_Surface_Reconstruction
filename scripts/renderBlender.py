# This is the blender script we wrote to be used inside Blender's scripting environment
# to render images of the mesh inside the Blender scene from multiple viewpoints.
# We convert it to a colmap compatible dataset.

import bpy
import random
import math
import os
from mathutils import Matrix, Quaternion, Vector

DEBUG_POINTS = False
RENDER = True

output_path = bpy.path.abspath("//tmp/blender_colmap_output")
sparse_path = os.path.join(output_path, "sparse", "0")
images_path = os.path.join(output_path, "images")
normals_path = os.path.join(output_path, "normals")

os.makedirs(sparse_path, exist_ok=True)
os.makedirs(images_path, exist_ok=True)
os.makedirs(normals_path, exist_ok=True)

num_cameras = 100
img_width = 800
img_height = 800

scene = bpy.context.scene

scene.render.film_transparent = True
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.color_depth = '16'
scene.render.image_settings.file_format = 'PNG'

scene.render.engine = 'CYCLES'
scene.cycles.device = 'GPU'

# We took same resolution as Truck dataset
scene.render.resolution_x = img_width
scene.render.resolution_y = img_height
scene.render.resolution_percentage = 100

scene.view_settings.view_transform = 'Raw'

# Camera intrinsics for the colmap dataset
cam_data_ref = bpy.data.cameras.new("COLMAP_CAM")
f_mm = cam_data_ref.lens
sensor_width = cam_data_ref.sensor_width

fx = (f_mm / sensor_width) * img_width
fy = fx
cx = img_width / 2.0
cy = img_height / 2.0

# Write cameras.txt
with open(os.path.join(sparse_path, "cameras.txt"), "w") as f:
    f.write("# Camera list with one line of data per camera:\n")
    f.write("# CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]\n")
    f.write("# Number of cameras: 1\n")
    f.write(f"1 SIMPLE_PINHOLE {img_width} {img_height} {fx} {cx} {cy}\n")


# -------------------------------------------------------------
# HELPER FUNCTIONS
# -------------------------------------------------------------
def random_spherical(base_radius=20.0):
    phi = random.uniform(0, 2*math.pi)

    costheta = random.uniform(-1.0, 1.0)
    theta = math.acos(costheta)

    # small jitter to avoid perfect spherical shell, maybe another transform trick would be better?
    r = base_radius + random.uniform(-1.0, 1.0)

    x = r * math.sin(theta) * math.cos(phi)
    y = r * math.sin(theta) * math.sin(phi)
    z = r * math.cos(theta)

    # small sideways jitter (breaks symmetry, improves COLMAP conditioning)
    x += random.uniform(-0.25, 0.25)
    y += random.uniform(-0.25, 0.25)

    return (x, y, z)

# Blender → COLMAP axis conversion
# X_colmap =  X_blender
# Y_colmap = -Y_blender
# Z_colmap = -Z_blender
BLENDER_TO_COLMAP = Matrix((
    (1,  0,  0),
    (0, -1,  0),
    (0,  0, -1),
))

def blender_cam_to_colmap_pose(cam_obj):
    M_c2w = cam_obj.matrix_world
    R_c2w = M_c2w.to_3x3()
    t_c2w = M_c2w.to_translation()

    R_w2c = R_c2w.transposed() # For rotation matrices, inverse is transpose
    t_w2c = -(R_w2c @ t_c2w)

    R_colmap = BLENDER_TO_COLMAP @ R_w2c
    t_colmap = BLENDER_TO_COLMAP @ t_w2c

    q = R_colmap.to_quaternion()
    return q, t_colmap

def make_normal_material():
    mat = bpy.data.materials.new("NORMAL_MAT")
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

def override_material(mat):
    old = {}
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue
        old[obj] = list(obj.data.materials)
        obj.data.materials.clear()
        obj.data.materials.append(mat)
    return old


def restore_materials(old):
    for obj, mats in old.items():
        obj.data.materials.clear()
        for m in mats:
            obj.data.materials.append(m)

normal_mat = make_normal_material()

def get_mesh_object(name):
    if name not in bpy.data.objects:
        raise RuntimeError(f"Mesh '{name}' not found in scene")
    obj = bpy.data.objects[name]
    if obj.type != 'MESH':
        raise RuntimeError(f"Object '{name}' is not a mesh")
    return obj

# Used to define the bounding box
# of the mesh, so to not sample the initial points on unused volume
# Better for gaussian splatting initialization later
def mesh_aabb_world(obj):
    min_v = Vector((1e9, 1e9, 1e9))
    max_v = Vector((-1e9, -1e9, -1e9))

    for v in obj.data.vertices:
        wv = obj.matrix_world @ v.co
        min_v = Vector((min(min_v[i], wv[i]) for i in range(3)))
        max_v = Vector((max(max_v[i], wv[i]) for i in range(3)))

    pad = 0.1 * (max_v - min_v)
    return min_v - pad, max_v + pad

# Sample N points stratified in the AABB defined by min_v and max_v
def stratified_volume_points(min_v, max_v, N):
    n = int(math.ceil(N ** (1/3)))
    pts = []
    for i in range(n):
        for j in range(n):
            for k in range(n):
                u = Vector((
                    (i + random.random()) / n,
                    (j + random.random()) / n,
                    (k + random.random()) / n,
                ))
                pts.append(min_v + u * (max_v - min_v))
    return pts[:N]

# Sample points on the surface
# The initialization of the points might be important in some gaussian splatting methods
# in this case we chose to do 30% volume points and 70% surface points
def sample_surface_points(obj, N):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    mesh = obj.evaluated_get(depsgraph).to_mesh()

    tris = []
    areas = []

    for poly in mesh.polygons:
        v0, v1, v2 = [obj.matrix_world @ mesh.vertices[i].co for i in poly.vertices]
        area = ((v1 - v0).cross(v2 - v0)).length * 0.5         # face area
        tris.append((v0, v1, v2))
        areas.append(area)

    total_area = sum(areas)
    cdf = []
    acc = 0.0
    for a in areas:
        acc += a
        cdf.append(acc / total_area)

    pts = []
    for _ in range(N):
        r = random.random()
        tri = tris[next(i for i,v in enumerate(cdf) if v >= r)]
        u, v = random.random(), random.random()
        if u + v > 1.0:
            u = 1.0 - u
            v = 1.0 - v
        p = tri[0] + u * (tri[1] - tri[0]) + v * (tri[2] - tri[0])
        pts.append(p)

    return pts

# Filter points that are visible from at least one camera
# if not visible from any camera, no contribution to reconstruction
def visible_from_any_camera(p, cameras):
    for cam in cameras:
        M = cam.matrix_world.inverted()
        pc = M @ p

        if pc.z >= -0.1:
            continue

        K = cam.data
        fx = (K.lens / K.sensor_width) * img_width
        fy = fx
        cx = img_width * 0.5
        cy = img_height * 0.5

        x = (fx * pc.x / -pc.z) + cx
        y = (fy * pc.y / -pc.z) + cy

        if 0 <= x < img_width and 0 <= y < img_height:
            return True

    return False

# -------------------------------------------------------------
# Look at target
# -------------------------------------------------------------
if "LookAt" not in bpy.data.objects:
    tgt = bpy.data.objects.new("LookAt", None)
    bpy.context.collection.objects.link(tgt)
    tgt.location = (0, 0, 0)
else:
    tgt = bpy.data.objects["LookAt"]

print("Preparing to render", num_cameras, "views")
images_txt = open(os.path.join(sparse_path, "images.txt"), "w")
images_txt.write("# Image list with two lines of data per image:\n")
images_txt.write("# IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n")
images_txt.write("# POINTS2D[] as (X, Y, POINT3D_ID)\n")

createdcams = []
for i in range(num_cameras):

    loc = random_spherical(base_radius=14.0)

    cam_data = bpy.data.cameras.new(name=f"Cam_{i}")
    cam_obj = bpy.data.objects.new(f"Cam_{i}", cam_data)
    bpy.context.collection.objects.link(cam_obj)

    cam_obj.location = loc
    cam_obj.rotation_mode = 'XYZ'

    c = cam_obj.constraints.new("TRACK_TO")
    c.target = tgt
    c.track_axis = 'TRACK_NEGATIVE_Z'
    c.up_axis = 'UP_Y'

    cam_obj.rotation_euler.x += random.uniform(-0.05, 0.05)
    cam_obj.rotation_euler.y += random.uniform(-0.05, 0.05)
    cam_obj.rotation_euler.z += random.uniform(-0.05, 0.05)

    createdcams.append(cam_obj)

    if not RENDER:
        continue

    scene.camera = cam_obj
    img_name = f"render_{i:03d}.png"
    scene.render.filepath = os.path.join(images_path, img_name)
    bpy.ops.render.render(write_still=True)

    # Render normal map
    backup = override_material(normal_mat)
    scene.render.filepath = os.path.join(normals_path, img_name)
    bpy.ops.render.render(write_still=True)
    restore_materials(backup)

    # Write transformation to images.txt
    q, t = blender_cam_to_colmap_pose(cam_obj)

    images_txt.write(
        f"{i+1} {q.w} {q.x} {q.y} {q.z} "
        f"{t.x} {t.y} {t.z} 1 {img_name}\n\n"
    )

    print(f"{i+1}/{num_cameras} done")

images_txt.close()
print("All renders completed.")

# -------------------------------------------------------------
# points3D.txt default init cloud, IMPORTANT 70% surface, 30% volume
# -------------------------------------------------------------

MESH_NAME = "MY_MESH"
TOTAL_POINTS = 20000
SURFACE_RATIO = 0.7

mesh_obj = get_mesh_object(MESH_NAME)
min_v, max_v = mesh_aabb_world(mesh_obj)

Ns = int(TOTAL_POINTS * SURFACE_RATIO)      # number of surface points
Nv = TOTAL_POINTS - Ns                      # number of volume points

print(f"Sampling {Ns} surface points and {Nv} volume points")

surface_pts = sample_surface_points(mesh_obj, Ns)
volume_pts  = stratified_volume_points(min_v, max_v, Nv)

all_pts = surface_pts + volume_pts

print("Filtering points not visible from any camera")
visible_pts = [
    p for p in all_pts
    if visible_from_any_camera(p, createdcams)
]

print(f"Kept {len(visible_pts)} / {len(all_pts)} points after visibility filtering")

with open(os.path.join(sparse_path, "points3D.txt"), "w") as f:
    f.write("# 3D point list with one line of data per point:\n")
    f.write("# POINT3D_ID, X, Y, Z, R, G, B, ERROR, TRACK[]\n")

    for i, p in enumerate(visible_pts):
        f.write(
            f"{i+1} {p.x} {p.y} {p.z} 255 255 255 1.0\n"
        )

# DEBUG: show points as very small spheres in the scene
# for easier cleanup, group them in a collection
if DEBUG_POINTS:
    print("Creating debug point spheres in the scene")
    point_coll = bpy.data.collections.new("COLMAP_Points")
    bpy.context.scene.collection.children.link(point_coll)

    for i, p in enumerate(visible_pts):
        print(f"Creating debug point sphere {i+1}/{len(visible_pts)}")
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.02, location=p)
        sph = bpy.context.active_object
        point_coll.objects.link(sph)
        bpy.context.scene.collection.objects.unlink(sph)

# Cleanup
if scene.camera in createdcams:
    scene.camera = None

for cam in createdcams:
    bpy.data.objects.remove(cam, do_unlink=True)

print("Rendering and COLMAP export completed.")