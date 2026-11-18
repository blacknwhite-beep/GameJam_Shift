import os
import unreal

SRC_DIR = r"D:\transfer\projects\models\Guns"
DEST_DIR = "/Game/Dev_Blueprint/Weapons"
MESH_DIR = f"{DEST_DIR}/Meshes"
MAT_DIR = f"{DEST_DIR}/Materials"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

for _cvar in ("Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.Texture 0"):
    unreal.SystemLibrary.execute_console_command(None, _cvar)

def log(msg):
    unreal.log_warning(f"[GUN_IMPORT] {msg}")

# 1. Inspect existing weapons
for w_path in [
    "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol",
    "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Rifle",
    "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_GrenadeLauncher",
]:
    bp = unreal.load_asset(w_path)
    if bp:
        cdo = unreal.get_default_object(bp.generated_class())
        fp = cdo.get_editor_property("FirstPersonMesh")
        tp = cdo.get_editor_property("ThirdPersonMesh")
        sk = fp.get_editor_property("skeletal_mesh_asset") if fp else None
        if sk:
            bb = sk.get_bounds()
            log(f"Existing {w_path}: mesh={sk.get_path_name()} origin={bb.origin} extent={bb.box_extent} fp_loc={fp.get_editor_property('relative_location')} fp_rot={fp.get_editor_property('relative_rotation')} fp_scale={fp.get_editor_property('relative_scale3d')}")
            for s_idx in range(sk.num_sockets()):
                sock = sk.get_socket_by_index(s_idx)
                log(f"  Socket {sock.socket_name}: rel_loc={sock.relative_location} rel_rot={sock.relative_rotation}")

# 2. Build clean PBR Gun materials
def build_gun_materials():
    m_path = f"{MAT_DIR}/M_GunMaster"
    if not eal.does_asset_exist(m_path):
        mat = asset_tools.create_asset("M_GunMaster", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        c_node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
        c_node.set_editor_property("parameter_name", "BaseColor")
        c_node.set_editor_property("default_value", unreal.LinearColor(0.14, 0.15, 0.17, 1.0))
        mel.connect_material_property(c_node, "", unreal.MaterialProperty.MP_BASE_COLOR)

        m_node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 100)
        m_node.set_editor_property("parameter_name", "Metallic")
        m_node.set_editor_property("default_value", 0.82)
        mel.connect_material_property(m_node, "", unreal.MaterialProperty.MP_METALLIC)

        r_node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 220)
        r_node.set_editor_property("parameter_name", "Roughness")
        r_node.set_editor_property("default_value", 0.34)
        mel.connect_material_property(r_node, "", unreal.MaterialProperty.MP_ROUGHNESS)

        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
    else:
        mat = unreal.load_asset(m_path)

    def make_mi(name, color, metallic, roughness):
        mi_path = f"{MAT_DIR}/{name}"
        if not eal.does_asset_exist(mi_path):
            mi = asset_tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        else:
            mi = unreal.load_asset(mi_path)
        mi.set_editor_property("parent", mat)
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor", color)
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        eal.save_loaded_asset(mi)
        return mi

    mi_dark = make_mi("MI_Gun_DarkSteel", unreal.LinearColor(0.11, 0.12, 0.14, 1.0), 0.85, 0.32)
    mi_wood = make_mi("MI_Gun_WalnutWood", unreal.LinearColor(0.36, 0.16, 0.06, 1.0), 0.05, 0.52)
    mi_khaki = make_mi("MI_Gun_DesertTan", unreal.LinearColor(0.58, 0.49, 0.34, 1.0), 0.35, 0.42)
    return {"Mat_Dark": mi_dark, "Mat_Wood": mi_wood, "Mat_Caqui": mi_khaki, "default": mi_dark}

gun_mats = build_gun_materials()

# 3. Import the 7 guns as StaticMeshes with combine_meshes=True
GUN_FILES = ["M1911", "M1911Tactical", "MP5", "MP7", "M4A1", "M16", "SCAR"]

for gname in GUN_FILES:
    fbx_file = os.path.join(SRC_DIR, f"{gname}.fbx")
    dest_name = f"SM_Gun_{gname}"
    asset_path = f"{MESH_DIR}/{dest_name}"

    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    opts.static_mesh_import_data.set_editor_property("combine_meshes", True)
    opts.static_mesh_import_data.set_editor_property("convert_scene", True)
    opts.static_mesh_import_data.set_editor_property("auto_generate_collision", False)

    task = unreal.AssetImportTask()
    task.filename = fbx_file
    task.destination_path = MESH_DIR
    task.destination_name = dest_name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = opts
    asset_tools.import_asset_tasks([task])

    sm = unreal.load_asset(asset_path)
    if sm:
        bb = sm.get_bounds()
        static_mats = sm.get_editor_property("static_materials")
        slot_names = []
        new_mats = []
        for idx, smat in enumerate(static_mats):
            sname = str(smat.material_slot_name)
            slot_names.append(sname)
            chosen_mi = gun_mats.get(sname, gun_mats["default"])
            smat.material_interface = chosen_mi
            new_mats.append(smat)
        sm.set_editor_property("static_materials", new_mats)
        eal.save_loaded_asset(sm)
        log(f"Imported {dest_name}: origin={bb.origin} extent={bb.box_extent} slots={slot_names}")
