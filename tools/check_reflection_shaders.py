#!/usr/bin/env python3
"""Offline GLSL validation of runtime reflection consumers and capture/filter shaders.

Requires glslangValidator and a C++17 compiler; no window or graphics context.
"""
import pathlib
import os
import re
import shlex
import subprocess
import tempfile

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


def validate(assembler):
    def insert_preamble(source, preamble):
        # Execute the same C++ insertion helper used by the runtime. Do not
        # reproduce its newline/version handling in Python.
        payload = preamble.encode() + source.encode()
        return subprocess.run([str(assembler)],
                              input=str(len(preamble.encode())).encode() + b'\n' + payload,
                              capture_output=True, check=True).stdout.decode()

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
                source = insert_preamble(source, shared)
            print(f'Checking {filename}: {name}', flush=True)
            subprocess.run(['glslangValidator', '--stdin', '-S', stage],
                           input=(insert_preamble(source, '#define WINDOW_FLAT_PASS 0\n')
                                  if name == 'WindowFs' else source), text=True, check=True)
            if name == 'WindowFs':
                for variant in (1, 2):
                    specialized = insert_preamble(source, f'#define WINDOW_FLAT_PASS {variant}\n')
                    print(f'Checking flat glass variant {variant}', flush=True)
                    subprocess.run(['glslangValidator', '--stdin', '-S', stage],
                                   input=specialized, text=True, check=True)
                    preprocessed = subprocess.run(
                        ['glslangValidator', '--stdin', '-S', stage, '-E'],
                        input=specialized, text=True, capture_output=True, check=True).stdout
                    assert 'discard' not in preprocessed, 'Flat glass must permit early depth rejection'
                    assert 'SampleTransmission(' not in preprocessed.replace(' ', ''), 'Flat glass must not sample scene refraction'


def main():
    # A tiny, graphics-free executable lets validation consume runtime-assembled
    # shader source, including leading whitespace in the actual C++ literals.
    driver = r'''
#include "sector_demo/renderer/SectorShaderSource.h"
#include <iostream>
#include <iterator>
int main() {
    std::size_t size = 0;
    std::cin >> size;
    std::cin.get();
    std::string preamble(size, '\0');
    std::cin.read(preamble.data(), size);
    std::string source((std::istreambuf_iterator<char>(std::cin)), {});
    std::cout << game::InsertSectorShaderPreamble(source, preamble);
}
'''
    with tempfile.TemporaryDirectory(prefix='sector-shader-check-') as directory:
        assembler = pathlib.Path(directory) / 'assemble'
        subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                       ['-std=c++17', '-x', 'c++', '-', '-I', str(ROOT / 'sources'),
                        '-o', str(assembler)], input=driver, text=True, check=True)
        validate(assembler)


if __name__ == '__main__':
    main()
