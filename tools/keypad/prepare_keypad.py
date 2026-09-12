"""Run: blender --background --factory-startup -noaudio --python tools/keypad/prepare_keypad.py

Offline preparation only. Imports the checked-in GLB into a separate Blender
process; does not change the source model or the interactive Blender session.
"""
import json
import math
from pathlib import Path
import struct
import wave

import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

if not bpy.app.background:
    raise RuntimeError("Run this recipe in a separate background Blender process")
bpy.context.preferences.system.audio_device = "None"

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/keypads/simple_keypad"
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(ROOT / "assets/models/simple_keypad.glb"))
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x = 1024
scene.render.resolution_y = 1920
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.view_settings.view_transform = "Standard"
scene.view_settings.look = "None"
scene.view_settings.exposure = 0
scene.world.use_nodes = True
scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.57, 0.6, 1)
scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.35
bpy.ops.object.camera_add(location=(2.0, 0.0, 0.36254))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 0.36254)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 0.80
scene.camera = camera
bpy.ops.object.light_add(type="AREA", location=(1.2, -0.6, 1.3))
lamp = bpy.context.object
lamp.data.energy = 90
lamp.data.shape = "DISK"
lamp.data.size = 1.4
lamp.rotation_euler = (Vector((0, 0, 0.36)) - lamp.location).to_track_quat("-Z", "Y").to_euler()
light = bpy.data.materials["Light"]
bsdf = next(n for n in light.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
bsdf.inputs["Emission Color"].default_value = (0, 0, 0, 1)
bsdf.inputs["Emission Strength"].default_value = 0
# The atlas also paints red around the LED. Neutralise only that narrow band
# for the 2D plate so blue/green sprites do not retain a baked red fringe.
for material in [bpy.data.materials["Base"], light]:
    tree = material.node_tree
    shader = next(n for n in tree.nodes if n.type == "BSDF_PRINCIPLED")
    source = shader.inputs["Base Color"].links[0].from_socket
    geometry = tree.nodes.new("ShaderNodeNewGeometry")
    centre = tree.nodes.new("ShaderNodeVectorMath")
    centre.operation = "SUBTRACT"
    centre.inputs[1].default_value = (0.10, 0.0056, 0.163)
    tree.links.new(geometry.outputs["Position"], centre.inputs[0])
    ellipse = tree.nodes.new("ShaderNodeVectorMath")
    ellipse.operation = "MULTIPLY"
    ellipse.inputs[1].default_value = (0, 18, 50)
    tree.links.new(centre.outputs[0], ellipse.inputs[0])
    distance = tree.nodes.new("ShaderNodeVectorMath")
    distance.operation = "LENGTH"
    tree.links.new(ellipse.outputs[0], distance.inputs[0])
    local_red = tree.nodes.new("ShaderNodeMapRange")
    local_red.inputs["From Min"].default_value = 0.8
    local_red.inputs["From Max"].default_value = 1.3
    local_red.inputs["To Min"].default_value = 1
    local_red.inputs["To Max"].default_value = 0
    tree.links.new(distance.outputs["Value"], local_red.inputs["Value"])
    gray = tree.nodes.new("ShaderNodeRGBToBW")
    tree.links.new(source, gray.inputs[0])
    mix = tree.nodes.new("ShaderNodeMixRGB")
    tree.links.new(local_red.outputs[0], mix.inputs[0])
    tree.links.new(source, mix.inputs[1])
    tree.links.new(gray.outputs[0], mix.inputs[2])
    tree.links.new(mix.outputs[0], shader.inputs["Base Color"])
scene.render.filepath = str(OUT / "panel.png")
bpy.ops.render.render(write_still=True)

def bounds(obj):
    points = [world_to_camera_view(scene, camera, obj.matrix_world @ Vector(p)) for p in obj.bound_box]
    x0, x1 = min(p.x for p in points), max(p.x for p in points)
    y0, y1 = min(p.y for p in points), max(p.y for p in points)
    return [x0, 1-y1, x1-x0, y1-y0]

# Mesh names are only used by this asset-preparation recipe, never by gameplay.
buttons = []
rows = [["002", "003", "004"], ["005", "006", "007"],
        ["008", "009", "010"], ["011", "001", "014"]]
actions = [str(n) for n in range(1, 10)] + ["backspace", "0", "submit"]
for mesh, action in zip(sum(rows, []), actions):
    buttons.append({"action": action, "rect": bounds(bpy.data.objects["AssetMesh_" + mesh])})
indicator = bounds(bpy.data.objects["AssetMesh_013"])
skin = {
    "id": "simple_keypad", "image": "simple_keypad/panel.png",
    "indicatorImage": "simple_keypad/indicator.png",
    "display": [0.375, 0.164, 0.36, 0.047], "indicator": indicator,
    "indicatorColor": [1.0, 0.0249675, 0.0187566],
    "displayColor": [176, 211, 162], "buttons": buttons,
    "sounds": ["simple_keypad/button.wav", "simple_keypad/rejected.wav", "simple_keypad/accepted.wav"]
}
(OUT.parent / "simple_keypad.json").write_text(json.dumps(skin, indent=2) + "\n")

# A neutral emission-only pass provides a precisely aligned tintable light mask.
for obj in scene.objects:
    if obj.type == "MESH" and obj.name != "AssetMesh_013":
        obj.hide_render = True
light.node_tree.nodes.clear()
emission = light.node_tree.nodes.new("ShaderNodeEmission")
output = light.node_tree.nodes.new("ShaderNodeOutputMaterial")
light.node_tree.links.new(emission.outputs[0], output.inputs["Surface"])
scene.render.filepath = str(OUT / "indicator.png")
bpy.ops.render.render(write_still=True)

for name, frequencies, duration in [("button", [900], 0.065), ("rejected", [180, 140], 0.32),
                                     ("accepted", [850, 1250], 0.20)]:
    rate = 22050
    count = int(rate * duration)
    samples = []
    for i in range(count):
        t = i/rate
        frequency = frequencies[min(len(frequencies)-1, int(t/duration*len(frequencies)))]
        envelope = min(1, t/0.006, (duration-t)/0.02)
        samples.append(struct.pack("<h", int(5000*envelope*math.sin(2*math.pi*frequency*t))))
    with wave.open(str(OUT / (name + ".wav")), "wb") as wav:
        wav.setparams((1, 2, rate, count, "NONE", "not compressed"))
        wav.writeframes(b"".join(samples))
