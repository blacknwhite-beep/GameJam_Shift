import bpy
import os
from mathutils import Vector, Euler
import math

OUT_DIR = r"C:\UnrealProjects\GameJam_Shift_2026\Scripts\CivilianExport"
SRC_FBX = os.path.join(OUT_DIR, "SKM_Farmer_male.fbx")
TEX_PATH = os.path.join(OUT_DIR, "T_Animalstextures.tga")

def srgb_to_linear(c):
    if c <= 0.04045:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4

def make_mat(name, hex_rgb, roughness=0.78):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    r, g, b = hex_rgb
    col = (srgb_to_linear(r/255.0), srgb_to_linear(g/255.0), srgb_to_linear(b/255.0), 1.0)
    bsdf.inputs["Base Color"].default_value = col
    bsdf.inputs["Roughness"].default_value = roughness
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    # Also set viewport display color for Workbench rendering
    mat.diffuse_color = col
    return mat

def load_palette_sampler():
    img = bpy.data.images.load(TEX_PATH)
    w, h = img.size
    pixels = list(img.pixels)
    def sample_rgb(u, v):
        u = max(0.0, min(0.9999, u % 1.0))
        v = max(0.0, min(0.9999, v % 1.0))
        idx = (int(v * h) * w + int(u * w)) * 4
        return (round(pixels[idx]*255), round(pixels[idx+1]*255), round(pixels[idx+2]*255))
    return sample_rgb

def add_lowpoly_mustache(mesh_obj, hair_mat_idx):
    import bmesh
    mw = mesh_obj.matrix_world
    mw_inv = mw.inverted()

    bm = bmesh.new()
    bm.from_mesh(mesh_obj.data)
    deform_layer = bm.verts.layers.deform.verify()
    head_vg = mesh_obj.vertex_groups.get("head")
    head_idx = head_vg.index if head_vg else None

    # Create a clean low-poly chevron/bar mustache between nose (Z=1.657) and lips (Z=1.620)
    # Center block + slightly drooping left/right tips for authentic low-poly character charm
    # World coordinates: front face is Y = -0.0777
    def add_box(x0, x1, y0, y1, z0_in, z1_in, z0_out, z1_out, is_left=False, is_right=False):
        # 8 corners in world space
        if is_left:
            # inner is x1 (closer to 0), outer is x0 (more negative)
            pts_w = [
                (x0, y0, z0_out), (x1, y0, z0_in), (x1, y1, z0_in), (x0, y1, z0_out),
                (x0, y0, z1_out), (x1, y0, z1_in), (x1, y1, z1_in), (x0, y1, z1_out),
            ]
        elif is_right:
            # inner is x0 (closer to 0), outer is x1 (more positive)
            pts_w = [
                (x0, y0, z0_in), (x1, y0, z0_out), (x1, y1, z0_out), (x0, y1, z0_in),
                (x0, y0, z1_in), (x1, y0, z1_out), (x1, y1, z1_out), (x0, y1, z1_in),
            ]
        else:
            pts_w = [
                (x0, y0, z0_in), (x1, y0, z0_in), (x1, y1, z0_in), (x0, y1, z0_in),
                (x0, y0, z1_in), (x1, y0, z1_in), (x1, y1, z1_in), (x0, y1, z1_in),
            ]
        bverts = []
        for p in pts_w:
            v = bm.verts.new(mw_inv @ Vector(p))
            if head_idx is not None:
                v[deform_layer][head_idx] = 1.0
            bverts.append(v)

        face_indices = [
            (0, 3, 2, 1), # bottom
            (4, 5, 6, 7), # top
            (0, 1, 5, 4), # front (-Y)
            (2, 3, 7, 6), # back (+Y)
            (0, 4, 7, 3), # left (-X)
            (1, 2, 6, 5), # right (+X)
        ]
        for fi in face_indices:
            f = bm.faces.new([bverts[i] for i in fi])
            f.material_index = hair_mat_idx

    # Left half of mustache (-0.044 .. 0.000) and right half (0.000 .. +0.044) with subtle classic chevron slant
    add_box(-0.044,  0.000, -0.090, -0.072, 1.626, 1.646, 1.616, 1.636, is_left=True)
    add_box( 0.000,  0.044, -0.090, -0.072, 1.626, 1.646, 1.616, 1.636, is_right=True)

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh_obj.data)
    bm.free()

def build_civilian(variant_name, shirt_rgb, pants_rgb, shoes_rgb, is_shorts=False, remove_beard=True, add_mustache=False):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC_FBX)

    sample_rgb = load_palette_sampler()
    mesh_obj = [o for o in bpy.data.objects if o.type == 'MESH'][0]
    mesh = mesh_obj.data
    uv_layer = mesh.uv_layers.active
    mw = mesh_obj.matrix_world
    mw_inv = mw.inverted()

    # Materials
    m_skin   = make_mat(f"M_{variant_name}_Skin",   (242, 188, 170), 0.72)
    m_hair   = make_mat(f"M_{variant_name}_Hair",   (118,  86,  62), 0.85)
    m_eyew   = make_mat(f"M_{variant_name}_EyeW",   (245, 245, 245), 0.35)
    m_eyep   = make_mat(f"M_{variant_name}_EyeP",   ( 32,  32,  36), 0.25)
    m_lips   = make_mat(f"M_{variant_name}_Lips",   (212, 110,  92), 0.65)
    m_shirt  = make_mat(f"M_{variant_name}_Shirt",  shirt_rgb,       0.80)
    m_pants  = make_mat(f"M_{variant_name}_Pants",  pants_rgb,       0.82)
    m_shoes  = make_mat(f"M_{variant_name}_Shoes",  shoes_rgb,       0.70)

    mats = [m_skin, m_hair, m_eyew, m_eyep, m_lips, m_shirt, m_pants, m_shoes]
    mesh.materials.clear()
    for m in mats:
        mesh.materials.append(m)

    IDX_SKIN, IDX_HAIR, IDX_EYEW, IDX_EYEP, IDX_LIPS, IDX_SHIRT, IDX_PANTS, IDX_SHOES = range(8)

    # First, flatten the raised green overall strap vertices slightly inward in Y so they sit flush with the shirt
    # Identify vertices belonging exclusively to the green strap faces (Z > 1.19, |X| in [0.045..0.098], green RGB)
    strap_verts = set()
    non_strap_verts = set()
    for poly in mesh.polygons:
        u = sum(uv_layer.data[li].uv.x for li in poly.loop_indices) / len(poly.loop_indices)
        v = sum(uv_layer.data[li].uv.y for li in poly.loop_indices) / len(poly.loop_indices)
        r, g, b = sample_rgb(u, v)
        c = mw @ poly.center
        is_green = (g > r + 20 and g > b + 5)
        if is_green and c.z > 1.195 and 0.042 <= abs(c.x) <= 0.102:
            for vi in poly.vertices:
                strap_verts.add(vi)
        else:
            for vi in poly.vertices:
                non_strap_verts.add(vi)

    # Pull strap-only verts slightly inward toward Y=0 (by 12%) so the strap ridge disappears into the shirt
    for vi in strap_verts:
        if vi not in non_strap_verts:
            wco = mw @ mesh.vertices[vi].co
            wco.y *= 0.86
            mesh.vertices[vi].co = mw_inv @ wco

    # Assign clean civilian material indices per face
    for poly in mesh.polygons:
        u = sum(uv_layer.data[li].uv.x for li in poly.loop_indices) / len(poly.loop_indices)
        v = sum(uv_layer.data[li].uv.y for li in poly.loop_indices) / len(poly.loop_indices)
        r, g, b = sample_rgb(u, v)
        c = mw @ poly.center
        ax = abs(c.x)
        z = c.z

        # 1. Head / Face / Neck (Z > 1.512, or already skin-colored near collar)
        is_orig_skin = (r > 210 and g > 160 and b > 140)
        if z > 1.512 or (z > 1.48 and is_orig_skin and ax < 0.12):
            if r > 230 and g > 230 and b > 230:
                poly.material_index = IDX_EYEW
            elif r < 60 and g < 60 and b < 60:
                poly.material_index = IDX_EYEP
            elif r > 195 and g < 120 and b < 90:
                poly.material_index = IDX_LIPS
            elif r < 175 and g < 130 and b < 100:
                # Beard sits on lower jaw/cheeks (Z < 1.705 and toward front/sides Y < 0.025)
                if remove_beard and z < 1.705 and c.y < 0.025:
                    poly.material_index = IDX_SKIN
                else:
                    poly.material_index = IDX_HAIR
            else:
                poly.material_index = IDX_SKIN
            continue

        # 2. Hands & Forearms (short-sleeve t-shirt ends cleanly at elbow |X| ~ 0.448)
        if ax > 0.452:
            poly.material_index = IDX_SKIN
            continue

        # 3. Shoes / Feet (Z < 0.084)
        if z < 0.084:
            poly.material_index = IDX_SHOES
            continue

        # 4. Shirt (Torso down to civilian waist Z >= 1.034 + upper arm short sleeves at Z > 1.20)
        if z >= 1.034 or (ax > 0.165 and z > 1.20):
            poly.material_index = IDX_SHIRT
            continue

        # 5. Lower Body: Jeans vs Shorts
        if is_shorts:
            if z >= 0.502:
                poly.material_index = IDX_PANTS   # Shorts (waist to just above knee)
            else:
                poly.material_index = IDX_SKIN    # Bare calves / lower legs
        else:
            poly.material_index = IDX_PANTS       # Full-length jeans

    mesh.update()

    if add_mustache:
        add_lowpoly_mustache(mesh_obj, IDX_HAIR)
        mesh.update()

    # Export FBX preserving exact armature & skinning
    fbx_out = os.path.join(OUT_DIR, f"SKM_{variant_name}.fbx")
    bpy.ops.export_scene.fbx(
        filepath=fbx_out,
        use_selection=False,
        object_types={'ARMATURE', 'MESH'},
        add_leaf_bones=False,
        bake_anim=False,
        mesh_smooth_type='FACE',
        path_mode='AUTO',
    )
    print(f"Exported {fbx_out}")
    return fbx_out

fbx1 = build_civilian("Civilian_RedShirt_Jeans",  shirt_rgb=(202, 42, 42), pants_rgb=(46, 84, 142), shoes_rgb=(40, 44, 52), is_shorts=False, remove_beard=True, add_mustache=False)
fbx2 = build_civilian("Civilian_BlueShirt_Shorts", shirt_rgb=(42, 114, 214), pants_rgb=(194, 166, 120), shoes_rgb=(42, 54, 74), is_shorts=True, remove_beard=True, add_mustache=True)

# Now render a clean 3-up studio lineup: Original Farmer | Red Shirt + Jeans | Blue Shirt + Shorts
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.studio_light = 'studio.sl'
scene.display.shading.color_type = 'MATERIAL'
scene.render.resolution_x = 1500
scene.render.resolution_y = 850
scene.render.image_settings.file_format = 'PNG'
scene.world = bpy.data.worlds.new("World")
scene.world.color = (0.14, 0.16, 0.20)

def import_at_x(fbx_path, x_offset, yaw_deg=15.0, use_farmer_tex=False):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=fbx_path)
    added = [o for o in bpy.data.objects if o not in before]
    roots = [o for o in added if o.parent is None]
    for r in roots:
        r.location.x += x_offset
        r.rotation_euler.z += math.radians(yaw_deg)
    if use_farmer_tex:
        img = bpy.data.images.load(TEX_PATH)
        for o in added:
            if o.type == 'MESH':
                # Switch to texture preview for farmer? Actually workbench can only use 1 color_type,
                # so let's render with EEVEE!
                mat = bpy.data.materials.new("FarmerOrig")
                mat.use_nodes = True
                nt = mat.node_tree
                nt.nodes.clear()
                out = nt.nodes.new("ShaderNodeOutputMaterial")
                bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
                bsdf.inputs["Roughness"].default_value = 0.78
                tn = nt.nodes.new("ShaderNodeTexImage")
                tn.image = img
                nt.links.new(tn.outputs["Color"], bsdf.inputs["Base Color"])
                nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
                o.data.materials.clear()
                o.data.materials.append(mat)
    return added

import_at_x(SRC_FBX, -1.85, yaw_deg=14.0, use_farmer_tex=True)
import_at_x(fbx1,     0.00, yaw_deg=10.0, use_farmer_tex=False)
import_at_x(fbx2,     1.85, yaw_deg=-10.0, use_farmer_tex=False)

# Use EEVEE for clean lit render with both texture & material colors!
scene.render.engine = 'BLENDER_EEVEE'
scene.eevee.taa_render_samples = 32
scene.view_settings.view_transform = 'Standard'

# 3-point studio lighting
def add_light(name, ltype, loc, rot_deg, energy, color=(1,1,1), size=2.0):
    ld = bpy.data.lights.new(name, type=ltype)
    ld.energy = energy
    ld.color = color
    if ltype == 'AREA':
        ld.size = size
    lo = bpy.data.objects.new(name, ld)
    lo.location = Vector(loc)
    lo.rotation_euler = Euler([math.radians(a) for a in rot_deg], 'XYZ')
    scene.collection.objects.link(lo)

add_light("Key",  'AREA', (1.8, -3.5, 3.2), (55, 0, 28), 320, (1.0, 0.97, 0.92), 3.0)
add_light("Fill", 'AREA', (-2.8, -2.8, 2.2), (65, 0, -40), 160, (0.85, 0.92, 1.0), 3.5)
add_light("Rim",  'AREA', (0.0,  3.0, 2.8), (-125, 0, 0), 220, (0.95, 0.97, 1.0), 4.0)

# Subtle ground plane
bpy.ops.mesh.primitive_plane_add(size=12.0, location=(0, 0, -0.002))
floor = bpy.context.active_object
floor.data.materials.append(make_mat("StudioFloor", (46, 52, 62), 0.9))

cam_data = bpy.data.cameras.new("Cam")
cam_data.lens = 50.0
cam_obj = bpy.data.objects.new("Cam", cam_data)
scene.collection.objects.link(cam_obj)
scene.camera = cam_obj

# Front 3-up lineup
cam_obj.location = Vector((0.0, -6.2, 1.05))
direction = Vector((0.0, 0.0, 0.92)) - cam_obj.location
cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
scene.render.filepath = os.path.join(OUT_DIR, "civilians_v2_front.png")
bpy.ops.render.render(write_still=True)
print("Rendered civilians_v2_front.png")

# Back 3-up lineup
cam_obj.location = Vector((0.0, 6.2, 1.05))
direction = Vector((0.0, 0.0, 0.92)) - cam_obj.location
cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
scene.render.filepath = os.path.join(OUT_DIR, "civilians_v2_back.png")
bpy.ops.render.render(write_still=True)
print("Rendered civilians_v2_back.png")
