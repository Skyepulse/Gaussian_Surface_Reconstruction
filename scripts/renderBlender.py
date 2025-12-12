# This is the blender script we wrote to be used inside Blender's scripting environment
# to render images of the mesh inside the Blender scene from multiple viewpoints.
# We convert it to a colmap compatible dataset.

import bpy
import random
import math
import os
from mathutils import Matrix, Quaternion, Vector

# CONFIGURATION
output_path = "//tmp/blender_colmap_output"
sparse_path = os.path.join(output_path, "sparse", "0")
images_path = os.path.join(output_path, "images")

num_cameras = 100
img_width = 800
img_height = 800

scene = bpy.context.scene

# Transparent background preserved, for datasets similar to Truck synthetic blender dataset
scene.render.film_transparent = True
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.color_depth = '8'
scene.render.image_settings.file_format = 'PNG'

scene.render.engine = 'CYCLES'
scene.cycles.device = 'GPU'

# We took same resolution as Truck dataset
scene.render.resolution_x = img_width
scene.render.resolution_y = img_height
scene.render.resolution_percentage = 100

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
BLENDER_TO_COLMAP = Matrix((
    (1,  0,  0),
    (0, -1,  0),
    (0,  0, -1),
))

def blender_cam_to_colmap_pose(cam_obj):
    M_c2w = cam_obj.matrix_world
    R_c2w = M_c2w.to_3x3()
    t_c2w = M_c2w.to_translation()

    R_w2c = R_c2w.transposed()
    t_w2c = -R_w2c @ t_c2w

    R_colmap = BLENDER_TO_COLMAP @ R_w2c
    t_colmap = BLENDER_TO_COLMAP @ t_w2c

    q = R_colmap.to_quaternion()
    return q, t_colmap

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
    loc = random_spherical(base_radius=8.0)

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

    scene.camera = cam_obj
    img_name = f"render_{i:03d}.png"
    scene.render.filepath = os.path.join(images_path, img_name)
    bpy.ops.render.render(write_still=True)

    # Write transformation to images.txt
    q, t = blender_cam_to_colmap_pose(cam_obj)

    images_txt.write(
        f"{i+1} {q.w} {q.x} {q.y} {q.z} "
        f"{t.x} {t.y} {t.z} 1 {img_name}\n\n"
    )

    createdcams.append(cam_obj)
    print(f"{i+1}/{num_cameras} done")

images_txt.close()
print("All renders completed.")

# -------------------------------------------------------------
# points3D.txt (empty, not needed for the gaussian reconstruction)
# -------------------------------------------------------------
with open(os.path.join(sparse_path, "points3D.txt"), "w") as f:
    f.write("# 3D point list with one line of data per point:\n")
    f.write("# POINT3D_ID, X, Y, Z, R, G, B, ERROR, TRACK[]\n")

# Cleanup
if scene.camera in createdcams:
    scene.camera = None

for cam in createdcams:
    bpy.data.objects.remove(cam, do_unlink=True)

print("Rendering and COLMAP export completed.")