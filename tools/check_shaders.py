#!/usr/bin/env python3
"""Validate every project shader using the runtime C++ source loader (no GPU)."""
import argparse
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=pathlib.Path,
                        default=ROOT / 'cmake-build-debug/shader_source_tool')
    parser.add_argument('--root', type=pathlib.Path, default=ROOT / 'assets/shaders')
    parser.add_argument('--validator', default='glslangValidator')
    args = parser.parse_args()
    args.tool = args.tool.resolve()
    args.root = args.root.resolve()
    if not args.tool.is_file():
        parser.error('Build shader_source_tool first: cmake --build cmake-build-debug --target shader_source_tool')
    validator = shutil.which(args.validator)
    if not validator:
        parser.error('glslangValidator is required; install glslang tools or pass --validator PATH')
    catalog = subprocess.run([str(args.tool), '--list'], text=True,
                             capture_output=True, check=True).stdout.splitlines()
    covered = set()
    names = set()
    with tempfile.TemporaryDirectory(prefix='engine-shaders-') as directory:
        for row in catalog:
            name, vertex, fragment = row.split('\t')
            if name in names:
                raise RuntimeError(f'Duplicate program name: {name}')
            names.add(name)
            stages = []
            source_maps = []
            for stage, relative in [('vert', vertex), ('frag', fragment)]:
                if relative == '-':
                    continue
                if not relative.endswith(f'.{stage}.glsl'):
                    raise RuntimeError(f'Incorrect shader stage suffix: {relative}')
                covered.add(relative)
                source = subprocess.run([str(args.tool), '--source', str(args.root), name, stage],
                                        text=True, capture_output=True)
                if source.returncode:
                    raise RuntimeError(source.stderr)
                print(f'Checking {name}: {relative}', flush=True)
                result = subprocess.run([validator, '--stdin', '-S', stage],
                                        input=source.stdout, text=True, capture_output=True)
                if result.returncode:
                    raise RuntimeError(f'{name} {stage}\n{source.stderr}\n{result.stdout}{result.stderr}')
                if name in ('window_flat_transmission', 'window_flat_reflection') and stage == 'frag':
                    expanded = subprocess.run([validator, '--stdin', '-S', stage, '-E'],
                                              input=source.stdout, text=True,
                                              capture_output=True, check=True).stdout
                    if re.search(r'\bdiscard\b|\bSampleTransmission\s*\(', expanded):
                        raise RuntimeError(f'{name}: flat glass must allow early depth rejection and omit refraction')
                # glslang's link mode identifies stages from these temporary
                # suffixes; the maintained assets all retain .glsl endings.
                path = pathlib.Path(directory) / f'{name}.{stage}'
                path.write_text(source.stdout)
                stages.append(str(path))
                source_maps.append(f"{stage}:\n{source.stderr}")
            if len(stages) == 2:
                linked = subprocess.run([validator, '-l', *stages], capture_output=True, text=True)
                if linked.returncode:
                    raise RuntimeError(f'{name}: program linkage failed\n'
                                       + '\n'.join(source_maps) + '\n'
                                       + linked.stdout + linked.stderr)
    entry_files = {str(p.relative_to(args.root)) for pattern in ('*.vert.glsl', '*.frag.glsl')
                   for p in args.root.rglob(pattern)}
    if entry_files != covered:
        raise RuntimeError(f'Shader coverage mismatch: uncovered={entry_files - covered}, missing={covered - entry_files}')
    print(f'Validated {len(names)} programs/variants and {len(covered)} stage files.')


def run():
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f'Shader validation failed: {error}', file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stdout or '', error.stderr or '', file=sys.stderr)
        raise SystemExit(1) from None


if __name__ == '__main__':
    run()
