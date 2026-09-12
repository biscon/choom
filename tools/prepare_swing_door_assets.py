#!/usr/bin/env python3
"""Build/verify the authored doors; retained wooden exports are read-only inputs.

blender --background --python tools/prepare_swing_door_assets.py -- --mode all
python3 tools/door_assets/validate.py

Blender MCP can import door_assets.authoring and call individual steps instead.
"""
from pathlib import Path
import argparse
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from door_assets import authoring


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('materials', 'prepare', 'verify', 'render', 'all'), default='verify')
    parser.add_argument('--asset', choices=list(authoring.SPECS))
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    if args.mode in ('materials', 'all'):
        authoring.bake_material_maps()
    if args.mode in ('prepare', 'all'):
        for asset_id in ([args.asset] if args.asset else authoring.SPECS):
            authoring.build_asset(asset_id)
        authoring.assemble_source()
    if args.mode in ('verify', 'all'):
        authoring.verify()
    if args.mode in ('render', 'all'):
        for view in ('front', 'rear', 'open', 'mirrored'):
            authoring.render_sheet(view)
        for asset_id in ([args.asset] if args.asset else authoring.SPECS):
            authoring.render_detail(asset_id)


if __name__ == '__main__':
    main()
