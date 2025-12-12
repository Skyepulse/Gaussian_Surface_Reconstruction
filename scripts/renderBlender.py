import bpy
import random
import math

output_path = "//teapot/input"
num_cameras = 100

scene = bpy.context.scene

# Transparent background preserved
scene.render.film_transparent = True
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.color_depth = '8'
scene.render.image_settings.file_format = 'PNG'

scene.render.engine = 'CYCLES'
scene.cycles.device = 'GPU'

scene.render.resolution_x = 800
scene.render.resolution_y = 800
scene.render.resolution_percentage = 100

# -------------------------------------------------------------
# Improved spherical sampling:
#   - still upper hemisphere only
#   - slight radius and tangent jitter
# -------------------------------------------------------------
def random_spherical(base_radius=20.0):
    phi = random.uniform(0, 2*math.pi)

    # Cameras stay above XY plane, but avoid nearly flat angles
    costheta = random.uniform(-1.0, 1.0)
    theta = math.acos(costheta)

    # small jitter to avoid perfect spherical shell
    r = base_radius + random.uniform(-1.0, 1.0)

    x = r * math.sin(theta) * math.cos(phi)
    y = r * math.sin(theta) * math.sin(phi)
    z = r * math.cos(theta)

    # small sideways jitter (breaks symmetry, improves COLMAP conditioning)
    x += random.uniform(-0.25, 0.25)
    y += random.uniform(-0.25, 0.25)

    return (x, y, z)

# -------------------------------------------------------------
# General look target (unchanged)
# -------------------------------------------------------------
if "LookAt" not in bpy.data.objects:
    tgt = bpy.data.objects.new("LookAt", None)
    bpy.context.collection.objects.link(tgt)
    tgt.location = (0, 0, 0)
else:
    tgt = bpy.data.objects["LookAt"]

createdcams = []
print("Preparing to render", num_cameras, "views")

for i in range(num_cameras):
    loc = random_spherical(base_radius=8.0)

    cam_data = bpy.data.cameras.new(name=f"Cam_{i}")
    cam_obj = bpy.data.objects.new(f"Cam_{i}", cam_data)
    bpy.context.collection.objects.link(cam_obj)

    cam_obj.location = loc
    cam_obj.rotation_mode = 'XYZ'

    # Track constraint, preserved
    c = cam_obj.constraints.new("TRACK_TO")
    c.target = tgt
    c.track_axis = 'TRACK_NEGATIVE_Z'
    c.up_axis = 'UP_Y'

    # ---------------------------------------------------------
    # ADD VERY SMALL RANDOM ROTATION
    # breaks perfect symmetry without losing "facing the teapot"
    # ---------------------------------------------------------
    cam_obj.rotation_euler.x += random.uniform(-0.05, 0.05)
    cam_obj.rotation_euler.y += random.uniform(-0.05, 0.05)
    cam_obj.rotation_euler.z += random.uniform(-0.05, 0.05)

    scene.camera = cam_obj
    scene.render.filepath = f"{output_path}/render_{i:03d}"
    bpy.ops.render.render(write_still=True)

    createdcams.append(cam_obj)
    print(f"{i+1}/{num_cameras} done")

print("All renders completed.")

# Cleanup
if scene.camera in createdcams:
    scene.camera = None

for cam in createdcams:
    bpy.data.objects.remove(cam, do_unlink=True)

print("Temporary cameras removed.")