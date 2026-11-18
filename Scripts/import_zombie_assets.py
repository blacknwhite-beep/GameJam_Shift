"""
Imports the extracted Zombie FBX base mesh, Texture Sets 1 & 2, and all 12 FBX animations
into /Game/Dev_Blueprint/Characters/Zombie/, builds PBR materials (MI_Zombie_Slow_Skin,
MI_Zombie_Fast_Skin), and updates BP_Zombie_Slow & BP_Zombie_Fast.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nopause -unattended -NullRHI -nosound
"""
import os
import unreal

SRC_ROOT = r"D:\transfer\projects\models\Zombie\Extracted"
DEST_ROOT = "/Game/Dev_Blueprint/Characters/Zombie"
TEX_DIR = f"{DEST_ROOT}/Textures"
MAT_DIR = f"{DEST_ROOT}/Materials"
ANIM_DIR = f"{DEST_ROOT}/Animations"
ENEMY_DIR = "/Game/Dev_Blueprint/Blueprints/Enemies"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Bypass UE5.7 Interchange Slate notifications during headless commandlet import
for _cvar in ("Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.Texture 0"):
    unreal.SystemLibrary.execute_console_command(None, _cvar)


def log(msg):
    unreal.log_warning(f"[ZOMBIE_IMPORT] {msg}")


def import_skeletal_mesh():
    fbx_path = os.path.join(SRC_ROOT, "BaseMesh", "Base mesh", "Base mesh fbx.fbx")
    mesh_path = f"{DEST_ROOT}/SK_Zombie"
    if eal.does_asset_exist(mesh_path):
        sk = unreal.load_asset(mesh_path)
        skel = unreal.load_asset(f"{DEST_ROOT}/SK_Zombie_Skeleton")
        if sk and skel:
            bb = sk.get_bounds()
            log(f"Loaded cached SK_Zombie: origin={bb.origin} extent={bb.box_extent}")
            return sk, skel

    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    opts.skeletal_mesh_import_data.set_editor_property("import_morph_targets", True)
    opts.skeletal_mesh_import_data.set_editor_property("convert_scene", True)

    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = DEST_ROOT
    task.destination_name = "SK_Zombie"
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = opts
    asset_tools.import_asset_tasks([task])

    sk = unreal.load_asset(mesh_path)
    skel = sk.get_editor_property("skeleton") if sk else None
    if sk:
        bb = sk.get_bounds()
        mats = sk.get_editor_property("materials")
        log(f"Imported SK_Zombie: origin={bb.origin} extent={bb.box_extent}, material_slots={len(mats)}, skeleton={skel}")
        eal.save_loaded_asset(sk)
    if skel:
        eal.save_loaded_asset(skel)
    return sk, skel


def import_texture(src_file, dest_name, is_normal=False, is_srgb=True):
    asset_path = f"{TEX_DIR}/{dest_name}"
    if eal.does_asset_exist(asset_path):
        return unreal.load_asset(asset_path)

    if not os.path.exists(src_file):
        log(f"WARNING: Missing texture {src_file}")
        return None

    task = unreal.AssetImportTask()
    task.filename = src_file
    task.destination_path = TEX_DIR
    task.destination_name = dest_name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.factory = unreal.TextureFactory()
    asset_tools.import_asset_tasks([task])

    tex = unreal.load_asset(asset_path)
    if tex:
        if is_normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        elif not is_srgb:
            tex.set_editor_property("srgb", False)
        eal.save_loaded_asset(tex)
    return tex


def build_zombie_material_master():
    mat_path = f"{MAT_DIR}/M_ZombieMaster"
    if eal.does_asset_exist(mat_path):
        mat = unreal.load_asset(mat_path)
        if mat and not mat.get_editor_property("used_with_skeletal_mesh"):
            mat.set_editor_property("used_with_skeletal_mesh", True)
            mel.recompile_material(mat)
            eal.save_loaded_asset(mat)
            log(f"Enabled used_with_skeletal_mesh=True and recompiled {mat_path}")
        return mat

    mat = asset_tools.create_asset("M_ZombieMaster", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)

    def tex_param(name, y, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -550, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("sampler_type", sampler_type)
        return node

    albedo = tex_param("Albedo", -320, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = tex_param("Normal", -80, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    metallic = tex_param("Metallic", 160, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    gloss = tex_param("Gloss", 380, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    ao = tex_param("AO", 600, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    emissive = tex_param("Emission", 820, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

    # Roughness = 1.0 - Gloss
    one_minus = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -240, 380)
    mel.connect_material_expressions(gloss, "RGB", one_minus, "")

    mel.connect_material_property(albedo, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(metallic, "R", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(one_minus, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(emissive, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"Built {mat_path}")
    return mat


def make_zombie_mi(master, name, tex_dict):
    mi_path = f"{MAT_DIR}/{name}"
    if eal.does_asset_exist(mi_path):
        mi = unreal.load_asset(mi_path)
    else:
        mi = asset_tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    for param, tex in tex_dict.items():
        if tex:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    eal.save_loaded_asset(mi)
    log(f"Saved {mi_path}")
    return mi


def import_animations(skel):
    anim_folder = os.path.join(SRC_ROOT, "Anim", "animation fbx")
    imported = {}
    for fname in sorted(os.listdir(anim_folder)):
        if not fname.lower().endswith(".fbx"):
            continue
        clip_name = os.path.splitext(fname)[0].replace("zombie@", "Anim_Zombie_")
        dest_path = f"{ANIM_DIR}/{clip_name}"
        if eal.does_asset_exist(dest_path):
            existing = unreal.load_asset(dest_path)
            if isinstance(existing, unreal.AnimSequence):
                seq = existing
            else:
                eal.delete_asset(dest_path)
                seq = None
        else:
            seq = None

        if not seq:
            opts = unreal.FbxImportUI()
            opts.set_editor_property("automated_import_should_detect_type", False)
            opts.set_editor_property("import_mesh", False)
            opts.set_editor_property("import_textures", False)
            opts.set_editor_property("import_materials", False)
            opts.set_editor_property("import_animations", True)
            opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
            opts.set_editor_property("skeleton", skel)
            opts.anim_sequence_import_data.set_editor_property("convert_scene", True)
            opts.anim_sequence_import_data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)

            task = unreal.AssetImportTask()
            task.filename = os.path.join(anim_folder, fname)
            task.destination_path = ANIM_DIR
            task.destination_name = clip_name
            task.automated = True
            task.save = True
            task.replace_existing = True
            task.factory = unreal.FbxFactory()
            task.options = opts
            asset_tools.import_asset_tasks([task])
            seq = unreal.load_asset(dest_path)
            if seq:
                eal.save_loaded_asset(seq)

        if seq:
            length = seq.get_play_length() if hasattr(seq, "get_play_length") else 0.0
            imported[clip_name] = seq
            log(f"Anim {clip_name}: length={length:.3f}s")
    return imported


def main():
    for d in (DEST_ROOT, TEX_DIR, MAT_DIR, ANIM_DIR):
        eal.make_directory(d)

    sk, skel = import_skeletal_mesh()
    if not sk or not skel:
        log("ERROR: Failed to import SK_Zombie or Skeleton!")
        return

    # Texture Set 1 (folder Tex1/Texture set 1 -> 2_*) for Slow Zombie
    t_slow = {
        "Albedo": import_texture(os.path.join(SRC_ROOT, "Tex1", "Texture set 1", "2_Albedo.tga"), "T_ZombieSet1_Albedo", is_srgb=True),
        "Normal": import_texture(os.path.join(SRC_ROOT, "Tex1", "Texture set 1", "2_Normal.tga"), "T_ZombieSet1_Normal", is_normal=True, is_srgb=False),
        "Metallic": import_texture(os.path.join(SRC_ROOT, "Tex1", "Texture set 1", "2_Metallic.tga"), "T_ZombieSet1_Metallic", is_srgb=False),
        "Gloss": import_texture(os.path.join(SRC_ROOT, "Tex1", "Texture set 1", "2_gloss.tga"), "T_ZombieSet1_Gloss", is_srgb=False),
        "AO": import_texture(os.path.join(SRC_ROOT, "Tex1", "Texture set 1", "2_Occlusion.tga"), "T_ZombieSet1_AO", is_srgb=False),
        "Emission": import_texture(os.path.join(SRC_ROOT, "Tex1", "Texture set 1", "2_Emission.tga"), "T_ZombieSet1_Emission", is_srgb=True),
    }

    # Texture Set 2 (folder Tex2/Texture set 2 -> 1_*) for Fast Zombie
    t_fast = {
        "Albedo": import_texture(os.path.join(SRC_ROOT, "Tex2", "Texture set 2", "1_Albedo.tga"), "T_ZombieSet2_Albedo", is_srgb=True),
        "Normal": import_texture(os.path.join(SRC_ROOT, "Tex2", "Texture set 2", "1_Normal.tga"), "T_ZombieSet2_Normal", is_normal=True, is_srgb=False),
        "Metallic": import_texture(os.path.join(SRC_ROOT, "Tex2", "Texture set 2", "1_Metallic.tga"), "T_ZombieSet2_Metallic", is_srgb=False),
        "Gloss": import_texture(os.path.join(SRC_ROOT, "Tex2", "Texture set 2", "1_gloss.tga"), "T_ZombieSet2_Gloss", is_srgb=False),
        "AO": import_texture(os.path.join(SRC_ROOT, "Tex2", "Texture set 2", "1_Occlusion.tga"), "T_ZombieSet2_AO", is_srgb=False),
        "Emission": import_texture(os.path.join(SRC_ROOT, "Tex2", "Texture set 2", "1_Emission.tga"), "T_ZombieSet2_Emission", is_srgb=True),
    }

    master = build_zombie_material_master()
    mi_slow = make_zombie_mi(master, "MI_Zombie_Slow_Skin", t_slow)
    mi_fast = make_zombie_mi(master, "MI_Zombie_Fast_Skin", t_fast)

    # Apply default material to SK_Zombie slots
    num_slots = len(sk.get_editor_property("materials"))
    new_mats = [unreal.SkeletalMaterial(material_interface=mi_slow) for _ in range(max(1, num_slots))]
    sk.set_editor_property("materials", new_mats)
    eal.save_loaded_asset(sk)

    anims = import_animations(skel)
    log(f"DONE: Imported SK_Zombie, {len(anims)} animations, MI_Zombie_Slow_Skin & MI_Zombie_Fast_Skin")


main()
