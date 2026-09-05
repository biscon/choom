#!/usr/bin/env python3
"""Offline GLSL validation of runtime reflection consumers and capture/filter shaders.

Requires glslangValidator; does not open a window or create a graphics context.
"""
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
RENDERER = ROOT / "sources/sector_demo/renderer"
RAW = re.compile(r'R"(\w*)\((.*?)\)\1"', re.DOTALL)


def sources(path, macros):
    text = path.read_text()
    result = {}
    for assignment in re.finditer(r'const char\s*\*\s*(\w+)\s*=\s*', text):
        cursor = assignment.end()
        if not RAW.match(text, cursor):
            continue
        parts = []
        while True:
            cursor += len(text[cursor:]) - len(text[cursor:].lstrip())
            raw = RAW.match(text, cursor)
            if raw:
                parts.append(raw.group(2))
                cursor = raw.end()
            elif text[cursor] == ';':
                result[assignment.group(1)] = ''.join(parts)
                break
            else:
                macro = re.match(r'[A-Z_]+', text[cursor:])
                if not macro or macro.group() not in macros:
                    raise ValueError(f"Unsupported shader expression in {path}:{cursor}")
                parts.append(macros[macro.group()])
                cursor += len(macro.group())
    return result


def main():
    macros = {}
    for path in RENDERER.glob('*.h'):
        text = path.read_text()
        for definition in re.finditer(r'#define\s+(SECTOR_\w+_GLSL)\s+', text):
            raw = RAW.match(text, definition.end())
            if raw:
                macros[definition.group(1)] = raw.group(2)
    shared = sources(RENDERER / 'SectorReflectionSampling.cpp', macros)['ReflectionSource']
    consumers = {
        'SectorMeshRenderer.cpp': ('SectorLightmapVs', 'SectorLightmapFs'),
        'SectorDoorRenderer.cpp': ('SectorDoorOpaqueVs', 'SectorDoorOpaqueFs'),
        'SectorStaticModelRenderer.cpp': ('SectorStaticModelVs', 'SectorStaticModelFs'),
        'SectorWindowRenderer.cpp': ('WindowVs', 'WindowFs'),
        'SectorLiquidRenderer.cpp': ('LiquidVs', 'LiquidFs'),
        'SectorRuntimeReflectionProbes.cpp': ('FilterVs', 'FilterFs'),
    }
    for filename, names in consumers.items():
        shaders = sources(RENDERER / filename, macros)
        for name, stage in zip(names, ('vert', 'frag')):
            source = shaders[name]
            if stage == 'frag' and name != 'FilterFs':
                offset = source.index('\n', source.index('#version')) + 1
                source = source[:offset] + shared + source[offset:]
            print(f'Checking {filename}: {name}', flush=True)
            subprocess.run(['glslangValidator', '--stdin', '-S', stage],
                           input=source, text=True, check=True)


if __name__ == '__main__':
    main()
