#!/usr/bin/env python3
"""Inspect, prepare, and verify canonical swing-door model assets.

Run this script with Blender, not the system Python:

    blender --background --factory-startup \
      --python tools/prepare_swing_door_assets.py -- --mode all

The three source GLBs below ``assets/models/doors`` are immutable inputs. The
prepared runtime assets are written to ``assets/models/doors/swing`` and the
machine-readable inspection plus contact sheets are written below
``build/swing_door_asset_work`` by default.
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
SOURCE_DIRECTORY = REPOSITORY_ROOT / "assets" / "models" / "doors"
OUTPUT_DIRECTORY = SOURCE_DIRECTORY / "swing"
WORK_DIRECTORY = REPOSITORY_ROOT / "build" / "swing_door_asset_work"
TOOL_VERSION = 1
CANONICAL_TOLERANCE = 3.0e-5
PARTLY_OPEN_DEGREES = 55.0

SOURCE_PACKS = {
    "industrial_metal_doors_pack.glb": {
        "sha256": "bd6cdbc1783313e6199cbdb7ebbff48cb8674f1fdf752a0917933f9c04213adc",
        "material": "Metal_Doors",
        "atlas": "doors_metal_base_color.png",
        "expected_roots": {
            "Metal Door_001", "Metal Door_002", "Metal Door_003",
            "Metal Door_004", "Metal Door_005", "Metal Door_007",
            "Metal Door_007.001", "Metal Doorframe_002",
            "Metal Doorframe_002.001", "Metal Doorframe_003",
            "Metal Doorframe_005",
        },
    },
    "wooden_interior_doors_pack.glb": {
        "sha256": "1d679dfadb8b5b8d7162eeec816496db6ce05ebae78f7d38b9ca10e983767633",
        "material": "Doors",
        "atlas": "doors_wood_base_color.png",
        "expected_roots": {
            "Wooden Door_001", "Wooden Door_002", "Wooden Door_003",
            "Wooden Door_004", "Wooden Door_005", "Wooden Door_006",
            "Wooden Door_007", "Wooden Door_008", "Wooden Door_009",
            "Wooden Doorframe_001", "Wooden Doorframe_002",
            "Wooden Doorframe_003", "Wooden Doorframe_004",
            "Wooden Doorframe_005", "Wooden Doorframe_005.001",
            "Wooden Doorframe_005.002", "Wooden Doorframe_009",
            "Wooden Doorframe_009.001",
        },
    },
    "worn_wooden_doors_pack.glb": {
        "sha256": "0c71d08f201a4121546fa51081b70c150eb7ef448e26e11acf2fe101ff735f59",
        "material": "Doors",
        "atlas": "doors_wood_base_color.png",
        "expected_roots": {
            "Worn Door_001", "Worn Door_002", "Worn Door_004",
            "Worn Door_005", "Worn Door_006", "Worn Doorframe_004",
            "Worn Doorframe_005", "Worn Doorframe_006",
        },
    },
}


def style(
    asset_id: str,
    source_pack: str,
    leaf_node: str,
    leaf_meshes: tuple[str, ...],
    frame_node: str | None = None,
    frame_meshes: tuple[str, ...] = (),
    nested_leaf: bool = False,
) -> dict:
    material = SOURCE_PACKS[source_pack]["material"]
    panel_mesh = f"{leaf_node}_{material.replace('_', ' ')}_0"
    return {
        "id": asset_id,
        "display_name": asset_id.replace("_", " ").title(),
        "source_pack": source_pack,
        "leaf_node": leaf_node,
        "panel_mesh": panel_mesh,
        "leaf_meshes": leaf_meshes,
        "frame_node": frame_node,
        "frame_meshes": frame_meshes,
        "nested_leaf": nested_leaf,
    }


STYLE_SPECS = (
    style("wooden_interior_001", "wooden_interior_doors_pack.glb", "Wooden Door_001",
          ("Wooden Door_001_Doors_0", "Door Handle.001_Doors_0"),
          "Wooden Doorframe_001", ("Wooden Doorframe_001_Doors_0",)),
    style("wooden_interior_002", "wooden_interior_doors_pack.glb", "Wooden Door_002",
          ("Wooden Door_002_Doors_0", "Door Handle.002_Doors_0"),
          "Wooden Doorframe_002", ("Wooden Doorframe_002_Doors_0",), True),
    style("wooden_interior_003", "wooden_interior_doors_pack.glb", "Wooden Door_003",
          ("Wooden Door_003_Doors_0", "Door Handle.006_Doors_0"),
          "Wooden Doorframe_003", ("Wooden Doorframe_003_Doors_0",)),
    style("wooden_interior_004", "wooden_interior_doors_pack.glb", "Wooden Door_004",
          ("Wooden Door_004_Doors_0", "Door Handle.003_Doors_0"),
          "Wooden Doorframe_004", ("Wooden Doorframe_004_Doors_0",)),
    style("wooden_interior_005", "wooden_interior_doors_pack.glb", "Wooden Door_005",
          ("Wooden Door_005_Doors_0", "Door Handle.012_Doors_0"),
          "Wooden Doorframe_005", ("Wooden Doorframe_005_Doors_0",)),
    style("wooden_interior_006", "wooden_interior_doors_pack.glb", "Wooden Door_006",
          ("Wooden Door_006_Doors_0", "Door Handle.011_Doors_0"),
          "Wooden Doorframe_005.001", ("Wooden Doorframe_005.001_Doors_0",)),
    style("wooden_interior_007", "wooden_interior_doors_pack.glb", "Wooden Door_007",
          ("Wooden Door_007_Doors_0", "Door Handle.013_Doors_0"),
          "Wooden Doorframe_009.001", ("Wooden Doorframe_009.001_Doors_0",)),
    style("wooden_interior_008", "wooden_interior_doors_pack.glb", "Wooden Door_008",
          ("Wooden Door_008_Doors_0", "Door Handle.014_Doors_0"),
          "Wooden Doorframe_005.002", ("Wooden Doorframe_005.002_Doors_0",)),
    style("wooden_interior_009", "wooden_interior_doors_pack.glb", "Wooden Door_009",
          ("Wooden Door_009_Doors_0", "Door Handle.010_Doors_0"),
          "Wooden Doorframe_009", ("Wooden Doorframe_009_Doors_0",)),
    style("worn_wooden_001", "worn_wooden_doors_pack.glb", "Worn Door_001",
          ("Worn Door_001_Doors_0",)),
    style("worn_wooden_002", "worn_wooden_doors_pack.glb", "Worn Door_002",
          ("Worn Door_002_Doors_0",)),
    style("worn_wooden_004", "worn_wooden_doors_pack.glb", "Worn Door_004",
          ("Worn Door_004_Doors_0", "Door Handle.009_Doors_0"),
          "Worn Doorframe_004", ("Worn Doorframe_004_Doors_0",)),
    style("worn_wooden_005", "worn_wooden_doors_pack.glb", "Worn Door_005",
          ("Worn Door_005_Doors_0", "Door Handle.008_Doors_0"),
          "Worn Doorframe_005", ("Worn Doorframe_005_Doors_0",)),
    style("worn_wooden_006", "worn_wooden_doors_pack.glb", "Worn Door_006",
          ("Worn Door_006_Doors_0", "Door Handle.005_Doors_0"),
          "Worn Doorframe_006", ("Worn Doorframe_006_Doors_0",)),
    style("industrial_metal_001", "industrial_metal_doors_pack.glb", "Metal Door_001",
          ("Metal Door_001_Metal Doors_0",)),
    style("industrial_metal_002", "industrial_metal_doors_pack.glb", "Metal Door_002",
          ("Metal Door_002_Metal Doors_0",)),
    style("industrial_metal_003", "industrial_metal_doors_pack.glb", "Metal Door_003",
          ("Metal Door_003_Metal Doors_0", "Circle.004_Metal Doors_0"),
          "Metal Doorframe_002", ("Metal Doorframe_002_Metal Doors_0",)),
    style("industrial_metal_004", "industrial_metal_doors_pack.glb", "Metal Door_004",
          ("Metal Door_004_Metal Doors_0", "Door Handle.023_Metal Doors_0"),
          "Metal Doorframe_003", ("Metal Doorframe_003_Metal Doors_0",)),
    style("industrial_metal_005", "industrial_metal_doors_pack.glb", "Metal Door_005",
          ("Metal Door_005_Metal Doors_0", "Circle.003_Metal Doors_0"),
          "Metal Doorframe_002.001", ("Metal Doorframe_002.001_Metal Doors_0",)),
    style("industrial_metal_007", "industrial_metal_doors_pack.glb", "Metal Door_007",
          ("Metal Door_007_Metal Doors_0", "Door Handle.024_Metal Doors_0")),
)


def fail(message: str) -> None:
    raise RuntimeError(f"swing-door asset preparation: {message}")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("inspect", "prepare", "verify", "all"), default="all")
    parser.add_argument(
        "--work-directory",
        type=Path,
        default=WORK_DIRECTORY,
        help="Diagnostic output directory (repository-relative paths are allowed).",
    )
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return parser.parse_args(arguments)


def relative(path: Path) -> str:
    return path.relative_to(REPOSITORY_ROOT).as_posix()


def source_path(source_pack: str) -> Path:
    return SOURCE_DIRECTORY / source_pack


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_recorded_source_hashes() -> dict[str, str]:
    hashes = {}
    for name, source in SOURCE_PACKS.items():
        path = source_path(name)
        if not path.is_file() or path.stat().st_size <= 20:
            fail(f"required immutable source is missing or empty: {relative(path)}")
        actual = sha256(path)
        if actual != source["sha256"]:
            fail(f"immutable source hash mismatch for {name}: expected {source['sha256']}, got {actual}")
        hashes[name] = actual
    return hashes


def verify_source_hashes_unchanged(before: dict[str, str]) -> None:
    after = verify_recorded_source_hashes()
    changed = [name for name in sorted(before) if before[name] != after[name]]
    if changed:
        fail(f"source files changed while the tool was running: {changed}")


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


def import_model(path: Path) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    result = bpy.ops.import_scene.gltf(filepath=str(path))
    if "FINISHED" not in result:
        fail(f"Blender could not import {relative(path)}")
    imported = [obj for obj in bpy.data.objects if obj not in before]
    if not imported or not any(obj.type == "MESH" for obj in imported):
        fail(f"model contains no importable mesh: {relative(path)}")
    bpy.context.view_layer.update()
    return imported


def load_glb(path: Path) -> tuple[dict, bytes]:
    payload = path.read_bytes()
    if len(payload) < 20:
        fail(f"invalid GLB header: {relative(path)}")
    magic, version, declared_size = struct.unpack_from("<4sII", payload, 0)
    if magic != b"glTF" or version != 2 or declared_size != len(payload):
        fail(f"unsupported or malformed GLB: {relative(path)}")
    json_size, json_type = struct.unpack_from("<II", payload, 12)
    if json_type != 0x4E4F534A or 20 + json_size > len(payload):
        fail(f"malformed GLB JSON chunk: {relative(path)}")
    document = json.loads(payload[20:20 + json_size].decode("utf-8"))
    binary_offset = 20 + json_size
    while binary_offset % 4:
        binary_offset += 1
    if binary_offset + 8 > len(payload):
        return document, b""
    binary_size, binary_type = struct.unpack_from("<II", payload, binary_offset)
    if binary_type != 0x004E4942 or binary_offset + 8 + binary_size > len(payload):
        fail(f"malformed GLB binary chunk: {relative(path)}")
    return document, payload[binary_offset + 8:binary_offset + 8 + binary_size]


def png_dimensions(data: bytes) -> tuple[int, int] | None:
    if len(data) >= 24 and data.startswith(b"\x89PNG\r\n\x1a\n"):
        return struct.unpack_from(">II", data, 16)
    return None


def embedded_atlas(document: dict, binary: bytes, source_pack: str) -> bytes:
    images = document.get("images", [])
    if len(images) != 1:
        fail(f"{source_pack} image count drifted; expected 1, found {len(images)}")
    image = images[0]
    if image.get("mimeType") != "image/png" or "bufferView" not in image:
        fail(f"{source_pack} atlas is no longer one embedded PNG")
    views = document.get("bufferViews", [])
    index = image["bufferView"]
    if not isinstance(index, int) or index < 0 or index >= len(views):
        fail(f"{source_pack} atlas buffer view is invalid")
    view = views[index]
    if int(view.get("buffer", 0)) != 0:
        fail(f"{source_pack} atlas is not stored in the GLB binary buffer")
    start = int(view.get("byteOffset", 0))
    size = int(view.get("byteLength", 0))
    data = binary[start:start + size]
    if len(data) != size or png_dimensions(data) != (1024, 256):
        fail(f"{source_pack} atlas is not the expected 1024x256 PNG")
    return data


def load_source_documents() -> dict[str, tuple[dict, bytes, bytes]]:
    loaded = {}
    for name in sorted(SOURCE_PACKS):
        document, binary = load_glb(source_path(name))
        materials = document.get("materials", [])
        expected_material = SOURCE_PACKS[name]["material"]
        if len(materials) != 1 or materials[0].get("name") != expected_material:
            fail(f"{name} materials drifted; expected only {expected_material!r}")
        if materials[0].get("alphaMode", "OPAQUE") != "OPAQUE":
            fail(f"{name} source material is no longer opaque")
        atlas = embedded_atlas(document, binary, name)
        extras = document.get("asset", {}).get("extras", {})
        required = ("title", "author", "license", "source")
        missing = [key for key in required if not isinstance(extras.get(key), str) or not extras[key]]
        if missing:
            fail(f"{name} is missing source attribution fields: {missing}")
        if document.get("skins") or document.get("animations"):
            fail(f"{name} unexpectedly contains skins or animations")
        loaded[name] = (document, binary, atlas)
    wooden = loaded["wooden_interior_doors_pack.glb"][2]
    worn = loaded["worn_wooden_doors_pack.glb"][2]
    if wooden != worn:
        fail("wooden-interior and worn-wooden embedded atlas bytes differ")
    return loaded


def rounded(values) -> list[float]:
    return [round(float(value), 9) for value in values]


def object_by_exact_name(name: str) -> bpy.types.Object:
    matches = [obj for obj in bpy.data.objects if obj.name == name]
    if len(matches) != 1:
        fail(f"expected exactly one imported object named {name!r}, found {len(matches)}")
    return matches[0]


def descendant_meshes(root: bpy.types.Object, excluded_roots=()) -> list[bpy.types.Object]:
    excluded = set(excluded_roots)
    result = []
    pending = list(root.children)
    while pending:
        obj = pending.pop()
        if obj in excluded:
            continue
        if obj.type == "MESH":
            result.append(obj)
        pending.extend(obj.children)
    return sorted(result, key=lambda obj: obj.name)


def is_descendant(obj: bpy.types.Object, ancestor: bpy.types.Object) -> bool:
    current = obj.parent
    while current is not None:
        if current == ancestor:
            return True
        current = current.parent
    return False


def mesh_material_names(meshes: list[bpy.types.Object]) -> set[str]:
    return {
        material.name
        for obj in meshes
        for material in obj.data.materials
        if material is not None
    }


def world_bounds(meshes: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ Vector(corner) for obj in meshes for corner in obj.bound_box]
    if not points:
        fail("could not measure an empty mesh selection")
    return (
        Vector(tuple(min(point[index] for point in points) for index in range(3))),
        Vector(tuple(max(point[index] for point in points) for index in range(3))),
    )


def bounds_record(meshes: list[bpy.types.Object]) -> dict:
    minimum, maximum = world_bounds(meshes)
    return {
        "minimum": rounded(minimum),
        "maximum": rounded(maximum),
        "dimensions": rounded(maximum - minimum),
    }


def validate_pack_roots(imported: list[bpy.types.Object], source_pack: str) -> None:
    prefixes = ("Wooden Door", "Worn Door", "Metal Door")
    roots = {
        obj.name
        for obj in imported
        if obj.type == "EMPTY" and obj.name.startswith(prefixes)
    }
    expected = SOURCE_PACKS[source_pack]["expected_roots"]
    if roots != expected:
        fail(
            f"{source_pack} named door/frame roots drifted; "
            f"missing={sorted(expected - roots)}, unexpected={sorted(roots - expected)}"
        )


def validate_style_scene(spec: dict) -> dict:
    leaf_root = object_by_exact_name(spec["leaf_node"])
    if leaf_root.type != "EMPTY":
        fail(f"{spec['leaf_node']} is no longer an object hierarchy root")
    leaf_meshes = descendant_meshes(leaf_root)
    actual_leaf_names = {obj.name for obj in leaf_meshes}
    expected_leaf_names = set(spec["leaf_meshes"])
    if actual_leaf_names != expected_leaf_names:
        fail(
            f"{spec['id']} leaf mesh hierarchy drifted; "
            f"expected={sorted(expected_leaf_names)}, actual={sorted(actual_leaf_names)}"
        )
    panel = object_by_exact_name(spec["panel_mesh"])
    if panel.type != "MESH" or panel.parent != leaf_root:
        fail(f"{spec['id']} panel mesh is no longer a direct child of its leaf root")
    expected_material = SOURCE_PACKS[spec["source_pack"]]["material"]
    if mesh_material_names(leaf_meshes) != {expected_material}:
        fail(f"{spec['id']} leaf material assignment drifted")

    panel_minimum, panel_maximum = world_bounds([panel])
    panel_dimensions = panel_maximum - panel_minimum
    if not all(math.isfinite(value) and value > 0.0 for value in panel_dimensions):
        fail(f"{spec['id']} panel bounds are invalid")
    if panel_dimensions.z <= panel_dimensions.x or panel_dimensions.x <= panel_dimensions.y:
        fail(f"{spec['id']} panel axes drifted from width=X, depth=Y, up=Z")
    hinge_distance = abs(leaf_root.matrix_world.translation.x - panel_maximum.x)
    free_edge_distance = abs(leaf_root.matrix_world.translation.x - panel_minimum.x)
    if hinge_distance >= free_edge_distance:
        fail(f"{spec['id']} source leaf origin is no longer nearest the panel maximum-X hinge edge")

    frame_meshes = []
    frame_root = None
    if spec["frame_node"]:
        frame_root = object_by_exact_name(spec["frame_node"])
        if frame_root.type != "EMPTY":
            fail(f"{spec['frame_node']} is no longer an object hierarchy root")
        relationship = is_descendant(leaf_root, frame_root)
        if relationship != spec["nested_leaf"]:
            fail(f"{spec['id']} configured leaf/frame descendant relationship drifted")
        frame_meshes = descendant_meshes(frame_root, (leaf_root,))
        actual_frame_names = {obj.name for obj in frame_meshes}
        expected_frame_names = set(spec["frame_meshes"])
        if actual_frame_names != expected_frame_names:
            fail(
                f"{spec['id']} frame mesh hierarchy drifted; "
                f"expected={sorted(expected_frame_names)}, actual={sorted(actual_frame_names)}"
            )
        if mesh_material_names(frame_meshes) != {expected_material}:
            fail(f"{spec['id']} frame material assignment drifted")

    return {
        "leaf_root": leaf_root,
        "leaf_meshes": leaf_meshes,
        "panel": panel,
        "panel_bounds": bounds_record([panel]),
        "full_leaf_bounds": bounds_record(leaf_meshes),
        "frame_root": frame_root,
        "frame_meshes": frame_meshes,
        "frame_bounds": bounds_record(frame_meshes) if frame_meshes else None,
    }


def node_hierarchy(document: dict) -> list[dict]:
    nodes = document.get("nodes", [])
    parents = {}
    for parent_index, node in enumerate(nodes):
        for child in node.get("children", []):
            parents[child] = parent_index
    return [
        {
            "index": index,
            "name": node.get("name", f"node_{index}"),
            "parent": nodes[parents[index]].get("name", f"node_{parents[index]}") if index in parents else None,
            "children": [nodes[child].get("name", f"node_{child}") for child in node.get("children", [])],
            "mesh": node.get("mesh"),
            "camera": node.get("camera"),
        }
        for index, node in enumerate(nodes)
    ]


def inspect_assets(work_directory: Path, documents: dict[str, tuple[dict, bytes, bytes]]) -> dict:
    work_directory.mkdir(parents=True, exist_ok=True)
    result = {
        "toolVersion": TOOL_VERSION,
        "sources": [],
        "assets": [],
    }
    for source_pack in sorted(SOURCE_PACKS):
        clear_scene()
        imported = import_model(source_path(source_pack))
        validate_pack_roots(imported, source_pack)
        document, _, atlas = documents[source_pack]
        extras = document["asset"]["extras"]
        result["sources"].append({
            "file": source_pack,
            "sha256": SOURCE_PACKS[source_pack]["sha256"],
            "assetMetadata": {key: extras[key] for key in ("title", "author", "license", "source")},
            "materials": document.get("materials", []),
            "images": [{
                "name": image.get("name"),
                "mimeType": image.get("mimeType"),
                "embedded": "bufferView" in image,
                "dimensions": list(png_dimensions(atlas) or ()),
                "byteLength": len(atlas),
                "sha256": hashlib.sha256(atlas).hexdigest(),
            } for image in document.get("images", [])],
            "hierarchy": node_hierarchy(document),
            "importedObjectCounts": {
                object_type: sum(obj.type == object_type for obj in imported)
                for object_type in sorted({obj.type for obj in imported})
            },
        })
        for spec in sorted((item for item in STYLE_SPECS if item["source_pack"] == source_pack), key=lambda item: item["id"]):
            validated = validate_style_scene(spec)
            result["assets"].append({
                "id": spec["id"],
                "sourcePack": source_pack,
                "leafNode": spec["leaf_node"],
                "leafMeshes": [obj.name for obj in validated["leaf_meshes"]],
                "panelMesh": validated["panel"].name,
                "panelBounds": validated["panel_bounds"],
                "fullLeafBounds": validated["full_leaf_bounds"],
                "frameNode": spec["frame_node"],
                "frameMeshes": [obj.name for obj in validated["frame_meshes"]],
                "frameBounds": validated["frame_bounds"],
                "leafNestedBelowFrame": spec["nested_leaf"],
            })
    result["assets"].sort(key=lambda item: item["id"])
    output = work_directory / "inspection.json"
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote {relative(output)}")
    return result


def write_shared_atlases(documents: dict[str, tuple[dict, bytes, bytes]]) -> None:
    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    wooden = documents["wooden_interior_doors_pack.glb"][2]
    worn = documents["worn_wooden_doors_pack.glb"][2]
    if wooden != worn:
        fail("wooden-interior and worn-wooden atlas bytes differ")
    atlases = {
        "doors_wood_base_color.png": wooden,
        "doors_metal_base_color.png": documents["industrial_metal_doors_pack.glb"][2],
    }
    for name, data in atlases.items():
        path = OUTPUT_DIRECTORY / name
        path.write_bytes(data)
        if path.read_bytes() != data:
            fail(f"shared atlas write verification failed: {relative(path)}")


def canonical_leaf_matrix(panel_minimum: Vector, panel_maximum: Vector) -> Matrix:
    hinge_x = panel_maximum.x
    depth_center = (panel_minimum.y + panel_maximum.y) * 0.5
    return Matrix((
        (-1.0, 0.0, 0.0, hinge_x),
        (0.0, -1.0, 0.0, depth_center),
        (0.0, 0.0, 1.0, -panel_minimum.z),
        (0.0, 0.0, 0.0, 1.0),
    ))


def canonical_frame_matrix(frame_minimum: Vector, frame_maximum: Vector) -> Matrix:
    width_center = (frame_minimum.x + frame_maximum.x) * 0.5
    depth_center = (frame_minimum.y + frame_maximum.y) * 0.5
    return Matrix((
        (-1.0, 0.0, 0.0, width_center),
        (0.0, -1.0, 0.0, depth_center),
        (0.0, 0.0, 1.0, -frame_minimum.z),
        (0.0, 0.0, 0.0, 1.0),
    ))


def prepare_selected_meshes(
    meshes: list[bpy.types.Object],
    transform: Matrix,
    root_name: str,
    part_names: list[str],
    extras: dict[str, str | int],
) -> bpy.types.Object:
    if len(meshes) != len(part_names):
        fail(f"internal part-name count mismatch for {root_name}")
    for obj in meshes:
        world = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = transform @ world
    selected = set(meshes)
    for obj in list(bpy.data.objects):
        if obj not in selected:
            bpy.data.objects.remove(obj, do_unlink=True)

    bpy.ops.object.select_all(action="DESELECT")
    for obj, name in zip(meshes, part_names):
        obj.name = name
        obj.data.name = f"{name}Mesh"
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    result = bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if "FINISHED" not in result:
        fail(f"could not bake evaluated transforms for {root_name}")

    root = bpy.data.objects.new(root_name, None)
    bpy.context.scene.collection.objects.link(root)
    for key, value in extras.items():
        root[key] = value
    for obj in meshes:
        obj.parent = root
        obj.matrix_parent_inverse = Matrix.Identity(4)
    bpy.context.view_layer.update()
    return root


def export_separate_gltf(root: bpy.types.Object, meshes: list[bpy.types.Object], output_path: Path, atlas_name: str) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    root.select_set(True)
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = root
    result = bpy.ops.export_scene.gltf(
        filepath=str(output_path),
        export_format="GLTF_SEPARATE",
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
    if "FINISHED" not in result or not output_path.is_file():
        fail(f"Blender glTF export failed: {relative(output_path)}")

    document = json.loads(output_path.read_text(encoding="utf-8"))
    images = document.get("images", [])
    if len(images) != 1:
        fail(f"generated model did not export exactly one image: {relative(output_path)}")
    previous_uri = images[0].get("uri")
    images[0].pop("bufferView", None)
    images[0].pop("mimeType", None)
    images[0]["uri"] = atlas_name
    for material in document.get("materials", []):
        material.pop("alphaCutoff", None)
        material.pop("alphaMode", None)
    buffers = document.get("buffers", [])
    expected_bin = f"{output_path.stem}.bin"
    if len(buffers) != 1:
        fail(f"generated model did not export one binary buffer: {relative(output_path)}")
    buffers[0]["uri"] = expected_bin
    output_path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if isinstance(previous_uri, str) and previous_uri != atlas_name and not previous_uri.startswith("data:"):
        generated_image = (output_path.parent / previous_uri).resolve()
        if generated_image.parent != OUTPUT_DIRECTORY.resolve():
            fail(f"exporter generated an image outside the prepared output directory: {generated_image}")
        if generated_image.is_file():
            generated_image.unlink()
    binary_path = output_path.with_suffix(".bin")
    if not binary_path.is_file() or binary_path.stat().st_size <= 0:
        fail(f"generated binary buffer is missing or empty: {relative(binary_path)}")


def source_root_extras(spec: dict, metadata: dict, part: str) -> dict[str, str | int]:
    return {
        "catalog_id": spec["id"],
        "asset_part": part,
        "source_title": metadata["title"],
        "source_author": metadata["author"],
        "source_license": metadata["license"],
        "source_url": metadata["source"],
        "source_pack": spec["source_pack"],
        "source_pack_sha256": SOURCE_PACKS[spec["source_pack"]]["sha256"],
        "preparation_tool_version": TOOL_VERSION,
    }


def prepare_style(spec: dict, metadata: dict) -> dict:
    clear_scene()
    imported = import_model(source_path(spec["source_pack"]))
    validate_pack_roots(imported, spec["source_pack"])
    validated = validate_style_scene(spec)
    panel_minimum, panel_maximum = world_bounds([validated["panel"]])
    panel_dimensions = panel_maximum - panel_minimum

    leaf_meshes = [object_by_exact_name(name) for name in spec["leaf_meshes"]]
    leaf_part_names = ["LeafPanel"] + [f"LeafAttachment{index:02d}" for index in range(1, len(leaf_meshes))]
    leaf_root = prepare_selected_meshes(
        leaf_meshes,
        canonical_leaf_matrix(panel_minimum, panel_maximum),
        f"DoorLeaf_{spec['id']}",
        leaf_part_names,
        source_root_extras(spec, metadata, "leaf"),
    )
    canonical_panel = object_by_exact_name("LeafPanel")
    canonical_panel_minimum, canonical_panel_maximum = world_bounds([canonical_panel])
    expected_panel_minimum = Vector((0.0, -panel_dimensions.y * 0.5, 0.0))
    expected_panel_maximum = Vector((panel_dimensions.x, panel_dimensions.y * 0.5, panel_dimensions.z))
    if (canonical_panel_minimum - expected_panel_minimum).length > CANONICAL_TOLERANCE or (canonical_panel_maximum - expected_panel_maximum).length > CANONICAL_TOLERANCE:
        fail(f"{spec['id']} leaf canonicalization produced unexpected panel bounds")
    leaf_path = OUTPUT_DIRECTORY / f"{spec['id']}_leaf.gltf"
    export_separate_gltf(leaf_root, leaf_meshes, leaf_path, SOURCE_PACKS[spec["source_pack"]]["atlas"])

    catalog = {
        "id": spec["id"],
        "displayName": spec["display_name"],
        "leafModelPath": relative(leaf_path),
        "nominalWidth": float(panel_dimensions.x),
        "nominalHeight": float(panel_dimensions.z),
        "nominalThickness": float(panel_dimensions.y),
        "sourcePack": spec["source_pack"],
    }

    if spec["frame_node"]:
        clear_scene()
        imported = import_model(source_path(spec["source_pack"]))
        validate_pack_roots(imported, spec["source_pack"])
        validated = validate_style_scene(spec)
        frame_minimum, frame_maximum = world_bounds(validated["frame_meshes"])
        frame_dimensions = frame_maximum - frame_minimum
        frame_meshes = [object_by_exact_name(name) for name in spec["frame_meshes"]]
        frame_part_names = ["FramePanel"] + [f"FramePart{index:02d}" for index in range(1, len(frame_meshes))]
        frame_root = prepare_selected_meshes(
            frame_meshes,
            canonical_frame_matrix(frame_minimum, frame_maximum),
            f"DoorFrame_{spec['id']}",
            frame_part_names,
            source_root_extras(spec, metadata, "frame"),
        )
        canonical_minimum, canonical_maximum = world_bounds(frame_meshes)
        expected_minimum = Vector((-frame_dimensions.x * 0.5, -frame_dimensions.y * 0.5, 0.0))
        expected_maximum = Vector((frame_dimensions.x * 0.5, frame_dimensions.y * 0.5, frame_dimensions.z))
        if (canonical_minimum - expected_minimum).length > CANONICAL_TOLERANCE or (canonical_maximum - expected_maximum).length > CANONICAL_TOLERANCE:
            fail(f"{spec['id']} frame canonicalization produced unexpected bounds")
        frame_path = OUTPUT_DIRECTORY / f"{spec['id']}_frame.gltf"
        export_separate_gltf(frame_root, frame_meshes, frame_path, SOURCE_PACKS[spec["source_pack"]]["atlas"])
        catalog["frameModelPath"] = relative(frame_path)
        catalog["frameOuterWidth"] = float(frame_dimensions.x)
        catalog["frameOuterHeight"] = float(frame_dimensions.z)
    return catalog


def write_attribution(catalog_assets: list[dict], documents: dict[str, tuple[dict, bytes, bytes]]) -> None:
    lines = [
        "# Swing Door Asset Attribution",
        "",
        "These prepared assets were generated from the immutable source packs listed below.",
        "The preparation separates individual leaves and frames, bakes their source hierarchy",
        "transforms, and reuses shared external texture atlases. No source pack GLB is modified.",
        "",
    ]
    for source_pack in sorted(SOURCE_PACKS):
        metadata = documents[source_pack][0]["asset"]["extras"]
        ids = [asset["id"] for asset in catalog_assets if asset["sourcePack"] == source_pack]
        lines.extend([
            f"## {metadata['title']}",
            "",
            f"- Source file: `{source_pack}`",
            f"- Prepared catalog IDs: {', '.join(f'`{asset_id}`' for asset_id in ids)}",
            f"- Author: {metadata['author']}",
            f"- License: {metadata['license']}",
            f"- Source: {metadata['source']}",
            f"- Source SHA-256: `{SOURCE_PACKS[source_pack]['sha256']}`",
            "",
        ])
    (OUTPUT_DIRECTORY / "ATTRIBUTION.md").write_text("\n".join(lines), encoding="utf-8")


def prepare_assets(documents: dict[str, tuple[dict, bytes, bytes]]) -> dict:
    write_shared_atlases(documents)
    assets = []
    for spec in sorted(STYLE_SPECS, key=lambda item: item["id"]):
        metadata = documents[spec["source_pack"]][0]["asset"]["extras"]
        assets.append(prepare_style(spec, metadata))
        print(f"Prepared {spec['id']}")
    catalog = {"formatVersion": 1, "assets": assets}
    catalog_path = OUTPUT_DIRECTORY / "catalog.json"
    catalog_path.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    write_attribution(assets, documents)
    print(f"Wrote {relative(catalog_path)} and {relative(OUTPUT_DIRECTORY / 'ATTRIBUTION.md')}")
    return catalog


def load_and_validate_catalog() -> dict:
    path = OUTPUT_DIRECTORY / "catalog.json"
    if not path.is_file():
        fail(f"generated catalog is missing: {relative(path)}")
    catalog = json.loads(path.read_text(encoding="utf-8"))
    if catalog.get("formatVersion") != 1 or set(catalog) != {"formatVersion", "assets"}:
        fail("generated catalog top-level shape or format version is invalid")
    assets = catalog.get("assets")
    if not isinstance(assets, list) or len(assets) != len(STYLE_SPECS):
        fail(f"generated catalog must contain exactly {len(STYLE_SPECS)} assets")
    ids = [asset.get("id") for asset in assets if isinstance(asset, dict)]
    expected_ids = sorted(spec["id"] for spec in STYLE_SPECS)
    if ids != expected_ids or len(set(ids)) != len(ids):
        fail("generated catalog IDs are missing, duplicated, or not sorted")
    required = {
        "id", "displayName", "leafModelPath", "nominalWidth",
        "nominalHeight", "nominalThickness", "sourcePack",
    }
    for asset in assets:
        if not required.issubset(asset):
            fail(f"catalog asset {asset.get('id')} is missing required fields")
        for field in ("nominalWidth", "nominalHeight", "nominalThickness"):
            value = asset[field]
            if not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0.0:
                fail(f"catalog asset {asset['id']} has invalid {field}")
        frame_fields = ("frameModelPath", "frameOuterWidth", "frameOuterHeight")
        if any(field in asset for field in frame_fields) != all(field in asset for field in frame_fields):
            fail(f"catalog asset {asset['id']} has partial frame metadata")
        for field in ("frameOuterWidth", "frameOuterHeight"):
            if field in asset and (not math.isfinite(asset[field]) or asset[field] <= 0.0):
                fail(f"catalog asset {asset['id']} has invalid {field}")
        for field in ("leafModelPath", "frameModelPath"):
            if field not in asset:
                continue
            model_path = Path(asset[field])
            if model_path.is_absolute() or ".." in model_path.parts or not asset[field].startswith("assets/models/doors/swing/"):
                fail(f"catalog asset {asset['id']} has escaping {field}")
            if model_path.suffix != ".gltf":
                fail(f"catalog asset {asset['id']} {field} is not separated glTF")
    return catalog


def matrix_is_identity(matrix: Matrix, tolerance: float = CANONICAL_TOLERANCE) -> bool:
    identity = Matrix.Identity(4)
    return all(abs(matrix[row][column] - identity[row][column]) <= tolerance for row in range(4) for column in range(4))


def mesh_vertex_centroid(obj: bpy.types.Object) -> Vector:
    if not obj.data.vertices:
        fail(f"generated mesh {obj.name} has no vertices")
    local = sum((vertex.co for vertex in obj.data.vertices), Vector()) / len(obj.data.vertices)
    return obj.matrix_world @ local


def verify_raw_gltf(spec: dict, asset: dict, part: str, model_path: Path, source_document: dict) -> dict:
    document = json.loads(model_path.read_text(encoding="utf-8"))
    expected_meshes = len(spec[f"{part}_meshes"])
    if len(document.get("meshes", [])) != expected_meshes:
        fail(f"{spec['id']} {part} mesh count disagrees with the configured source isolation")
    materials = document.get("materials", [])
    expected_material = source_document["materials"][0]
    if len(materials) != 1 or materials[0].get("alphaMode", "OPAQUE") != "OPAQUE":
        fail(f"{spec['id']} {part} must contain one opaque material")
    if materials[0].get("name") != expected_material.get("name"):
        fail(f"{spec['id']} {part} material identity drifted during preparation")
    pbr = materials[0].get("pbrMetallicRoughness", {})
    expected_pbr = expected_material.get("pbrMetallicRoughness", {})
    if "baseColorTexture" not in pbr:
        fail(f"{spec['id']} {part} lost its base-color atlas assignment")
    scalar_factors = (("metallicFactor", 1.0), ("roughnessFactor", 1.0))
    if any(abs(float(pbr.get(name, default)) - float(expected_pbr.get(name, default))) > 1.0e-6 for name, default in scalar_factors):
        fail(f"{spec['id']} {part} metallic/roughness factors drifted during preparation")
    actual_base_color = pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0])
    expected_base_color = expected_pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0])
    if len(actual_base_color) != 4 or any(abs(float(actual_base_color[index]) - float(expected_base_color[index])) > 1.0e-6 for index in range(4)):
        fail(f"{spec['id']} {part} base-color factor drifted during preparation")
    if bool(materials[0].get("doubleSided", False)) != bool(expected_material.get("doubleSided", False)):
        fail(f"{spec['id']} {part} double-sided material policy drifted during preparation")
    primitives = [primitive for mesh in document.get("meshes", []) for primitive in mesh.get("primitives", [])]
    for primitive in primitives:
        attributes = primitive.get("attributes", {})
        if not all(name in attributes for name in ("POSITION", "NORMAL", "TANGENT", "TEXCOORD_0")):
            fail(f"{spec['id']} {part} lost UV0, normals, or tangents")
        if primitive.get("material") != 0:
            fail(f"{spec['id']} {part} has an unexpected material assignment")
    images = document.get("images", [])
    expected_atlas = SOURCE_PACKS[spec["source_pack"]]["atlas"]
    if len(images) != 1 or images[0].get("uri") != expected_atlas or "bufferView" in images[0]:
        fail(f"{spec['id']} {part} does not reference the shared external atlas")
    buffers = document.get("buffers", [])
    if len(buffers) != 1 or buffers[0].get("uri") != f"{model_path.stem}.bin":
        fail(f"{spec['id']} {part} binary buffer path is unstable")
    forbidden = ("animations", "cameras", "skins")
    if any(document.get(field) for field in forbidden) or document.get("extensionsRequired"):
        fail(f"{spec['id']} {part} contains unrelated scene or unsupported extension data")
    nodes = document.get("nodes", [])
    scenes = document.get("scenes", [])
    if len(scenes) != 1 or len(scenes[0].get("nodes", [])) != 1:
        fail(f"{spec['id']} {part} does not have one canonical root")
    root_node = nodes[scenes[0]["nodes"][0]]
    extras = root_node.get("extras", {})
    source_metadata = source_document["asset"]["extras"]
    expected_extras = {
        "catalog_id": spec["id"],
        "asset_part": part,
        "source_title": source_metadata["title"],
        "source_author": source_metadata["author"],
        "source_license": source_metadata["license"],
        "source_url": source_metadata["source"],
        "source_pack": spec["source_pack"],
        "source_pack_sha256": SOURCE_PACKS[spec["source_pack"]]["sha256"],
        "preparation_tool_version": TOOL_VERSION,
    }
    if any(extras.get(key) != value for key, value in expected_extras.items()):
        fail(f"{spec['id']} {part} root attribution/provenance extras are incomplete")
    if any(key in root_node for key in ("translation", "rotation", "scale", "matrix")):
        fail(f"{spec['id']} {part} serialized root transform is not identity")
    atlas_path = OUTPUT_DIRECTORY / expected_atlas
    if not atlas_path.is_file() or png_dimensions(atlas_path.read_bytes()) != (1024, 256):
        fail(f"{spec['id']} {part} shared atlas is missing or invalid")
    return {
        "meshCount": expected_meshes,
        "materialCount": len(materials),
        "primitiveCount": len(primitives),
        "atlas": expected_atlas,
    }


def verify_imported_model(spec: dict, asset: dict, part: str, model_path: Path) -> dict:
    clear_scene()
    imported = import_model(model_path)
    roots = [obj for obj in imported if obj.parent is None]
    meshes = [obj for obj in imported if obj.type == "MESH"]
    expected_meshes = len(spec[f"{part}_meshes"])
    if len(roots) != 1 or roots[0].type != "EMPTY" or len(meshes) != expected_meshes:
        fail(f"{spec['id']} {part} reimport contains unrelated objects")
    root = roots[0]
    if not matrix_is_identity(root.matrix_world):
        fail(f"{spec['id']} {part} root transform is not clean after reimport")
    if any(obj.type not in {"EMPTY", "MESH"} for obj in imported):
        fail(f"{spec['id']} {part} reimport contains camera, light, armature, or other object data")
    if any(not is_descendant(obj, root) for obj in meshes):
        fail(f"{spec['id']} {part} reimport mesh escaped the canonical root")

    minimum, maximum = world_bounds(meshes)
    dimensions = maximum - minimum
    if not all(math.isfinite(value) and value > 0.0 for value in dimensions):
        fail(f"{spec['id']} {part} reimport bounds are invalid")
    if part == "leaf":
        panels = [obj for obj in meshes if obj.name == "LeafPanel"]
        if len(panels) != 1:
            fail(f"{spec['id']} leaf reimport lost its panel identity")
        panel_minimum, panel_maximum = world_bounds(panels)
        expected_minimum = Vector((0.0, -asset["nominalThickness"] * 0.5, 0.0))
        expected_maximum = Vector((asset["nominalWidth"], asset["nominalThickness"] * 0.5, asset["nominalHeight"]))
        if (panel_minimum - expected_minimum).length > CANONICAL_TOLERANCE or (panel_maximum - expected_maximum).length > CANONICAL_TOLERANCE:
            fail(f"{spec['id']} leaf panel bounds disagree with the catalog")
        if minimum.x < -CANONICAL_TOLERANCE or minimum.z < -CANONICAL_TOLERANCE:
            fail(f"{spec['id']} leaf attachment crosses the canonical hinge or bottom")
        attachment_checks = 0
        rotation = Matrix.Rotation(math.radians(PARTLY_OPEN_DEGREES), 4, "Z")
        before = {obj.name: mesh_vertex_centroid(obj) for obj in meshes}
        root.matrix_world = rotation
        bpy.context.view_layer.update()
        for obj in meshes:
            expected = rotation @ before[obj.name]
            if (mesh_vertex_centroid(obj) - expected).length > CANONICAL_TOLERANCE:
                fail(f"{spec['id']} leaf part {obj.name} detached during root rotation")
            attachment_checks += int(obj.name != "LeafPanel")
        root.matrix_world = Matrix.Identity(4)
        bpy.context.view_layer.update()
    else:
        expected_minimum = Vector((-asset["frameOuterWidth"] * 0.5, minimum.y, 0.0))
        expected_maximum = Vector((asset["frameOuterWidth"] * 0.5, maximum.y, asset["frameOuterHeight"]))
        if abs(minimum.x - expected_minimum.x) > CANONICAL_TOLERANCE or abs(minimum.z) > CANONICAL_TOLERANCE or abs(maximum.x - expected_maximum.x) > CANONICAL_TOLERANCE or abs(maximum.z - expected_maximum.z) > CANONICAL_TOLERANCE:
            fail(f"{spec['id']} frame bounds disagree with the catalog")
        if abs(minimum.y + maximum.y) > CANONICAL_TOLERANCE:
            fail(f"{spec['id']} frame depth is not centered around its origin")
        attachment_checks = 0
    return {
        "boundsMinimum": rounded(minimum),
        "boundsMaximum": rounded(maximum),
        "dimensions": rounded(dimensions),
        "attachmentRotationChecks": attachment_checks,
    }


def add_area_light(name: str, location: tuple[float, float, float], energy: float, size: float) -> None:
    data = bpy.data.lights.new(name=name, type="AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (Vector((8.0, 0.0, 4.0)) - obj.location).to_track_quat("-Z", "Y").to_euler()


def configure_render(width: int, height: int) -> bpy.types.Object:
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    if background is not None:
        background.inputs["Color"].default_value = (0.018, 0.022, 0.030, 1.0)
        background.inputs["Strength"].default_value = 0.45
    camera_data = bpy.data.cameras.new("ContactSheetCamera")
    camera_data.type = "ORTHO"
    camera_data.clip_start = 0.01
    camera_data.clip_end = 100.0
    camera = bpy.data.objects.new("ContactSheetCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    add_area_light("ContactKey", (-2.0, -8.0, 14.0), 1800.0, 8.0)
    add_area_light("ContactFill", (20.0, -2.0, 10.0), 1200.0, 10.0)
    return camera


def add_label(text: str, position: Vector, front: bool) -> None:
    data = bpy.data.curves.new(type="FONT", name=f"Label_{text}")
    data.body = text
    data.align_x = "CENTER"
    data.align_y = "CENTER"
    data.size = 0.18
    data.extrude = 0.002
    obj = bpy.data.objects.new(f"Label_{text}", data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = position
    if front:
        obj.rotation_euler.x = math.radians(90.0)


def add_hinge_marker(position: Vector) -> None:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, radius=0.045, location=position)
    marker = bpy.context.object
    marker.name = "HingeMarker"
    material = bpy.data.materials.get("HingeMarkerMaterial")
    if material is None:
        material = bpy.data.materials.new("HingeMarkerMaterial")
        material.diffuse_color = (1.0, 0.08, 0.03, 1.0)
    marker.data.materials.append(material)


def import_and_transform(path: Path, transform: Matrix) -> None:
    imported = import_model(path)
    roots = [obj for obj in imported if obj.parent is None]
    if len(roots) != 1:
        fail(f"contact-sheet import does not have one root: {relative(path)}")
    roots[0].matrix_world = transform


def place_contact_variant(asset: dict, hinge: Vector, angle_degrees: float) -> None:
    leaf_path = REPOSITORY_ROOT / asset["leafModelPath"]
    leaf_transform = Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(angle_degrees), 4, "Z")
    import_and_transform(leaf_path, leaf_transform)
    if "frameModelPath" in asset:
        frame_origin = hinge + Vector((asset["nominalWidth"] * 0.5, 0.0, 0.0))
        import_and_transform(REPOSITORY_ROOT / asset["frameModelPath"], Matrix.Translation(frame_origin))
    add_hinge_marker(hinge)


def render_contact_sheet(catalog: dict, work_directory: Path, view: str) -> Path:
    clear_scene()
    width, height = (2200, 1450)
    camera = configure_render(width, height)
    columns = 5
    rows = 4
    cell_width = 4.0
    cell_height = 2.65 if view == "front" else 2.1
    assets = catalog["assets"]
    for index, asset in enumerate(assets):
        column = index % columns
        row = index // columns
        if view == "front":
            center = Vector((column * cell_width, 0.0, (rows - 1 - row) * cell_height))
            closed_hinge = center + Vector((-1.15, 0.0, 0.0))
            open_hinge = center + Vector((0.75, 0.0, 0.0))
            place_contact_variant(asset, closed_hinge, 0.0)
            place_contact_variant(asset, open_hinge, PARTLY_OPEN_DEGREES)
            add_label(asset["id"], center + Vector((0.0, -0.12, 2.28)), True)
        else:
            center = Vector((column * cell_width, (rows - 1 - row) * cell_height, 0.0))
            closed_hinge = center + Vector((-1.15, -0.35, 0.0))
            open_hinge = center + Vector((0.75, -0.35, 0.0))
            place_contact_variant(asset, closed_hinge, 0.0)
            place_contact_variant(asset, open_hinge, PARTLY_OPEN_DEGREES)
            add_label(asset["id"], center + Vector((0.0, 0.65, 0.08)), False)

    if view == "front":
        target = Vector(((columns - 1) * cell_width * 0.5, 0.0, (rows - 1) * cell_height * 0.5 + 1.0))
        camera.location = target + Vector((0.0, -30.0, 0.0))
        camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
        camera.data.ortho_scale = columns * cell_width + 1.5
    else:
        target = Vector(((columns - 1) * cell_width * 0.5, (rows - 1) * cell_height * 0.5, 0.0))
        camera.location = target + Vector((0.0, 0.0, 30.0))
        camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
        camera.data.ortho_scale = columns * cell_width + 1.5
    bpy.context.view_layer.update()
    contact_directory = work_directory / "contact_sheets"
    contact_directory.mkdir(parents=True, exist_ok=True)
    output = contact_directory / f"swing_doors_{view}.png"
    bpy.context.scene.render.filepath = str(output)
    bpy.ops.render.render(write_still=True)
    if not output.is_file() or output.stat().st_size <= 0:
        fail(f"contact sheet was not rendered: {relative(output)}")
    print(f"Rendered {relative(output)}")
    return output


def verify_assets(work_directory: Path, documents: dict[str, tuple[dict, bytes, bytes]]) -> dict:
    catalog = load_and_validate_catalog()
    specs = {spec["id"]: spec for spec in STYLE_SPECS}
    result = {
        "toolVersion": TOOL_VERSION,
        "sources": verify_recorded_source_hashes(),
        "assets": [],
    }
    expected_models = set()
    for asset in catalog["assets"]:
        spec = specs[asset["id"]]
        record = {"id": asset["id"]}
        for part, field in (("leaf", "leafModelPath"), ("frame", "frameModelPath")):
            if field not in asset:
                continue
            model_path = REPOSITORY_ROOT / asset[field]
            binary_path = model_path.with_suffix(".bin")
            if not model_path.is_file() or not binary_path.is_file():
                fail(f"catalog model or binary is missing for {asset['id']} {part}")
            expected_models.update((model_path.name, binary_path.name))
            record[part] = {
                "raw": verify_raw_gltf(spec, asset, part, model_path, documents[spec["source_pack"]][0]),
                "reimport": verify_imported_model(spec, asset, part, model_path),
            }
        result["assets"].append(record)

    actual_models = {path.name for path in OUTPUT_DIRECTORY.iterdir() if path.suffix in {".gltf", ".bin"}}
    if actual_models != expected_models:
        fail(
            "prepared model set contains missing or unrelated files; "
            f"missing={sorted(expected_models - actual_models)}, unexpected={sorted(actual_models - expected_models)}"
        )
    expected_atlases = {SOURCE_PACKS[spec["source_pack"]]["atlas"] for spec in STYLE_SPECS}
    actual_pngs = {path.name for path in OUTPUT_DIRECTORY.glob("*.png")}
    if actual_pngs != expected_atlases:
        fail(f"prepared output must contain only the two shared atlases; found {sorted(actual_pngs)}")
    if not (OUTPUT_DIRECTORY / "ATTRIBUTION.md").is_file():
        fail("prepared attribution file is missing")

    front = render_contact_sheet(catalog, work_directory, "front")
    top = render_contact_sheet(catalog, work_directory, "top")
    result["contactSheets"] = {
        "front": relative(front),
        "top": relative(top),
        "inspected": False,
        "note": "Set by the execution log after a person or agent inspects the diagnostic images.",
    }
    work_directory.mkdir(parents=True, exist_ok=True)
    output = work_directory / "verification.json"
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Verified {len(catalog['assets'])} swing-door assets; wrote {relative(output)}")
    return result


def main() -> None:
    arguments = parse_arguments()
    work_directory = arguments.work_directory
    if not work_directory.is_absolute():
        work_directory = REPOSITORY_ROOT / work_directory
    before_hashes = verify_recorded_source_hashes()
    documents = load_source_documents()
    if arguments.mode in ("inspect", "all"):
        inspect_assets(work_directory, documents)
    if arguments.mode in ("prepare", "all"):
        prepare_assets(documents)
    if arguments.mode in ("verify", "all"):
        verify_assets(work_directory, documents)
    verify_source_hashes_unchanged(before_hashes)


if __name__ == "__main__":
    main()
