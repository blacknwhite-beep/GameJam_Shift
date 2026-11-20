"""
Builds Phase 3 Crop Blueprints & bespoke 3D crop meshes (Blueprint Checklist 3.1 - 3.3):
  - /Game/Dev_Blueprint/Crops/BP_Crop_ShotgunCorn
  - /Game/Dev_Blueprint/Crops/BP_Crop_RifleWheat
  - /Game/Dev_Blueprint/Crops/BP_Crop_MedHerb

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nopause -unattended -NullRHI -nosound
"""
import math
import os
import unreal

CROPS_DIR = "/Game/Dev_Blueprint/Crops"
MESH_DIR = f"{CROPS_DIR}/Meshes"
MAT_DIR = f"{CROPS_DIR}/Materials"
AUDIO_DIR = "/Game/Dev_Blueprint/Audio"
FARM_LEVEL = "/Game/Dev_Blueprint/Maps/Lvl_Farm"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) or unreal.EditorLevelLibrary


def log(msg):
    unreal.log_warning(f"[CROPS] {msg}")


# ---------------------------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------------------------
def make_crop_base_material():
    path = f"{MAT_DIR}/M_CropBase"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = asset_tools.create_asset("M_CropBase", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.2, 0.5, 0.2, 1.0))

    emissive = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 80)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 240)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.75)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 360)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.0)

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def make_crop_mi(parent, name, color, roughness=0.75, metallic=0.0, emissive=(0.0, 0.0, 0.0)):
    path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
    mel.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(*emissive, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    eal.save_loaded_asset(mi)
    return mi


# ---------------------------------------------------------------------------------------------
# Procedural Mesh Primitives via StaticMeshDescription
# ---------------------------------------------------------------------------------------------
def add_box(desc, pg, center, size, rot_deg=(0.0, 0.0, 0.0)):
    cx, cy, cz = center
    hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
    rx, ry, rz = [math.radians(a) for a in rot_deg]

    def xform(x, y, z):
        # Pitch (Y), Yaw (Z), Roll (X)
        y1 = y * math.cos(rx) - z * math.sin(rx)
        z1 = y * math.sin(rx) + z * math.cos(rx)
        x2 = x * math.cos(ry) + z1 * math.sin(ry)
        z2 = -x * math.sin(ry) + z1 * math.cos(ry)
        x3 = x2 * math.cos(rz) - y1 * math.sin(rz)
        y3 = x2 * math.sin(rz) + y1 * math.cos(rz)
        return unreal.Vector(cx + x3, cy + y3, cz + z2)

    pts = [
        xform(-hx, -hy, -hz), xform(hx, -hy, -hz), xform(hx, hy, -hz), xform(-hx, hy, -hz),
        xform(-hx, -hy, hz), xform(hx, -hy, hz), xform(hx, hy, hz), xform(-hx, hy, hz),
    ]
    vids = [desc.create_vertex() for _ in range(8)]
    for v, p in zip(vids, pts):
        desc.set_vertex_position(v, p)

    faces = [
        (0, 1, 2, 3), (4, 7, 6, 5),
        (0, 4, 5, 1), (1, 5, 6, 2),
        (2, 6, 7, 3), (3, 7, 4, 0),
    ]
    uvs = [unreal.Vector2D(0, 0), unreal.Vector2D(1, 0), unreal.Vector2D(1, 1), unreal.Vector2D(0, 1)]
    for a, b, c, d in faces:
        vis = []
        for idx, uv in zip((a, b, c, d), uvs):
            vi = desc.create_vertex_instance(vids[idx])
            desc.set_vertex_instance_uv(vi, uv, 0)
            vis.append(vi)
        desc.create_triangle(pg, [vis[0], vis[1], vis[2]])
        desc.create_triangle(pg, [vis[0], vis[2], vis[3]])


def add_cylinder(desc, pg, base_center, radius_bottom, radius_top, height, sides=10, tilt_deg=(0.0, 0.0, 0.0)):
    cx, cy, cz = base_center
    rx, ry, rz = [math.radians(a) for a in tilt_deg]

    def xform(x, y, z):
        y1 = y * math.cos(rx) - z * math.sin(rx)
        z1 = y * math.sin(rx) + z * math.cos(rx)
        x2 = x * math.cos(ry) + z1 * math.sin(ry)
        z2 = -x * math.sin(ry) + z1 * math.cos(ry)
        x3 = x2 * math.cos(rz) - y1 * math.sin(rz)
        y3 = x2 * math.sin(rz) + y1 * math.cos(rz)
        return unreal.Vector(cx + x3, cy + y3, cz + z2)

    ring_b, ring_t = [], []
    for i in range(sides):
        ang = 2.0 * math.pi * i / sides
        vb = desc.create_vertex()
        desc.set_vertex_position(vb, xform(math.cos(ang) * radius_bottom, math.sin(ang) * radius_bottom, 0.0))
        vt = desc.create_vertex()
        desc.set_vertex_position(vt, xform(math.cos(ang) * radius_top, math.sin(ang) * radius_top, height))
        ring_b.append(vb)
        ring_t.append(vt)

    v_top = desc.create_vertex()
    desc.set_vertex_position(v_top, xform(0.0, 0.0, height))
    v_bot = desc.create_vertex()
    desc.set_vertex_position(v_bot, xform(0.0, 0.0, 0.0))

    def inst(v, u, vv):
        vi = desc.create_vertex_instance(v)
        desc.set_vertex_instance_uv(vi, unreal.Vector2D(u, vv), 0)
        return vi

    for i in range(sides):
        j = (i + 1) % sides
        b0, b1 = inst(ring_b[i], i / sides, 0.0), inst(ring_b[j], (i + 1) / sides, 0.0)
        t0, t1 = inst(ring_t[i], i / sides, 1.0), inst(ring_t[j], (i + 1) / sides, 1.0)
        desc.create_triangle(pg, [b0, b1, t1])
        desc.create_triangle(pg, [b0, t1, t0])
        desc.create_triangle(pg, [inst(ring_t[i], 0.5, 0.5), inst(ring_t[j], 0.5, 0.5), inst(v_top, 0.5, 0.5)])
        desc.create_triangle(pg, [inst(ring_b[j], 0.5, 0.5), inst(ring_b[i], 0.5, 0.5), inst(v_bot, 0.5, 0.5)])


def build_static_mesh(name, slots_materials, build_fn):
    path = f"{MESH_DIR}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mesh = eal.duplicate_asset("/Engine/BasicShapes/Cube", path)
    static_mats = [unreal.StaticMaterial(material_interface=mat, material_slot_name=slot) for slot, mat in slots_materials]
    mesh.set_editor_property("static_materials", static_mats)

    desc = mesh.create_static_mesh_description()
    pgs = {}
    for slot, _ in slots_materials:
        pg = desc.create_polygon_group()
        desc.set_polygon_group_material_slot_name(pg, slot)
        pgs[slot] = pg

    build_fn(desc, pgs)
    mesh.build_from_static_mesh_descriptions([desc], False, False)
    eal.save_loaded_asset(mesh)
    log(f"Built StaticMesh {path}")
    return mesh


def build_all_crop_meshes(mats):
    # 1. Tilled Soil Mound
    def build_dirt(desc, pgs):
        add_cylinder(desc, pgs["Dirt"], (0, 0, 0), 62.0, 50.0, 14.0, sides=12)

    sm_dirt = build_static_mesh("SM_Crop_DirtMound", [("Dirt", mats["dirt_dry"])], build_dirt)

    # 2. Young Sprout
    def build_sprout(desc, pgs):
        add_cylinder(desc, pgs["Leaf"], (0, 0, 8), 3.0, 2.0, 22.0, sides=8)
        add_box(desc, pgs["Leaf"], (11, 0, 26), (22, 9, 2.5), rot_deg=(0, -28, 0))
        add_box(desc, pgs["Leaf"], (-11, 0, 24), (22, 9, 2.5), rot_deg=(0, 28, 0))

    sm_sprout = build_static_mesh("SM_Crop_Sprout", [("Leaf", mats["leaf"])], build_sprout)

    # 3. Mature Shotgun Corn (tall stalk + leaves + red shotgun shell cobs with brass bases)
    def build_corn(desc, pgs):
        add_cylinder(desc, pgs["Stalk"], (0, 0, 8), 6.0, 3.5, 125.0, sides=10)
        for z, yaw, pitch in ((38, 25, -35), (62, 160, -30), (88, -80, -32)):
            add_box(desc, pgs["Leaf"], (math.cos(math.radians(yaw)) * 20, math.sin(math.radians(yaw)) * 20, z),
                    (38, 10, 2.5), rot_deg=(0, pitch, yaw))
        # 3 Shotgun Shell Cobs
        for z, yaw in ((52, 0), (74, 125), (92, 245)):
            dx, dy = math.cos(math.radians(yaw)) * 9.0, math.sin(math.radians(yaw)) * 9.0
            add_cylinder(desc, pgs["Brass"], (dx, dy, z), 5.2, 5.2, 7.0, sides=10, tilt_deg=(0, 35, yaw))
            add_cylinder(desc, pgs["Shell"], (dx * 1.55, dy * 1.55, z + 5.5), 4.9, 4.7, 22.0, sides=10, tilt_deg=(0, 35, yaw))

    sm_corn = build_static_mesh(
        "SM_Crop_ShotgunCorn",
        [("Stalk", mats["stalk"]), ("Leaf", mats["leaf"]), ("Shell", mats["shell_red"]), ("Brass", mats["brass"])],
        build_corn,
    )

    # 4. Mature Rifle Wheat (cluster of golden stalks + brass/copper 5.56mm cartridge heads)
    def build_wheat(desc, pgs):
        offsets = [(0, 0, 0, 0), (16, 10, 8, 12), (-15, 12, -9, 10), (12, -16, 10, -11),
                   (-16, -12, -10, -8), (22, -3, 14, 2), (-22, 2, -14, -3)]
        for ox, oy, tx, ty in offsets:
            add_cylinder(desc, pgs["Wheat"], (ox, oy, 8), 2.2, 1.5, 76.0, sides=7, tilt_deg=(ty, tx, 0))
            hx, hy, hz = ox + tx * 1.1, oy + ty * 1.1, 80.0
            add_cylinder(desc, pgs["Brass"], (hx, hy, hz), 3.6, 3.4, 18.0, sides=8, tilt_deg=(ty, tx, 0))
            add_cylinder(desc, pgs["Copper"], (hx + tx * 0.25, hy + ty * 0.25, hz + 18.0), 3.2, 0.4, 11.0, sides=8, tilt_deg=(ty, tx, 0))

    sm_wheat = build_static_mesh(
        "SM_Crop_RifleWheat",
        [("Wheat", mats["wheat"]), ("Brass", mats["brass"]), ("Copper", mats["copper"])],
        build_wheat,
    )

    # 5. Mature Medicinal Herb (glowing emerald leaves + red medicinal cross blossom)
    def build_herb(desc, pgs):
        for i in range(6):
            yaw = i * 60.0
            dx, dy = math.cos(math.radians(yaw)) * 18.0, math.sin(math.radians(yaw)) * 18.0
            add_box(desc, pgs["Herb"], (dx, dy, 22), (32, 15, 3.0), rot_deg=(0, -25, yaw))
        add_cylinder(desc, pgs["Herb"], (0, 0, 10), 4.0, 3.0, 34.0, sides=8)
        # Glowing Red Medkit Cross Blossom at center top
        add_box(desc, pgs["Blossom"], (0, 0, 48), (22, 7, 7))
        add_box(desc, pgs["Blossom"], (0, 0, 48), (7, 22, 7))

    sm_herb = build_static_mesh(
        "SM_Crop_MedHerb",
        [("Herb", mats["herb_glow"]), ("Blossom", mats["med_blossom"])],
        build_herb,
    )

    return {
        "dirt": sm_dirt,
        "sprout": sm_sprout,
        "corn": sm_corn,
        "wheat": sm_wheat,
        "herb": sm_herb,
    }


# ---------------------------------------------------------------------------------------------
# Audio & Crop Blueprints
# ---------------------------------------------------------------------------------------------
def import_crop_pop_sound():
    wav = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "SFX_CropPop.wav")
    path = f"{AUDIO_DIR}/SFX_CropPop"
    if not eal.does_asset_exist(path) and os.path.exists(wav):
        task = unreal.AssetImportTask()
        task.filename = wav
        task.destination_path = AUDIO_DIR
        task.destination_name = "SFX_CropPop"
        task.automated = True
        task.save = True
        task.replace_existing = True
        asset_tools.import_asset_tasks([task])
    return unreal.load_asset(path)


def create_crop_blueprint(bp_name, item_id, qty, floating_text, mature_mesh, meshes, mats, pop_sound):
    bp_path = f"{CROPS_DIR}/{bp_name}"
    if eal.does_asset_exist(bp_path):
        eal.delete_asset(bp_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.FarmPlotBase)
    bp = asset_tools.create_asset(bp_name, CROPS_DIR, unreal.Blueprint, factory)
    if not bp:
        log(f"ERROR creating {bp_path}")
        return None

    gen_cls = bp.generated_class()
    cdo = unreal.get_default_object(gen_cls)

    cdo.set_editor_property("StageDuration", 10.0)
    cdo.set_editor_property("bPreserveConfiguredYield", True)
    yield_data = unreal.CropHarvestData()
    yield_data.set_editor_property("ItemID", unreal.Name(item_id))
    yield_data.set_editor_property("Quantity", qty)
    cdo.set_editor_property("HarvestYield", yield_data)
    cdo.set_editor_property("HarvestFloatingText", floating_text)
    cdo.set_editor_property("DrySoilMaterial", mats["dirt_dry"])
    cdo.set_editor_property("WetSoilMaterial", mats["dirt_wet"])
    if pop_sound:
        cdo.set_editor_property("HarvestPopSound", pop_sound)

    # Assign components on the Blueprint CDO
    soil_comp = cdo.get_editor_property("SoilMesh")
    if soil_comp:
        soil_comp.set_static_mesh(meshes["dirt"])
        soil_comp.set_material(0, mats["dirt_dry"])
        soil_comp.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
        soil_comp.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))

    sprout_comp = cdo.get_editor_property("Mesh_Sprout")
    if sprout_comp:
        sprout_comp.set_static_mesh(meshes["sprout"])

    mature_comp = cdo.get_editor_property("Mesh_Mature")
    if mature_comp:
        mature_comp.set_static_mesh(mature_mesh)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    log(f"Created {bp_path} (Yield: {item_id} x{qty}, StageDuration: 10.0s)")
    return bp


def upgrade_farm_level_plots():
    if not eal.does_asset_exist(FARM_LEVEL):
        return
    world = unreal.EditorLoadingAndSavingUtils.load_map(FARM_LEVEL)
    all_actors = actor_sub.get_all_level_actors()
    old_plots = [a for a in all_actors if a.get_class().get_name() == "FarmPlotBase"]
    coords = [(a.get_actor_location(), a.get_actor_label()) for a in old_plots]
    for a in old_plots:
        actor_sub.destroy_actor(a)

    corn_cls = unreal.load_object(None, f"{CROPS_DIR}/BP_Crop_ShotgunCorn.BP_Crop_ShotgunCorn_C")
    wheat_cls = unreal.load_object(None, f"{CROPS_DIR}/BP_Crop_RifleWheat.BP_Crop_RifleWheat_C")
    herb_cls = unreal.load_object(None, f"{CROPS_DIR}/BP_Crop_MedHerb.BP_Crop_MedHerb_C")
    fallback_cls = unreal.load_class(None, "/Script/GameJam_Shift_2026.FarmPlotBase")

    palette = [corn_cls or fallback_cls, wheat_cls or fallback_cls, herb_cls or fallback_cls, corn_cls or fallback_cls]
    coords.sort(key=lambda item: (item[0].x, item[0].y))
    for idx, (loc, label) in enumerate(coords):
        row_cls = palette[(idx // 4) % len(palette)]
        p = actor_sub.spawn_actor_from_class(row_cls, loc, unreal.Rotator(0, 0, 0))
        p.set_actor_label(label)
        p.set_folder_path("Farm/Field/Plots")

    unreal.EditorLoadingAndSavingUtils.save_map(world, FARM_LEVEL)
    log(f"Upgraded {len(coords)} plots in {FARM_LEVEL} to use BP_Crop_ShotgunCorn, BP_Crop_RifleWheat, BP_Crop_MedHerb")


def main():
    eal.make_directory(CROPS_DIR)
    eal.make_directory(MESH_DIR)
    eal.make_directory(MAT_DIR)

    base = make_crop_base_material()
    mats = {
        "dirt_dry": make_crop_mi(base, "MI_Crop_DirtDry", (0.16, 0.10, 0.05), roughness=0.96),
        "dirt_wet": make_crop_mi(base, "MI_Crop_DirtWet", (0.04, 0.025, 0.015), roughness=0.25),
        "leaf": make_crop_mi(base, "MI_Crop_LeafGreen", (0.09, 0.34, 0.06), roughness=0.72),
        "stalk": make_crop_mi(base, "MI_Crop_CornStalk", (0.18, 0.36, 0.07), roughness=0.75),
        "shell_red": make_crop_mi(base, "MI_Crop_ShellRed", (0.68, 0.04, 0.04), roughness=0.35),
        "brass": make_crop_mi(base, "MI_Crop_Brass", (0.78, 0.58, 0.18), roughness=0.28, metallic=0.88),
        "copper": make_crop_mi(base, "MI_Crop_Copper", (0.72, 0.32, 0.14), roughness=0.3, metallic=0.85),
        "wheat": make_crop_mi(base, "MI_Crop_WheatGold", (0.75, 0.55, 0.12), roughness=0.78),
        "herb_glow": make_crop_mi(base, "MI_Crop_HerbGlow", (0.06, 0.45, 0.18), roughness=0.55, emissive=(0.04, 0.35, 0.15)),
        "med_blossom": make_crop_mi(base, "MI_Crop_MedBlossom", (0.82, 0.08, 0.08), roughness=0.45, emissive=(0.45, 0.04, 0.04)),
    }

    meshes = build_all_crop_meshes(mats)
    pop_sound = import_crop_pop_sound()

    create_crop_blueprint("BP_Crop_ShotgunCorn", "Item_Ammo_Shotgun", 16, "+16 Shotgun Shells", meshes["corn"], meshes, mats, pop_sound)
    create_crop_blueprint("BP_Crop_RifleWheat", "Item_Ammo_Rifle", 30, "+30 Rifle Ammo", meshes["wheat"], meshes, mats, pop_sound)
    create_crop_blueprint("BP_Crop_MedHerb", "Item_Medkit", 1, "+1 Medkit", meshes["herb"], meshes, mats, pop_sound)

    upgrade_farm_level_plots()


main()
