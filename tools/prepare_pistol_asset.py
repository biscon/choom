#!/usr/bin/env python3
"""Inspect, compare, render, and prepare the canonical FPS pistol GLB.

Run with Blender, not the system Python:

    blender --background --factory-startup --python tools/prepare_pistol_asset.py -- --mode inspect

The comparison renders and machine-readable inspection data are written below
``build/pistol_asset_work``. Source GLBs are treated as read-only.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
WEAPON_DIRECTORY = REPOSITORY_ROOT / "assets" / "models" / "weapons"
ARMS_PATH = WEAPON_DIRECTORY / "pistol_arms.glb"
CANDIDATE_PATHS = tuple(WEAPON_DIRECTORY / f"pistol{index}.glb" for index in range(1, 4))
CANONICAL_PATH = WEAPON_DIRECTORY / "pistol.glb"
SELECTED_PATH = WEAPON_DIRECTORY / "pistol3.glb"
SELECTED_SCALE = 0.085386365
WORK_DIRECTORY = REPOSITORY_ROOT / "build" / "pistol_asset_work"
ANIMATION_NAME = "Pistol Idle"
REPRESENTATIVE_FRAME = 20.0

# Temporary rigid transforms used only for visual hand-fit comparison. The
# source anchors are measured in Blender's imported Z-up scene. They are not
# baked into the canonical pistol because final hand-bone attachment is a
# separate engine task.
HAND_FIT = {
    "pistol1.glb": {
        "scale": 0.010457665,
        "rotation_z_degrees": 180.0,
        "source_anchor": (-0.633087, 5.759228, 4.451049),
        "target_anchor": (-0.030, -0.510, 1.385),
    },
    "pistol2.glb": {
        "scale": 0.006979433,
        "rotation_z_degrees": 0.0,
        "source_anchor": (0.0, -6.638909, 1.774792),
        "target_anchor": (-0.030, -0.510, 1.385),
    },
    "pistol3.glb": {
        "scale": 0.085386365,
        "rotation_z_degrees": 90.0,
        "source_anchor": (-0.010743, 0.001971, 0.029829),
        "target_anchor": (-0.030, -0.525, 1.365),
    },
}


def fail(message: str) -> None:
    raise RuntimeError(f"pistol asset preparation: {message}")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mode",
        choices=("inspect", "compare", "prepare", "verify", "all"),
        default="all",
    )
    parser.add_argument(
        "--work-directory",
        type=Path,
        default=WORK_DIRECTORY,
        help="Temporary JSON and render output directory (repository-relative paths allowed).",
    )
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(arguments)


def require_sources() -> None:
    for path in (*CANDIDATE_PATHS, ARMS_PATH):
        if not path.is_file():
            fail(f"required source GLB is missing: {path.relative_to(REPOSITORY_ROOT)}")
        if path.stat().st_size <= 20:
            fail(f"required source GLB is empty: {path.relative_to(REPOSITORY_ROOT)}")


def source_hashes() -> dict[Path, str]:
    return {
        path: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (*CANDIDATE_PATHS, ARMS_PATH)
    }


def verify_source_hashes(before: dict[Path, str]) -> None:
    after = source_hashes()
    changed = [path for path, digest in before.items() if after.get(path) != digest]
    if changed:
        fail(f"source assets changed unexpectedly: {[path.name for path in changed]}")


def clear_scene() -> None:
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in (
        bpy.data.meshes,
        bpy.data.curves,
        bpy.data.materials,
        bpy.data.images,
        bpy.data.armatures,
        bpy.data.actions,
        bpy.data.cameras,
        bpy.data.lights,
    ):
        for block in list(collection):
            collection.remove(block)


def import_glb(path: Path) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    result = bpy.ops.import_scene.gltf(filepath=str(path))
    if "FINISHED" not in result:
        fail(f"Blender could not import {path.relative_to(REPOSITORY_ROOT)}")
    imported = [obj for obj in bpy.data.objects if obj not in before]
    if not imported or not any(obj.type == "MESH" for obj in imported):
        fail(f"GLB has no importable mesh: {path.relative_to(REPOSITORY_ROOT)}")
    return imported


def load_glb_json(path: Path) -> tuple[dict, bytes]:
    payload = path.read_bytes()
    if len(payload) < 20:
        fail(f"invalid GLB header: {path.relative_to(REPOSITORY_ROOT)}")
    magic, version, declared_size = struct.unpack_from("<4sII", payload, 0)
    if magic != b"glTF" or version != 2 or declared_size != len(payload):
        fail(f"unsupported or malformed GLB: {path.relative_to(REPOSITORY_ROOT)}")
    json_size, json_type = struct.unpack_from("<II", payload, 12)
    if json_type != 0x4E4F534A or 20 + json_size > len(payload):
        fail(f"GLB JSON chunk is malformed: {path.relative_to(REPOSITORY_ROOT)}")
    document = json.loads(payload[20 : 20 + json_size].decode("utf-8"))
    binary_offset = 20 + json_size
    while binary_offset % 4:
        binary_offset += 1
    if binary_offset + 8 > len(payload):
        return document, b""
    binary_size, binary_type = struct.unpack_from("<II", payload, binary_offset)
    if binary_type != 0x004E4942 or binary_offset + 8 + binary_size > len(payload):
        fail(f"GLB binary chunk is malformed: {path.relative_to(REPOSITORY_ROOT)}")
    return document, payload[binary_offset + 8 : binary_offset + 8 + binary_size]


def embedded_image_dimensions(data: bytes, mime_type: str) -> tuple[int, int] | None:
    if mime_type == "image/png" and len(data) >= 24 and data.startswith(b"\x89PNG\r\n\x1a\n"):
        return struct.unpack_from(">II", data, 16)
    if mime_type in ("image/jpeg", "image/jpg") and len(data) >= 4:
        offset = 2
        while offset + 9 < len(data):
            if data[offset] != 0xFF:
                offset += 1
                continue
            marker = data[offset + 1]
            if marker in (0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF):
                height, width = struct.unpack_from(">HH", data, offset + 5)
                return width, height
            if offset + 4 > len(data):
                break
            segment_size = struct.unpack_from(">H", data, offset + 2)[0]
            if segment_size < 2:
                break
            offset += 2 + segment_size
    return None


def glb_texture_information(document: dict, binary: bytes) -> list[dict]:
    information: list[dict] = []
    for image_index, image in enumerate(document.get("images", [])):
        uri = image.get("uri")
        embedded = "bufferView" in image or (isinstance(uri, str) and uri.startswith("data:"))
        dimensions = None
        byte_length = None
        if "bufferView" in image:
            view = document.get("bufferViews", [])[image["bufferView"]]
            start = int(view.get("byteOffset", 0))
            byte_length = int(view["byteLength"])
            image_data = binary[start : start + byte_length]
            dimensions = embedded_image_dimensions(image_data, image.get("mimeType", ""))
        information.append(
            {
                "index": image_index,
                "name": image.get("name", f"image_{image_index}"),
                "mime_type": image.get("mimeType"),
                "embedded": embedded,
                "uri": None if embedded else uri,
                "dimensions": list(dimensions) if dimensions else None,
                "byte_length": byte_length,
            }
        )
    return information


def material_texture_roles(document: dict) -> dict[int, list[str]]:
    roles: dict[int, list[str]] = {}

    def add(texture_info: dict | None, role: str) -> None:
        if not texture_info or "index" not in texture_info:
            return
        texture_index = int(texture_info["index"])
        textures = document.get("textures", [])
        if texture_index >= len(textures):
            return
        source = textures[texture_index].get("source")
        if source is not None:
            roles.setdefault(int(source), []).append(role)

    for material in document.get("materials", []):
        pbr = material.get("pbrMetallicRoughness", {})
        add(pbr.get("baseColorTexture"), "base color")
        add(pbr.get("metallicRoughnessTexture"), "metallic (B) / roughness (G)")
        add(material.get("normalTexture"), "normal")
        add(material.get("occlusionTexture"), "occlusion (R)")
        add(material.get("emissiveTexture"), "emissive")
        extensions = material.get("extensions", {})
        clearcoat = extensions.get("KHR_materials_clearcoat", {})
        add(clearcoat.get("clearcoatTexture"), "clearcoat")
        add(clearcoat.get("clearcoatRoughnessTexture"), "clearcoat roughness")
        add(clearcoat.get("clearcoatNormalTexture"), "clearcoat normal")
    return roles


def vector(values) -> list[float]:
    return [round(float(value), 6) for value in values]


def world_bounds(objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    points = [
        obj.matrix_world @ Vector(corner)
        for obj in objects
        if obj.type == "MESH"
        for corner in obj.bound_box
    ]
    if not points:
        fail("could not calculate bounds because no mesh bounds are available")
    return (
        Vector((min(point.x for point in points), min(point.y for point in points), min(point.z for point in points))),
        Vector((max(point.x for point in points), max(point.y for point in points), max(point.z for point in points))),
    )


def object_bounds(obj: bpy.types.Object) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    return (
        Vector((min(point.x for point in points), min(point.y for point in points), min(point.z for point in points))),
        Vector((max(point.x for point in points), max(point.y for point in points), max(point.z for point in points))),
    )


def inspect_glb(path: Path) -> dict:
    clear_scene()
    imported = import_glb(path)
    bpy.context.view_layer.update()
    document, binary = load_glb_json(path)
    minimum, maximum = world_bounds(imported)
    dimensions = maximum - minimum
    mesh_objects = [obj for obj in imported if obj.type == "MESH"]
    unique_meshes = {obj.data for obj in mesh_objects}
    triangle_count = 0
    for mesh in unique_meshes:
        mesh.calc_loop_triangles()
        triangle_count += len(mesh.loop_triangles)
    image_info = glb_texture_information(document, binary)
    if not document.get("materials"):
        fail(f"GLB has no materials: {path.relative_to(REPOSITORY_ROOT)}")
    if not image_info:
        fail(f"GLB has no textures: {path.relative_to(REPOSITORY_ROOT)}")
    missing_images = [image for image in image_info if not image["embedded"] and not image.get("uri")]
    if missing_images:
        fail(f"GLB has unresolved image references: {path.relative_to(REPOSITORY_ROOT)}")
    roles = material_texture_roles(document)
    for image in image_info:
        image["roles"] = roles.get(image["index"], [])
    primitives = [primitive for mesh in document.get("meshes", []) for primitive in mesh.get("primitives", [])]
    root_objects = [obj for obj in imported if obj.parent is None]
    asset_extras = document.get("asset", {}).get("extras", {})
    scene_index = int(document.get("scene", 0))
    scene_roots = document.get("scenes", [{}])[scene_index].get("nodes", []) if document.get("scenes") else []
    root_extras = {}
    if len(scene_roots) == 1:
        root_extras = document.get("nodes", [])[scene_roots[0]].get("extras", {})
    unsupported_extensions = sorted(
        extension
        for extension in document.get("extensionsUsed", [])
        if extension not in {"KHR_materials_unlit"}
    )
    return {
        "file": path.name,
        "file_size_bytes": path.stat().st_size,
        "dimensions_m": vector(dimensions),
        "bounds_min_m": vector(minimum),
        "bounds_max_m": vector(maximum),
        "bounds_center_m": vector((minimum + maximum) * 0.5),
        "vertex_count": sum(len(mesh.vertices) for mesh in unique_meshes),
        "triangle_count": triangle_count,
        "mesh_count": len(unique_meshes),
        "mesh_object_count": len(mesh_objects),
        "object_count": len(imported),
        "object_types": {object_type: sum(obj.type == object_type for obj in imported) for object_type in sorted({obj.type for obj in imported})},
        "material_count": len(document.get("materials", [])),
        "materials": [material.get("name", f"material_{index}") for index, material in enumerate(document.get("materials", []))],
        "textures": image_info,
        "all_primitives_have_uv0": bool(primitives) and all("TEXCOORD_0" in primitive.get("attributes", {}) for primitive in primitives),
        "all_primitives_have_normals": bool(primitives) and all("NORMAL" in primitive.get("attributes", {}) for primitive in primitives),
        "all_primitives_have_tangents": bool(primitives) and all("TANGENT" in primitive.get("attributes", {}) for primitive in primitives),
        "root_objects": [
            {
                "name": obj.name,
                "type": obj.type,
                "translation": vector(obj.location),
                "rotation_euler_radians": vector(obj.rotation_euler),
                "scale": vector(obj.scale),
            }
            for obj in root_objects
        ],
        "mesh_objects": [
            {
                "name": obj.name,
                "bounds_min_m": vector(object_bounds(obj)[0]),
                "bounds_max_m": vector(object_bounds(obj)[1]),
                "bounds_center_m": vector(sum(object_bounds(obj), Vector()) * 0.5),
            }
            for obj in mesh_objects
        ],
        "cameras": [obj.name for obj in imported if obj.type == "CAMERA"],
        "lights": [obj.name for obj in imported if obj.type == "LIGHT"],
        "armatures": [obj.name for obj in imported if obj.type == "ARMATURE"],
        "animations": [animation.get("name", f"animation_{index}") for index, animation in enumerate(document.get("animations", []))],
        "extensions_used": document.get("extensionsUsed", []),
        "extensions_requiring_engine_review": unsupported_extensions,
        "author": asset_extras.get("author") or root_extras.get("source_author"),
        "license": asset_extras.get("license") or root_extras.get("source_license"),
        "source": asset_extras.get("source") or root_extras.get("source_url"),
        "title": asset_extras.get("title") or root_extras.get("source_title"),
    }


def inspect_arms() -> dict:
    clear_scene()
    bpy.context.scene.render.fps = 30
    imported = import_glb(ARMS_PATH)
    armatures = [obj for obj in imported if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        fail(f"expected one arms armature, found {len(armatures)}")
    armature = armatures[0]
    actions = [action for action in bpy.data.actions if action.name == ANIMATION_NAME]
    if len(actions) != 1:
        fail(f"required arms animation {ANIMATION_NAME!r} was not imported exactly once")
    action = actions[0]
    if armature.animation_data is None:
        fail("arms armature has no animation data")
    # glTF import already associates the action/slot with the armature. Setting
    # scene time is enough to evaluate the representative pose.
    bpy.context.scene.render.fps = 30
    bpy.context.scene.frame_set(int(REPRESENTATIVE_FRAME), subframe=REPRESENTATIVE_FRAME % 1.0)
    bpy.context.view_layer.update()
    right_hand_candidates = [
        bone for bone in armature.pose.bones
        if bone.name.lower().endswith("righthand")
    ]
    if len(right_hand_candidates) != 1:
        fail(f"could not identify one right-hand pose bone; candidates: {[bone.name for bone in right_hand_candidates]}")
    hand = right_hand_candidates[0]
    hand_world = armature.matrix_world @ hand.matrix
    minimum, maximum = world_bounds(imported)
    relevant_bones = {}
    for bone in armature.pose.bones:
        if any(token in bone.name for token in ("RightHand", "LeftHand")):
            bone_world = armature.matrix_world @ bone.matrix
            relevant_bones[bone.name] = {
                "head": vector(bone_world @ Vector((0.0, 0.0, 0.0))),
                "tail": vector(bone_world @ Vector((0.0, bone.length, 0.0))),
            }
    return {
        "file": ARMS_PATH.name,
        "animation": action.name,
        "animation_frame_range": vector(action.frame_range),
        "representative_frame": REPRESENTATIVE_FRAME,
        "armature": armature.name,
        "bone_count": len(armature.data.bones),
        "right_hand_bone": hand.name,
        "right_hand_world_translation": vector(hand_world.translation),
        "right_hand_world_matrix": [vector(row) for row in hand_world],
        "hand_and_finger_bones": relevant_bones,
        "mesh_objects": [
            {
                "name": obj.name,
                "bounds_min_m": vector(object_bounds(obj)[0]),
                "bounds_max_m": vector(object_bounds(obj)[1]),
            }
            for obj in imported if obj.type == "MESH"
        ],
        "dimensions_m": vector(maximum - minimum),
        "bounds_min_m": vector(minimum),
        "bounds_max_m": vector(maximum),
    }


def write_inspection(work_directory: Path) -> dict:
    work_directory.mkdir(parents=True, exist_ok=True)
    result = {
        "arms": inspect_arms(),
        "candidates": [inspect_glb(path) for path in CANDIDATE_PATHS],
    }
    output = work_directory / "inspection.json"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {output.relative_to(REPOSITORY_ROOT)}")
    return result


def find_arms_pose(imported: list[bpy.types.Object]) -> tuple[bpy.types.Object, bpy.types.PoseBone]:
    armatures = [obj for obj in imported if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        fail(f"expected one arms armature, found {len(armatures)}")
    if not any(action.name == ANIMATION_NAME for action in bpy.data.actions):
        fail(f"required arms animation {ANIMATION_NAME!r} is missing")
    armature = armatures[0]
    hands = [bone for bone in armature.pose.bones if bone.name.lower().endswith("righthand")]
    if len(hands) != 1:
        fail(f"could not identify exactly one right-hand bone: {[bone.name for bone in hands]}")
    bpy.context.scene.render.fps = 30
    bpy.context.scene.frame_set(int(REPRESENTATIVE_FRAME), subframe=REPRESENTATIVE_FRAME % 1.0)
    bpy.context.view_layer.update()
    return armature, hands[0]


def transform_imported_group(objects: list[bpy.types.Object], transform: Matrix) -> None:
    roots = [obj for obj in objects if obj.parent is None]
    if not roots:
        fail("imported candidate has no root object")
    for root in roots:
        root.matrix_world = transform @ root.matrix_world
    bpy.context.view_layer.update()


def hand_fit_transform(candidate_name: str) -> Matrix:
    configuration = HAND_FIT[candidate_name]
    source_anchor = Vector(configuration["source_anchor"])
    target_anchor = Vector(configuration["target_anchor"])
    rotation = Matrix.Rotation(math.radians(configuration["rotation_z_degrees"]), 4, "Z")
    scale = Matrix.Scale(configuration["scale"], 4)
    return Matrix.Translation(target_anchor) @ rotation @ scale @ Matrix.Translation(-source_anchor)


def set_camera(camera: bpy.types.Object, location: tuple[float, float, float], target: tuple[float, float, float], fov_y_degrees: float) -> None:
    camera.location = location
    direction = Vector(target) - camera.location
    if direction.length_squared < 1.0e-8:
        fail("camera location and target must differ")
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "PERSP"
    camera.data.angle_y = math.radians(fov_y_degrees)
    camera.data.lens_unit = "FOV"


def add_area_light(name: str, location: tuple[float, float, float], energy: float, size: float, color: tuple[float, float, float], target: tuple[float, float, float]) -> None:
    data = bpy.data.lights.new(name=name, type="AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    data.color = color
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def configure_render_scene() -> bpy.types.Object:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 960
    scene.render.resolution_y = 540
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.image_settings.color_mode = "RGBA"
    scene.world.color = (0.018, 0.022, 0.030)
    scene.view_settings.look = "AgX - Medium High Contrast"
    camera_data = bpy.data.cameras.new("ComparisonCamera")
    camera_data.clip_start = 0.01
    camera_data.clip_end = 20.0
    camera = bpy.data.objects.new("ComparisonCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    focus = (-0.025, -0.50, 1.36)
    add_area_light("Key", (0.55, 0.15, 2.15), 800.0, 1.2, (1.0, 0.82, 0.70), focus)
    add_area_light("Fill", (-0.65, -0.15, 1.75), 500.0, 1.0, (0.62, 0.76, 1.0), focus)
    add_area_light("Rim", (0.0, -1.35, 1.85), 650.0, 0.8, (0.72, 0.84, 1.0), focus)
    return camera


def render_comparisons(work_directory: Path) -> dict:
    render_directory = work_directory / "renders"
    render_directory.mkdir(parents=True, exist_ok=True)
    output: dict[str, dict] = {}
    views = {
        "first_person": {
            "location": (0.0, 0.16, 1.55),
            "target": (-0.02, -0.50, 1.34),
            "fov_y": 55.0,
        },
        "three_quarter": {
            "location": (0.34, -0.20, 1.55),
            "target": (-0.03, -0.51, 1.36),
            "fov_y": 48.0,
        },
        "grip_closeup": {
            "location": (0.20, -0.27, 1.46),
            "target": (-0.03, -0.49, 1.36),
            "fov_y": 32.0,
        },
    }
    for candidate_path in CANDIDATE_PATHS:
        clear_scene()
        bpy.context.scene.render.fps = 30
        arms_objects = import_glb(ARMS_PATH)
        _, hand = find_arms_pose(arms_objects)
        for obj in arms_objects:
            # The source arms GLB contains an unrelated two-metre Icosphere.
            # It is excluded only from temporary comparison renders; the arms
            # source asset itself remains untouched.
            if obj.type == "MESH" and obj.name == "Icosphere":
                obj.hide_render = True
        candidate_objects = import_glb(candidate_path)
        transform = hand_fit_transform(candidate_path.name)
        transform_imported_group(candidate_objects, transform)
        camera = configure_render_scene()
        candidate_minimum, candidate_maximum = world_bounds(candidate_objects)
        candidate_result = {
            "scale": HAND_FIT[candidate_path.name]["scale"],
            "rotation_z_degrees": HAND_FIT[candidate_path.name]["rotation_z_degrees"],
            "source_anchor": list(HAND_FIT[candidate_path.name]["source_anchor"]),
            "target_anchor": list(HAND_FIT[candidate_path.name]["target_anchor"]),
            "hand_fit_dimensions_m": vector(candidate_maximum - candidate_minimum),
            "right_hand_bone": hand.name,
            "renders": {},
        }
        for view_name, view in views.items():
            set_camera(camera, view["location"], view["target"], view["fov_y"])
            output_path = render_directory / f"{candidate_path.stem}_{view_name}.png"
            bpy.context.scene.render.filepath = str(output_path)
            bpy.ops.render.render(write_still=True)
            if not output_path.is_file() or output_path.stat().st_size <= 0:
                fail(f"comparison render was not written: {output_path}")
            candidate_result["renders"][view_name] = str(output_path.relative_to(REPOSITORY_ROOT))
            print(f"Rendered {output_path.relative_to(REPOSITORY_ROOT)}")
        output[candidate_path.name] = candidate_result
    comparison_path = work_directory / "comparison.json"
    comparison_path.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {comparison_path.relative_to(REPOSITORY_ROOT)}")
    return output


def prepare_canonical() -> None:
    if SELECTED_PATH not in CANDIDATE_PATHS:
        fail("selected source must be one of the three immutable pistol candidates")
    if CANONICAL_PATH in (*CANDIDATE_PATHS, ARMS_PATH):
        fail("canonical output path aliases an immutable source path")
    before_hashes = source_hashes()
    clear_scene()
    imported = import_glb(SELECTED_PATH)
    mesh_objects = [obj for obj in imported if obj.type == "MESH"]
    if len(mesh_objects) != 2:
        fail(f"selected pistol structure changed; expected 2 mesh objects, found {len(mesh_objects)}")
    if len({material for obj in mesh_objects for material in obj.data.materials if material is not None}) != 2:
        fail("selected pistol structure changed; expected 2 materials")
    minimum, maximum = world_bounds(mesh_objects)
    center = (minimum + maximum) * 0.5
    canonical_transform = (
        Matrix.Rotation(math.radians(90.0), 4, "Z")
        @ Matrix.Scale(SELECTED_SCALE, 4)
        @ Matrix.Translation(-center)
    )

    # Detach only the legitimate mesh objects while retaining their evaluated
    # world transforms. Empty Sketchfab/FBX conversion wrappers are deliberately
    # omitted from the canonical asset.
    for obj in mesh_objects:
        world = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = canonical_transform @ world
    for obj in list(bpy.data.objects):
        if obj not in mesh_objects:
            bpy.data.objects.remove(obj, do_unlink=True)

    bpy.ops.object.select_all(action="DESELECT")
    for index, obj in enumerate(mesh_objects, start=1):
        obj.name = f"PistolPart{index:02d}"
        obj.data.name = f"PistolPart{index:02d}Mesh"
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objects[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    root = bpy.data.objects.new("Pistol", None)
    bpy.context.scene.collection.objects.link(root)
    root["source_title"] = "9mm Pistol"
    root["source_author"] = "TORI106 (https://sketchfab.com/TORI106)"
    root["source_license"] = "CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)"
    root["source_url"] = "https://sketchfab.com/3d-models/9mm-pistol-43bc09f5aace4346a8a2b6e1580fc03f"
    for obj in mesh_objects:
        obj.parent = root
        obj.matrix_parent_inverse = Matrix.Identity(4)
    bpy.context.view_layer.update()
    final_minimum, final_maximum = world_bounds(mesh_objects)
    final_dimensions = final_maximum - final_minimum
    expected_dimensions = Vector((
        0.333998 * SELECTED_SCALE,
        2.400852 * SELECTED_SCALE,
        1.579198 * SELECTED_SCALE,
    ))
    if any(abs(final_dimensions[index] - expected_dimensions[index]) > 2.0e-5 for index in range(3)):
        fail(f"canonical dimensions are unexpected: {vector(final_dimensions)}")

    bpy.ops.object.select_all(action="DESELECT")
    root.select_set(True)
    for obj in mesh_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = root
    result = bpy.ops.export_scene.gltf(
        filepath=str(CANONICAL_PATH),
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_normals=True,
        export_tangents=True,
        export_materials="EXPORT",
        export_extras=True,
        export_cameras=False,
        export_lights=False,
        export_animations=False,
    )
    if "FINISHED" not in result or not CANONICAL_PATH.is_file() or CANONICAL_PATH.stat().st_size <= 20:
        fail(f"canonical export failed: {CANONICAL_PATH.relative_to(REPOSITORY_ROOT)}")
    verify_source_hashes(before_hashes)
    print(f"Exported {CANONICAL_PATH.relative_to(REPOSITORY_ROOT)}")


def render_verification(work_directory: Path) -> dict:
    render_directory = work_directory / "renders"
    render_directory.mkdir(parents=True, exist_ok=True)
    clear_scene()
    bpy.context.scene.render.fps = 30
    arms_objects = import_glb(ARMS_PATH)
    _, hand = find_arms_pose(arms_objects)
    for obj in arms_objects:
        if obj.type == "MESH" and obj.name == "Icosphere":
            obj.hide_render = True
    pistol_objects = import_glb(CANONICAL_PATH)
    transform_imported_group(pistol_objects, Matrix.Translation(Vector((-0.030, -0.525, 1.365))))
    camera = configure_render_scene()
    views = {
        "first_person": ((0.0, 0.16, 1.55), (-0.02, -0.50, 1.34), 55.0),
        "three_quarter": ((0.34, -0.20, 1.55), (-0.03, -0.51, 1.36), 48.0),
        "grip_closeup": ((0.20, -0.27, 1.46), (-0.03, -0.49, 1.36), 32.0),
    }
    renders = {}
    for view_name, (location, target, fov) in views.items():
        set_camera(camera, location, target, fov)
        output_path = render_directory / f"pistol_verification_{view_name}.png"
        bpy.context.scene.render.filepath = str(output_path)
        bpy.ops.render.render(write_still=True)
        if not output_path.is_file() or output_path.stat().st_size <= 0:
            fail(f"verification render was not written: {output_path}")
        renders[view_name] = str(output_path.relative_to(REPOSITORY_ROOT))
        print(f"Rendered {output_path.relative_to(REPOSITORY_ROOT)}")
    return {"right_hand_bone": hand.name, "renders": renders}


def verify_canonical(work_directory: Path) -> dict:
    if not CANONICAL_PATH.is_file():
        fail(f"canonical GLB is missing: {CANONICAL_PATH.relative_to(REPOSITORY_ROOT)}")
    statistics = inspect_glb(CANONICAL_PATH)
    expected = Vector((0.333998 * SELECTED_SCALE, 2.400852 * SELECTED_SCALE, 1.579198 * SELECTED_SCALE))
    actual = Vector(statistics["dimensions_m"])
    if any(abs(actual[index] - expected[index]) > 2.0e-5 for index in range(3)):
        fail(f"reimport dimensions are incorrect: {statistics['dimensions_m']}")
    if statistics["mesh_count"] != 2 or statistics["material_count"] != 2:
        fail("reimport did not preserve the selected pistol's two mesh/material groups")
    if len(statistics["textures"]) != 6 or not all(image["embedded"] for image in statistics["textures"]):
        fail("reimport did not preserve six embedded textures")
    if not all((statistics["all_primitives_have_uv0"], statistics["all_primitives_have_normals"], statistics["all_primitives_have_tangents"])):
        fail("reimport lost UVs, normals, or tangents")
    if statistics["cameras"] or statistics["lights"] or statistics["armatures"] or statistics["animations"]:
        fail("canonical GLB contains unrelated camera, light, armature, or animation data")
    for root in statistics["root_objects"]:
        if root["translation"] != [0.0, 0.0, 0.0] or root["rotation_euler_radians"] != [0.0, 0.0, 0.0] or root["scale"] != [1.0, 1.0, 1.0]:
            fail(f"canonical root transform is not clean: {root}")
    render_result = render_verification(work_directory)
    result = {
        "selected_source": SELECTED_PATH.name,
        "uniform_scale": SELECTED_SCALE,
        "canonical": statistics,
        "hand_fit_verification": render_result,
    }
    output = work_directory / "verification.json"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {output.relative_to(REPOSITORY_ROOT)}")
    return result


def main() -> None:
    arguments = parse_arguments()
    work_directory = arguments.work_directory
    if not work_directory.is_absolute():
        work_directory = REPOSITORY_ROOT / work_directory
    require_sources()
    if arguments.mode in ("inspect", "all"):
        write_inspection(work_directory)
    if arguments.mode in ("compare", "all"):
        render_comparisons(work_directory)
    if arguments.mode in ("prepare", "all"):
        prepare_canonical()
    if arguments.mode in ("verify", "all"):
        verify_canonical(work_directory)


if __name__ == "__main__":
    main()
