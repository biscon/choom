#!/usr/bin/env python3
"""Read-only checks of shipped glTF door geometry, PBR references and opaque coverage.

Requires numpy. Does not load any user-authored level or require a graphics context.
"""
import json
from pathlib import Path
from urllib.parse import unquote
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'assets/models/doors/swing'
NEW_IDS = {
    'wood_walnut_panel', 'painted_ivory_panel', 'painted_sage_panel',
    'kitchen_service', 'industrial_charcoal', 'security_reinforced',
    'security_institutional',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_model(path, authored):
    doc = json.loads(path.read_text())
    buffers = []
    for buf in doc['buffers']:
        resource = (path.parent / unquote(buf['uri'])).resolve()
        require(resource.is_relative_to(OUT), f'{path}: escaping buffer')
        data = resource.read_bytes()
        require(len(data) == buf['byteLength'], f'{path}: buffer length')
        buffers.append(data)
    for img in doc.get('images', []):
        resource = (path.parent / unquote(img['uri'])).resolve()
        require(resource.is_relative_to(OUT) and resource.is_file(), f'{path}: image {resource}')
    require(not doc.get('skins') and not doc.get('animations'), f'{path}: unexpected rig/animation')

    def accessor(index):
        a = doc['accessors'][index]
        require('sparse' not in a, f'{path}: unexpected sparse accessor')
        view = doc['bufferViews'][a['bufferView']]
        dtype = {5121: '<u1', 5123: '<u2', 5125: '<u4', 5126: '<f4'}[a['componentType']]
        columns = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        size = np.dtype(dtype).itemsize
        offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = view.get('byteStride', columns * size)
        require(a.get('byteOffset', 0) + (a['count']-1)*stride + columns*size <= view['byteLength'], f'{path}: accessor overruns view')
        return np.ndarray((a['count'], columns), dtype=dtype, buffer=buffers[view['buffer']], offset=offset, strides=(stride,size)).copy()

    triangles = []
    for node in doc.get('nodes', []):
        # All transforms must already be baked; engine uses its own hinge transform.
        if authored:
            require('matrix' not in node and all(abs(v) < 1e-6 for v in node.get('translation', [])), f'{path}: noncanonical translation')
            require(node.get('rotation', [0,0,0,1]) == [0,0,0,1], f'{path}: rotation not baked')
            require(node.get('scale', [1,1,1]) == [1,1,1], f'{path}: scale not baked')
        if 'mesh' not in node:
            continue
        for prim in doc['meshes'][node['mesh']]['primitives']:
            attrs = prim['attributes']
            require({'POSITION','NORMAL','TANGENT','TEXCOORD_0'} <= set(attrs), f'{path}: missing shading attributes')
            values = {name: accessor(index) for name,index in attrs.items()}
            for name,data in values.items():
                require(np.isfinite(data).all(), f'{path}: nonfinite {name}')
            positions = values['POSITION']
            indices = accessor(prim['indices']).reshape(-1)
            require(len(indices)%3 == 0 and indices.max() < len(positions), f'{path}: invalid triangle indices')
            require(prim.get('mode',4) == 4, f'{path}: nontriangle primitive')
            triangles.append(positions[indices.reshape(-1,3)])
            mat = doc['materials'][prim['material']]
            require(mat.get('alphaMode','OPAQUE') == 'OPAQUE', f'{path}: transparent material')
            if authored:
                require(not mat.get('doubleSided',False), f'{path}: double-sided shortcut')
                pbr = mat['pbrMetallicRoughness']
                require('baseColorTexture' in pbr and 'metallicRoughnessTexture' in pbr and 'normalTexture' in mat, f'{path}: incomplete PBR')
                require(pbr.get('baseColorFactor',[1]*4)[3] == 1, f'{path}: material opacity')
                for name in ['NORMAL','TANGENT']:
                    lengths = np.linalg.norm(values[name][:,:3],axis=1)
                    require(np.max(abs(lengths-1)) < .02, f'{path}: invalid {name} length')
    require(bool(triangles), f'{path}: empty model')
    tris = np.concatenate(triangles)
    return doc,tris


def coverage(tris, width, height):
    """A dense grid must hit front- and back-facing triangles over the whole leaf."""
    # Project glTF XY; triangle winding identifies front/back even without a GPU.
    a,b,c=tris[:,0,:2],tris[:,1,:2],tris[:,2,:2]
    v0,v1=b-a,c-a
    det=v0[:,0]*v1[:,1]-v0[:,1]*v1[:,0]
    valid=abs(det)>1e-12
    a,v0,v1,det=a[valid],v0[valid],v1[valid],det[valid]
    for x in np.linspace(.004,width-.004,39):
        for y in np.linspace(.004,height-.004,83):
            q=np.array([x,y])-a
            u=(q[:,0]*v1[:,1]-q[:,1]*v1[:,0])/det
            v=(v0[:,0]*q[:,1]-v0[:,1]*q[:,0])/det
            inside=(u>=-1e-6)&(v>=-1e-6)&(u+v<=1+1e-6)
            require(np.any(inside&(det>0)) and np.any(inside&(det<0)),f'Leaf coverage/winding gap at {x:.4f}, {y:.4f}')
    return 39*83


def validate():
    catalog=json.loads((OUT/'catalog.json').read_text())
    ids=[a['id'] for a in catalog['assets']]
    retained={f'wooden_interior_{i:03}' for i in range(1,10)}
    require(catalog['formatVersion']==1 and len(ids)==len(set(ids))==16,'Catalog version/count/duplicate IDs')
    require(set(ids)==retained|NEW_IDS,'Catalog ID set')
    require(not list(OUT.glob('industrial_metal_*')) and not list(OUT.glob('worn_wooden_*')) and not (OUT/'doors_metal_base_color.png').exists(),'Retired assets remain')
    report={}
    for asset in catalog['assets']:
        authored=asset['id'] in NEW_IDS
        record={}
        for part in ['leaf','frame']:
            doc,tris=read_model(ROOT/asset[part+'ModelPath'],authored)
            points=tris.reshape(-1,3);lo=points.min(axis=0);hi=points.max(axis=0)
            record[part]={'triangles':len(tris),'materials':len(doc['materials']),'boundsMin':lo.tolist(),'boundsMax':hi.tolist()}
            if authored:
                tol=.00003
                if part=='leaf':
                    require(abs(lo[0])<tol and abs(hi[0]-asset['nominalWidth'])<tol and abs(lo[1])<tol and abs(hi[1]-asset['nominalHeight'])<tol,f"{asset['id']}: leaf canonical bounds")
                    record['coverageRaysPerFace']=coverage(tris,asset['nominalWidth'],asset['nominalHeight'])
                else:
                    require(abs(lo[0]+hi[0])<tol and abs(lo[1])<tol,f"{asset['id']}: frame pivot")
                    require(abs(hi[0]-lo[0]-asset['frameOuterWidth'])<tol and abs(hi[1]-lo[1]-asset['frameOuterHeight'])<tol,f"{asset['id']}: frame metadata")
        total=sum(record[p]['triangles'] for p in ['leaf','frame'])
        if authored: require(total<=12000 if asset['id'].startswith('security_') else total<=8000,f"{asset['id']}: triangle budget {total}")
        record['totalTriangles']=total
        report[asset['id']]=record
    return report


if __name__=='__main__':
    print(json.dumps(validate(),indent=2))
