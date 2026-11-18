"""
Rebuild M_ZombieMaster with its TextureSampleParameter2D nodes bound to the matching Set 1 textures.

Root cause: the sampler nodes had no texture assigned, so they used /Engine/EngineResources/DefaultTexture
(sRGB Color). The Normal and Linear Color sampler types don't match that texture, so M_ZombieMaster
fails to compile and MI_Zombie_*_Skin fall back to the default material.

Run headless (editor must be closed):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nopause -unattended -NullRHI -nosound
"""
import unreal

MAT_DIR = "/Game/Dev_Blueprint/Characters/Zombie/Materials"
TEX_DIR = "/Game/Dev_Blueprint/Characters/Zombie/Textures"
MAT_PATH = f"{MAT_DIR}/M_ZombieMaster"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning(f"[ZOMBIE_FIX] {msg}")


def load_tex(name):
    tex = unreal.load_asset(f"{TEX_DIR}/{name}")
    if not tex:
        log(f"ERROR: missing {TEX_DIR}/{name}")
    return tex


def main():
    if eal.does_asset_exist(MAT_PATH):
        eal.delete_asset(MAT_PATH)
        log("Deleted old M_ZombieMaster")

    mat = asset_tools.create_asset("M_ZombieMaster", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)

    def tex_param(param, tex_name, y, sampler_type):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -550, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", sampler_type)
        node.set_editor_property("texture", load_tex(tex_name))
        return node

    C = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    N = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    L = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR

    albedo = tex_param("Albedo", "T_ZombieSet1_Albedo", -320, C)
    normal = tex_param("Normal", "T_ZombieSet1_Normal", -80, N)
    metallic = tex_param("Metallic", "T_ZombieSet1_Metallic", 160, L)
    gloss = tex_param("Gloss", "T_ZombieSet1_Gloss", 380, L)
    ao = tex_param("AO", "T_ZombieSet1_AO", 600, L)
    emissive = tex_param("Emission", "T_ZombieSet1_Emission", 820, C)

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
    log("Rebuilt M_ZombieMaster with Set 1 textures bound")

    for mi_name in ("MI_Zombie_Slow_Skin", "MI_Zombie_Fast_Skin"):
        mi = unreal.load_asset(f"{MAT_DIR}/{mi_name}")
        if not mi:
            log(f"ERROR: missing {mi_name}")
            continue
        mi.set_editor_property("parent", mat)
        eal.save_loaded_asset(mi)
        log(f"Re-parented {mi_name} to M_ZombieMaster")

    log("DONE")


main()
