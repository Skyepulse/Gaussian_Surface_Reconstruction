import bpy
import random
import math

blend_path = "../render.blend"
output_path = "../renders/"
num_cameras = 5

bpy.context.scene.render.film_transparent = True
bpy.context.scene.render.image_settings.color_mode = 'RGBA'
bpy.context.scene.render.image_settings.color_depth = '8'
bpy.context.scene.render.image_settings.file_format = 'PNG'

scene = bpy.context.scene
scene.render.engine = 'CYCLES'  # change if needed
scene.cycles.device = 'GPU'     # optional, if GPU available

def random_spherical(radius=5.0):
    phi = random.uniform(0, 2 * math.pi)
    costheta = random.uniform(-1, 1)
    theta = math.acos(costheta)
    r = radius

    x = r * math.sin(theta) * math.cos(phi)
    y = r * math.sin(theta) * math.sin(phi)
    z = r * math.cos(theta)
    return (x, y, z)

for i in range(num_cameras):
    loc = random_spherical(radius=5.0)
    
    cam_data = bpy.data.cameras.new(name=f"Cam_{i}")
    cam_obj = bpy.data.objects.new(f"Cam_{i}", cam_data)
    bpy.context.collection.objects.link(cam_obj)

    cam_obj.location = loc
    cam_obj.rotation_mode = 'QUATERNION'
    cam_obj.rotation_quaternion = bpy.context.scene.cursor.rotation_quaternion.copy()
    cam_obj.constraints.new("TRACK_TO")
    cam_obj.constraints["Track To"].target = None  # allow explicit pointing
    cam_obj.constraints["Track To"].track_axis = 'TRACK_NEGATIVE_Z'
    cam_obj.constraints["Track To"].up_axis = 'UP_Y'

    # Explicit aim at origin
    direction = (-cam_obj.location[0], -cam_obj.location[1], -cam_obj.location[2])
    cam_obj.rotation_mode = 'XYZ'
    cam_obj.rotation_euler = math.atan2(direction[1], direction[0]), math.atan2(direction[2], math.hypot(direction[0], direction[1])), 0

    scene.camera = cam_obj
    scene.render.filepath = f"{output_path}/render_{i}.png"
    bpy.ops.render.render(write_still=True)