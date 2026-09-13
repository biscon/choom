"""Repeatable Blender construction of seven opaque door assemblies.

Blender: X across, Z up, -Y front. glTF: X across, Y up, +Z front.
Leaves pivot at bottom hinge; frames pivot at bottom center. Downloaded wooden
exports are retained inputs, never regenerated. See the asset README.
"""
import json
import math
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'assets/models/doors/swing'
TEX = OUT / 'textures'
SOURCE = OUT / 'source'
REVIEW = ROOT / 'build/swing_door_asset_work/new_doors'
WIDTH, HEIGHT, BOTTOM = .90, 2.05, .008
SPECS = {
    'wood_walnut_panel': dict(name='Walnut - Raised Panel', kind='wood', thickness=.045, finish='walnut', hardware='brass'),
    'painted_ivory_panel': dict(name='Ivory - Panel Door', kind='ivory', thickness=.045, finish='ivory', hardware='nickel'),
    'painted_sage_panel': dict(name='Sage - Three Panel', kind='sage', thickness=.045, finish='sage', hardware='nickel'),
    'kitchen_service': dict(name='Kitchen - Protected Service Door', kind='kitchen', thickness=.045, finish='ivory', hardware='steel'),
    'industrial_charcoal': dict(name='Industrial - Charcoal Steel', kind='industrial', thickness=.050, finish='charcoal', hardware='steel'),
    'security_reinforced': dict(name='Security - Reinforced Steel', kind='reinforced', thickness=.090, finish='reinforced', hardware='steel'),
    'security_institutional': dict(name='Security - Institutional Steel', kind='institutional', thickness=.075, finish='gray', hardware='steel'),
}
FINISHES = {
    'walnut': ('walnut', (.55, .46, .36, 1)),
    'ivory': ('enamel', (.88, .83, .71, 1)),
    'sage': ('enamel', (.24, .32, .23, 1)),
    'charcoal': ('enamel', (.063, .073, .078, 1)),
    'reinforced': ('enamel', (.15, .19, .17, 1)),
    'gray': ('enamel', (.34, .37, .35, 1)),
    # Same calibrated silver finish as the bathroom kit, without a dark tint.
    'steel': ('nickel_used', (1, 1, 1, 1)),
    'nickel': ('nickel_used', (1, 1, 1, 1)),
    'brass': ('brass', (1, 1, 1, 1)),
    'dark': ('enamel', (.018, .020, .018, 1)),
}


def reset():
    for obj in list(bpy.context.scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)


def active(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def image(path, linear=False):
    img = bpy.data.images.load(str(path), check_existing=True)
    img.colorspace_settings.name = 'Non-Color' if linear else 'sRGB'
    return img


def material(name):
    label = 'Door_' + name
    if label in bpy.data.materials:
        if 'door_tint' in bpy.data.materials[label]:
            return bpy.data.materials[label]
        bpy.data.materials[label].name = label + '_imported'
    family, tint = FINISHES[name]
    mat = bpy.data.materials.new(label)
    mat.use_nodes = True
    mat.diffuse_color = tint
    mat.use_backface_culling = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    base = nodes.new('ShaderNodeTexImage')
    base.image = image(TEX / f'{family}_basecolor.png')
    multiply = nodes.new('ShaderNodeMixRGB')
    multiply.blend_type = 'MULTIPLY'
    multiply.inputs[0].default_value = 1
    multiply.inputs[2].default_value = tint
    links.new(base.outputs['Color'], multiply.inputs[1])
    links.new(multiply.outputs[0], bsdf.inputs['Base Color'])
    normal = nodes.new('ShaderNodeTexImage')
    normal.image = image(TEX / f'{family}_normal_opengl.png', True)
    normalmap = nodes.new('ShaderNodeNormalMap')
    links.new(normal.outputs['Color'], normalmap.inputs['Color'])
    links.new(normalmap.outputs[0], bsdf.inputs['Normal'])
    properties = nodes.new('ShaderNodeTexImage')
    properties.image = image(TEX / f'{family}_orm.png', True)
    split = nodes.new('ShaderNodeSeparateColor')
    links.new(properties.outputs['Color'], split.inputs[0])
    links.new(split.outputs['Green'], bsdf.inputs['Roughness'])
    metallic_factor = 209 / 255 if name == 'brass' else 1.0
    metallic = nodes.new('ShaderNodeMath')
    metallic.operation = 'MULTIPLY'
    metallic.inputs[1].default_value = metallic_factor
    links.new(split.outputs['Blue'], metallic.inputs[0])
    links.new(metallic.outputs[0], bsdf.inputs['Metallic'])
    mat['door_tint'] = list(tint)
    mat['door_metallic_factor'] = metallic_factor
    return mat


def uv_project(obj, horizontal=False):
    """Consistent physical texel scale; grain follows the long axis of joinery."""
    mesh = obj.data
    uv = mesh.uv_layers.new(name='UVMap') if not mesh.uv_layers else mesh.uv_layers[0]
    center = Vector(tuple((min(v.co[i] for v in mesh.vertices)
                           + max(v.co[i] for v in mesh.vertices)) * .5 for i in range(3)))
    for poly in mesh.polygons:
        axis = max(range(3), key=lambda i: abs(poly.normal[i]))
        for loop_id in poly.loop_indices:
            co = mesh.vertices[mesh.loops[loop_id].vertex_index].co - center
            if axis == 1: u, v = co.x, co.z
            elif axis == 0: u, v = co.y, co.z
            else: u, v = co.y, co.x
            if horizontal: u, v = v, u
            uv.data[loop_id].uv = (u / 1.10 + .5, v / 2.20 + .5)


def finish(obj, name, mat, bevel=0, horizontal=False):
    obj.name = name
    active(obj)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.clear()
    obj.data.materials.append(material(mat))
    if bevel:
        mod = obj.modifiers.new('Machined edge radius', 'BEVEL')
        mod.width = bevel
        mod.segments = 2 if bevel >= .0015 else 1
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for p in obj.data.polygons: p.use_smooth = True
    weighted = obj.modifiers.new('Weighted corner normals', 'WEIGHTED_NORMAL')
    weighted.keep_sharp = True
    weighted.weight = 40
    bpy.ops.object.modifier_apply(modifier=weighted.name)
    uv_project(obj, horizontal)
    return obj


def box(name, loc, dims, mat, bevel=.002, horizontal=False):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.dimensions = dims
    return finish(obj, name, mat, bevel, horizontal)


def cylinder(name, loc, radius, depth, mat, axis='Y', vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    obj = bpy.context.object
    obj.rotation_euler = (math.pi / 2, 0, 0) if axis == 'Y' else ((0, math.pi / 2, 0) if axis == 'X' else (0, 0, 0))
    active(obj)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return finish(obj, name, mat, .0006)


def bar(name, points, radius, mat):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 6
    curve.bevel_depth = radius
    curve.bevel_resolution = 2
    curve.use_fill_caps = True
    spline = curve.splines.new('BEZIER')
    spline.bezier_points.add(len(points) - 1)
    for bp, point in zip(spline.bezier_points, points):
        bp.co = point
        bp.handle_left_type = 'AUTO'
        bp.handle_right_type = 'AUTO'
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    active(obj)
    bpy.ops.object.convert(target='MESH')
    return finish(bpy.context.object, name, mat)


def screw(name, x, y, z, mat, sign, radius=.003):
    cylinder(name, (x,y,z), radius, .0016, mat, vertices=12)
    box(name + '_slot', (x,y+sign*.001,z), (radius*1.3,.0004,.0006), 'dark', .0001)


def molding(name, bounds, y, sign, mat, raised=False):
    x0, x1, z0, z1 = bounds
    profile = [(0,0),(.004,.002),(.010,.007),(.016,.009),(.021,.005),(.029,-.003)]
    vertices=[]
    for inset, depth in profile:
        for x,z in [(x0+inset,z0+inset),(x1-inset,z0+inset),(x1-inset,z1-inset),(x0+inset,z1-inset)]:
            vertices.append((x,y+sign*depth,z))
    faces=[]
    for r in range(len(profile)-1):
        for c in range(4):
            n=(c+1)%4
            faces.append((r*4+c,r*4+n,(r+1)*4+n,(r+1)*4+c))
    for c in range(4):
        n=(c+1)%4
        faces.append((c,(len(profile)-1)*4+c,(len(profile)-1)*4+n,n))
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    obj=bpy.data.objects.new(name,mesh)
    bpy.context.collection.objects.link(obj)
    active(obj)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    finish(obj,name,mat,.0008)
    if raised:
        box(name+'_raised',((x0+x1)/2,sign*.011,(z0+z1)/2),
            (x1-x0-.092,.013,z1-z0-.092),mat,.005)


def panel_leaf(spec):
    t, mat, kind=spec['thickness'],spec['finish'],spec['kind']
    # Solid web guarantees opaque coverage behind decorative joinery.
    box('LeafPanel',(WIDTH/2,0,HEIGHT/2),(WIDTH-.240,.012,HEIGHT-.20),mat,.001)
    for x in [.061, WIDTH-.061]: box('Solid stile',(x,0,HEIGHT/2),(.122,t,HEIGHT),mat,.002)
    if kind=='sage':
        gaps=[(.185,.660),(.800,1.255),(1.395,1.920)]
        rails=[(0,.185),(.660,.800),(1.255,1.395),(1.920,HEIGHT)]
    else:
        gaps=[(.185,.775),(.935,1.920)]
        rails=[(0,.185),(.775,.935),(1.920,HEIGHT)]
    for lo,hi in rails:
        box('Cross grain rail',(WIDTH/2,0,(lo+hi)/2),(WIDTH-.244,t,hi-lo),mat,.002,True)
    for sign in [-1,1]:
        for i,(lo,hi) in enumerate(gaps):
            molding(f'Panel molding {sign} {i}',(.119,WIDTH-.119,lo-.002,hi+.002),sign*(t/2-.003),sign,mat,kind=='wood')


def flush_leaf(spec):
    t, mat, kind=spec['thickness'],spec['finish'],spec['kind']
    box('LeafPanel',(WIDTH/2,0,HEIGHT/2),(WIDTH,t,HEIGHT),mat,.003)
    for sign in [-1,1]:
        y=sign*(t/2+.0015)
        if kind=='kitchen':
            box('Stainless kick plate',(WIDTH/2,y,.22),(WIDTH-.075,.003,.35),'steel',.002)
            box('Stainless push plate',(.76,y,1.22),(.15,.003,.42),'steel',.002)
            for x in [.07,WIDTH-.07]:
                for z in [.072,.367]: screw('Kick plate screw',x,y+sign*.002,z,'steel',sign)
            for z in [1.035,1.405]: screw('Push plate screw',.76,y+sign*.002,z,'steel',sign)
        elif kind=='industrial':
            box('Lower steel scuff plate',(WIDTH/2,y,.13),(WIDTH-.09,.002,.18),'steel',.001)
            for x in [.018,WIDTH-.018]: box('Folded skin seam',(x,y,HEIGHT/2),(.014,.002,HEIGHT-.02),mat,.001)
        elif kind=='reinforced':
            for x in [.041,WIDTH-.041]:
                box('Reinforced edge strap',(x,y,HEIGHT/2),(.060,.008,HEIGHT-.026),mat,.002)
                for z in [.10,.50,1.04,1.57,1.95]: cylinder('Security fastener',(x,y+sign*.005,z),.007,.005,'steel',vertices=6)
            for z in [.060,.75,1.97]: box('Welded reinforcement',(WIDTH/2,y,z),(WIDTH-.055,.010,.065),mat,.002,True)
        elif kind=='institutional':
            plate_y = y + sign*.001
            box('Large lock reinforcement plate',(.765,plate_y,1.09),(.19,.005,.34),'steel',.003)
            for x in [.69,.84]:
                for z in [.945,1.235]: screw('Lock plate screw',x,plate_y+sign*.003,z,'steel',sign,.0035)


def cut_mortise_recess(spec):
    """A real recess keeps the plate, latch and leaf at distinct surface depths."""
    bodies = []
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            lo, hi = bounds([obj])
            if lo[0] < WIDTH-.004 < hi[0] and lo[2] < 1.025 < hi[2]:
                bodies.append(obj)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(WIDTH, 0, 1.025))
    cutter = bpy.context.object
    cutter.dimensions = (.008, spec['thickness']*.7+.0006, .1506)
    active(cutter)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for obj in bodies:
        active(obj)
        modifier = obj.modifiers.new('Recess behind mortise plate', 'BOOLEAN')
        modifier.operation = 'DIFFERENCE'
        modifier.solver = 'EXACT'
        modifier.object = cutter
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.data.objects.remove(cutter, do_unlink=True)


def hardware(spec):
    t,metal,kind=spec['thickness'],spec['hardware'],spec['kind']
    x,z=.795,1.045
    for sign in [-1,1]:
        y=sign*(t/2+.004)
        if kind=='wood':
            box('Brass handle backplate',(x,y,z-.055),(.042,.006,.22),metal,.008)
            for dz in [-.142,.032]: screw('Backplate screw',x,y+sign*.004,z+dz,metal,sign)
        else: cylinder('Handle rose',(x,y,z),.027,.006,metal)
        cylinder('Lever spindle',(x,y+sign*.015,z),.010,.025,metal)
        bar('Sculpted lever',[(x,y+sign*.023,z),(x-.018,y+sign*.043,z+.004),
                            (x-.060,y+sign*.047,z+.005),(x-.120,y+sign*.044,z+.004)],.0085,metal)
        lock_z=z-.09
        cylinder('Lock escutcheon',(x,y,lock_z),.019,.005,metal)
        cylinder('Solid cylinder plug',(x,y+sign*.004,lock_z),.010,.003,'brass' if kind=='wood' else 'steel')
        box('Recessed key slot',(x,y+sign*.006,lock_z),(.002,.0007,.011),'dark',.0004)
        if kind in ('reinforced','institutional'):
            dead_z=1.38 if kind=='reinforced' else 1.16
            cylinder('Deadlock housing',(.795,y,dead_z),.029,.009,metal)
            cylinder('Deadlock cylinder',(.795,y+sign*.006,dead_z),.012,.007,metal)
            if sign<0: box('Deadlock key recess',(.795,y+sign*.01,dead_z),(.002,.001,.013),'dark',.0004)
            else: box('Deadlock thumbturn',(.795,y+sign*.016,dead_z),(.040,.012,.011),metal,.004)
    box('Mortise faceplate',(WIDTH-.002,0,1.025),(.002,t*.7,.15),'steel',.001)
    box('Latch tongue',(WIDTH-.001,0,1.043),(.002,.013,.021),'steel',.0006)
    heavy=kind in ('reinforced','institutional')
    radius=.013 if heavy else .009
    depth=.11 if heavy else .075
    for z in [.20,1.02,1.85]:
        hy=-(t/2+radius*.55)
        box('Hinge leaf strap',(.029,hy+.004,z),(.050,.003,depth),metal,.001)
        for dz in [-depth*.33,depth*.33]: screw('Hinge fixing',.042,hy-.001,z+dz,metal,-1)
        for offset in ([-.034,0,.034] if heavy else [-.025,0,.025]):
            cylinder('Hinge knuckle',(radius,hy,z+offset),radius,depth/3-.002,metal,'Z')
        cylinder('Hinge pin cap',(radius,hy,z+depth/2+.001),radius*.85,.003,metal,'Z')


def frame(spec):
    kind,mat=spec['kind'],spec['finish']
    heavy=kind in ('reinforced','institutional')
    depth=.19 if heavy else .14
    jamb=.065 if heavy else .060
    for x in [-.003-jamb/2,WIDTH+.003+jamb/2]:
        box('Frame jamb',(x,0,(HEIGHT+BOTTOM+.005)/2),(jamb,depth,HEIGHT+BOTTOM+.005),mat,.002)
    box('Frame head',(WIDTH/2,0,HEIGHT+BOTTOM+.005+jamb/2),(WIDTH+.006+2*jamb,depth,jamb),mat,.002,True)
    for sign in [-1,1]:
        casing=.083 if heavy else .080
        for x in [-.004-casing/2,WIDTH+.004+casing/2]:
            box('Frame casing',(x,sign*(depth/2+.007),(HEIGHT+.019)/2),(casing,.020,HEIGHT+.017),mat,.003)
            if kind in ('wood','ivory','sage'):
                box('Casing bead',(x-sign*.018,sign*(depth/2+.019),(HEIGHT+.020)/2),(.011,.010,HEIGHT+.016),mat,.002)
        box('Head casing',(WIDTH/2,sign*(depth/2+.007),HEIGHT+.058),(WIDTH+.008+2*casing,.020,.080),mat,.003,True)
    # Recessed stops behind the closed leaf; the existing system determines swing.
    rear=spec['thickness']/2+.010
    for x in [-.004,WIDTH+.004]: box('Rebated stop',(x,rear,HEIGHT/2),(.008,.012,HEIGHT-.008),mat,.001)
    box('Head stop',(WIDTH/2,rear,HEIGHT+BOTTOM+.006),(WIDTH,.012,.010),mat,.001,True)
    box('Strike plate',(WIDTH+.003,0,1.025+BOTTOM),(.002,spec['thickness']*.8,.16),'steel',.001)


def consolidate(objects, root_name):
    root=bpy.data.objects.new(root_name,None)
    bpy.context.collection.objects.link(root)
    groups={}
    for obj in objects: groups.setdefault(obj.data.materials[0].name,[]).append(obj)
    for name,members in groups.items():
        bpy.ops.object.select_all(action='DESELECT')
        for obj in members: obj.select_set(True)
        bpy.context.view_layer.objects.active=members[0]
        if len(members) > 1:
            bpy.ops.object.join()
        obj=bpy.context.object
        obj.name=root_name+'_'+name
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        obj.parent=root
        mod=obj.modifiers.new('Export triangles','TRIANGULATE')
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return root


def bounds(objects):
    points=[o.matrix_world @ Vector(p) for o in objects for p in o.bound_box]
    return [min(p[i] for p in points) for i in range(3)],[max(p[i] for p in points) for i in range(3)]


def export(root,path):
    bpy.ops.object.select_all(action='DESELECT')
    root.select_set(True)
    for obj in root.children_recursive: obj.select_set(True)
    bpy.context.view_layer.objects.active=root
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLTF_SEPARATE',use_selection=True,
        export_yup=True,export_normals=True,export_tangents=True,export_materials='EXPORT',
        export_extras=True,export_cameras=False,export_lights=False,export_animations=False,
        export_texture_dir='textures',export_image_format='AUTO',export_keep_originals=True)
    doc=json.loads(path.read_text())
    for mat in doc.get('materials',[]):
        source=bpy.data.materials[mat['name']]
        pbr=mat['pbrMetallicRoughness']
        pbr['baseColorFactor']=list(source['door_tint'])
        pbr['metallicFactor']=source['door_metallic_factor']
        pbr['roughnessFactor']=1.0
        mat['occlusionTexture'] = dict(pbr['metallicRoughnessTexture'])
        mat['alphaMode']='OPAQUE'
        mat['doubleSided']=False
        mat.pop('extras',None)
    path.write_text(json.dumps(doc,indent=2)+'\n')


def build_asset(asset_id):
    spec=SPECS[asset_id]
    for folder in [OUT,SOURCE,REVIEW]: folder.mkdir(parents=True,exist_ok=True)
    reset()
    if spec['kind'] in ('wood','ivory','sage'): panel_leaf(spec)
    else: flush_leaf(spec)
    bpy.context.view_layer.update()
    cut_mortise_recess(spec)
    hardware(spec)
    leaf=consolidate(list(bpy.context.scene.objects),'DoorLeaf_'+asset_id)
    leaf['catalog_id']=asset_id;leaf['part']='leaf'
    export(leaf,OUT/f'{asset_id}_leaf.gltf')
    for obj in [leaf]+list(leaf.children_recursive): obj.hide_set(True)
    before=set(bpy.context.scene.objects)
    frame(spec)
    meshes=[o for o in bpy.context.scene.objects if o not in before]
    bpy.context.view_layer.update()
    lo,hi=bounds(meshes)
    center=(lo[0]+hi[0])/2
    for obj in meshes: obj.location.x-=center
    frame_root=consolidate(meshes,'DoorFrame_'+asset_id)
    frame_root['catalog_id']=asset_id;frame_root['part']='frame'
    export(frame_root,OUT/f'{asset_id}_frame.gltf')
    asset=dict(id=asset_id,displayName=spec['name'],leafModelPath=f'assets/models/doors/swing/{asset_id}_leaf.gltf',
        frameModelPath=f'assets/models/doors/swing/{asset_id}_frame.gltf',sourcePack='source/door_set.blend',
        nominalWidth=WIDTH,nominalHeight=HEIGHT,nominalThickness=spec['thickness'],
        frameOuterWidth=round(hi[0]-lo[0],6),frameOuterHeight=round(hi[2]-lo[2],6),
        leafHingeToFrameCenter=round(center,6),leafBottomOffset=BOTTOM)
    path=OUT/'catalog.json';catalog=json.loads(path.read_text())
    catalog['assets']=[a for a in catalog['assets'] if a['id']!=asset_id]+[asset]
    catalog['assets'].sort(key=lambda a:a['id'])
    path.write_text(json.dumps(catalog,indent=2)+'\n')
    print('EXPORTED',asset_id,asset)
    return asset


def bake_material_maps(family=None):
    TEX.mkdir(parents=True,exist_ok=True)
    for kind in ([family] if family else ['walnut','enamel','brass']):
        reset()
        scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1;scene.cycles.device='CPU'
        bpy.ops.mesh.primitive_plane_add(size=2)
        plane=bpy.context.object
        mat=bpy.data.materials.new('Bake_'+kind);mat.use_nodes=True;plane.data.materials.append(mat)
        nodes,links=mat.node_tree.nodes,mat.node_tree.links
        bsdf=nodes.get('Principled BSDF')
        coord=nodes.new('ShaderNodeTexCoord')
        scale=nodes.new('ShaderNodeVectorMath');scale.operation='MULTIPLY'
        scale.inputs[1].default_value=(190,9,1) if kind in ('walnut','steel','brass') else (170,75,1)
        links.new(coord.outputs['UV'],scale.inputs[0])
        noise=nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=1;noise.inputs['Detail'].default_value=2
        links.new(scale.outputs[0],noise.inputs['Vector'])
        bump=nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.25
        bump.inputs['Distance'].default_value=.0012 if kind=='walnut' else .00035
        links.new(noise.outputs['Fac'],bump.inputs['Height']);links.new(bump.outputs['Normal'],bsdf.inputs['Normal'])
        target=nodes.new('ShaderNodeTexImage')
        normal=bpy.data.images.new(kind+'_normal_opengl',width=1024,height=1024,alpha=False)
        normal.colorspace_settings.name='Non-Color';target.image=normal;nodes.active=target;active(plane)
        bpy.ops.object.bake(type='NORMAL',normal_space='TANGENT',margin=4)
        normal.filepath_raw=str(TEX/f'{kind}_normal_opengl.png');normal.file_format='PNG';normal.save()
        rough=nodes.new('ShaderNodeMapRange')
        rough.inputs['To Min'].default_value={'walnut':.28,'enamel':.39,'steel':.26,'brass':.26}[kind]
        rough.inputs['To Max'].default_value={'walnut':.42,'enamel':.51,'steel':.38,'brass':.37}[kind]
        links.new(noise.outputs['Fac'],rough.inputs['Value'])
        combine=nodes.new('ShaderNodeCombineColor');combine.inputs['Red'].default_value=1
        combine.inputs['Blue'].default_value=1 if kind in ('steel','brass') else 0
        links.new(rough.outputs[0],combine.inputs['Green'])
        emission=nodes.new('ShaderNodeEmission');links.new(combine.outputs[0],emission.inputs['Color'])
        links.new(emission.outputs[0],nodes.get('Material Output').inputs['Surface'])
        orm=bpy.data.images.new(kind+'_orm',width=1024,height=1024,alpha=False)
        orm.colorspace_settings.name='Non-Color';target.image=orm;nodes.active=target
        bpy.ops.object.bake(type='EMIT',margin=4)
        orm.filepath_raw=str(TEX/f'{kind}_orm.png');orm.file_format='PNG';orm.save()
        print('BAKED',kind)


def import_assembly(asset_id,position=(0,0,0),angle=0,mirror=False):
    asset=next(a for a in json.loads((OUT/'catalog.json').read_text())['assets'] if a['id']==asset_id)
    container=bpy.data.objects.new(asset_id,None);bpy.context.collection.objects.link(container)
    for part in ['leaf','frame']:
        before=set(bpy.context.scene.objects)
        bpy.ops.import_scene.gltf(filepath=str(ROOT/asset[part+'ModelPath']))
        imported=[o for o in bpy.context.scene.objects if o not in before]
        roots=[o for o in imported if o.parent not in imported];assert len(roots)==1
        obj=roots[0];obj.parent=container
        if part=='leaf':
            obj.location.z=asset['leafBottomOffset']
            obj.rotation_mode='XYZ'
            obj.rotation_euler.z=math.radians(angle + (180 if mirror else 0))
            if mirror:
                obj.location.x=2*asset['leafHingeToFrameCenter']
        else: obj.location.x=asset['leafHingeToFrameCenter']
    container.location=position
    return container


def assemble_source():
    reset()
    for i,asset_id in enumerate(SPECS): import_assembly(asset_id,(i*1.4,0,0))
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene['README']='Seven authored opaque swing doors. Separate canonical glTF leaves and frames. See README.md and tools/door_assets/authoring.py.'
    SOURCE.mkdir(parents=True,exist_ok=True)
    bpy.data.orphans_purge(do_recursive=True)
    # Portable source file: all images resolve alongside the kit, not in Codex storage.
    for img in bpy.data.images:
        if img.source == 'FILE':
            img.filepath = '//../textures/' + Path(bpy.path.abspath(img.filepath)).name
    previous_versions = bpy.context.preferences.filepaths.save_version
    try:
        bpy.context.preferences.filepaths.save_version = 0
        bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'door_set.blend'))
    finally:
        bpy.context.preferences.filepaths.save_version = previous_versions
    print('Saved editable source assembly')


def look_at(obj,target):
    obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


def studio():
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24
    scene.cycles.use_denoising=True;scene.cycles.device='CPU'
    scene.world.color=(.17,.17,.17);scene.view_settings.view_transform='AgX'
    for name,loc,power,size,color in [
        ('Key',(-3,-4,5),1100,5,(1,.91,.79)),('Fill',(5,-2,3),850,4,(.78,.88,1)),('Rim',(2,3,5),1250,3,(1,1,1))]:
        data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;data.color=color
        obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);obj.location=loc;look_at(obj,(2,0,1))
    bpy.ops.object.camera_add();scene.camera=bpy.context.object
    return scene


def render_sheet(view='front'):
    reset()
    for i,asset_id in enumerate(SPECS):
        col,row=i%4,i//4
        mirrored = view == 'mirrored'
        import_assembly(asset_id,(col*1.40,0,-row*2.60),
                        angle=-55 if view in ('open','mirrored') else 0,mirror=mirrored)
        font=bpy.data.curves.new('Label','FONT');font.body=SPECS[asset_id]['name'].replace(' - ','\n');font.align_x='CENTER';font.size=.095
        obj=bpy.data.objects.new('Label',font);bpy.context.collection.objects.link(obj)
        obj.location=(col*1.40+.45,-.23,-row*2.60-.19);obj.rotation_euler=(math.pi/2,0,0)
        if view == 'rear':
            obj.location.y = .23
            obj.rotation_euler.z = math.pi
    scene=studio();camera=scene.camera;camera.data.type='ORTHO';camera.data.ortho_scale=6.8
    camera.location=(2.55,-12 if view!='rear' else 12,-.25);look_at(camera,(2.55,0,-.25))
    scene.render.resolution_x=2000;scene.render.resolution_y=1700;scene.render.resolution_percentage=100
    REVIEW.mkdir(parents=True,exist_ok=True);scene.render.filepath=str(REVIEW/f'{view}.png')
    bpy.ops.render.render(write_still=True)
    return scene.render.filepath


def render_detail(asset_id, rear=False):
    reset()
    import_assembly(asset_id, angle=25 if rear else -25)
    scene=studio()
    scene.camera.location=(1.7, 3 if rear else -3, 1.60)
    look_at(scene.camera,(.72,.30 if rear else -.30,1.10))
    scene.camera.data.type='ORTHO'
    scene.camera.data.ortho_scale=.85
    scene.render.resolution_x=1000;scene.render.resolution_y=1000
    scene.render.filepath=str(REVIEW/(asset_id+('_rear_detail' if rear else '_detail')+'.png'))
    bpy.ops.render.render(write_still=True)
    return scene.render.filepath


def verify():
    from door_assets.validate import validate
    report=validate();REVIEW.mkdir(parents=True,exist_ok=True)
    (REVIEW/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return report
