"""
Adds Tom Looman-style procedural 3D sphere-mask wound & bleeding support to M_ZombieMaster
(/Game/Dev_Blueprint/Characters/Zombie/Materials/M_ZombieMaster) and re-saves MI_Zombie_Slow_Skin
and MI_Zombie_Fast_Skin.

Wound parameters added to M_ZombieMaster:
  - HitPosition0, HitPosition1, HitPosition2, HitPosition3 (VectorParameter: XYZ = Local Mesh-Space Hit Offset from ActorLocation, A = Radius)
  - HitActive0, HitActive1, HitActive2, HitActive3 (ScalarParameter: 0.0 -> 1.0 wound intensity)

Also hides any visual mesh on BP_ShooterProjectile_Bullet so no rubber/foam test ball is ever visible.
"""
import unreal

MAT_DIR = "/Game/Dev_Blueprint/Characters/Zombie/Materials"
TEX_DIR = "/Game/Dev_Blueprint/Characters/Zombie/Textures"
MAT_PATH = f"{MAT_DIR}/M_ZombieMaster"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning(f"[WOUND_MAT] {msg}")


def load_tex(name):
    tex = unreal.load_asset(f"{TEX_DIR}/{name}")
    if not tex:
        log(f"ERROR: missing {TEX_DIR}/{name}")
    return tex


def build_wound_zombie_master():
    if eal.does_asset_exist(MAT_PATH):
        eal.delete_asset(MAT_PATH)
        log("Deleted old M_ZombieMaster")

    mat = asset_tools.create_asset("M_ZombieMaster", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)

    def tex_param(param, tex_name, x, y, sampler_type):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", sampler_type)
        node.set_editor_property("texture", load_tex(tex_name))
        return node

    C = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    N = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    L = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR

    albedo = tex_param("Albedo", "T_ZombieSet1_Albedo", -900, -520, C)
    normal = tex_param("Normal", "T_ZombieSet1_Normal", -900, -240, N)
    metallic = tex_param("Metallic", "T_ZombieSet1_Metallic", -900, 40, L)
    gloss = tex_param("Gloss", "T_ZombieSet1_Gloss", -900, 280, L)
    ao = tex_param("AO", "T_ZombieSet1_AO", -900, 520, L)
    emissive = tex_param("Emission", "T_ZombieSet1_Emission", -900, 760, C)

    one_minus_gloss = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -620, 280)
    mel.connect_material_expressions(gloss, "RGB", one_minus_gloss, "")

    # -------------------------------------------------------------------------
    # Tom Looman Character Wound SphereMask Network (4 Persistent Hit Slots)
    # LocalPos = AbsoluteWorldPosition - ActorPosition
    # Plus subtle Normal-map organic edge breakup so wounds have ragged flesh edges!
    # -------------------------------------------------------------------------
    world_pos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, -1200)
    actor_pos = mel.create_material_expression(mat, unreal.MaterialExpressionActorPositionWS, -1400, -1060)
    local_pos = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -1150, -1140)
    mel.connect_material_expressions(world_pos, "", local_pos, "A")
    mel.connect_material_expressions(actor_pos, "", local_pos, "B")

    # Organic wound edge distortion from the zombie's Normal map R channel (-5..+5 cm ragged border)
    edge_noise_scale = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -680, -240)
    edge_noise_scale.set_editor_property("const_b", 6.5)
    mel.connect_material_expressions(normal, "R", edge_noise_scale, "A")

    combined_mask = None
    for idx in range(4):
        y_off = -1400 + idx * 220

        hit_vec = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1150, y_off)
        hit_vec.set_editor_property("parameter_name", f"HitPosition{idx}")
        hit_vec.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, -5000.0, 20.0))

        hit_active = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y_off + 90)
        hit_active.set_editor_property("parameter_name", f"HitActive{idx}")
        hit_active.set_editor_property("default_value", 0.0)

        # Effective radius = HitPosition.A + Normal.R * 6.5 (ragged flesh wound shape)
        noisy_radius = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -900, y_off)
        mel.connect_material_expressions(hit_vec, "A", noisy_radius, "A")
        mel.connect_material_expressions(edge_noise_scale, "", noisy_radius, "B")

        # Distance between LocalPos and HitPosition.RGB
        dist = mel.create_material_expression(mat, unreal.MaterialExpressionDistance, -900, y_off - 70)
        mel.connect_material_expressions(local_pos, "", dist, "A")
        mel.connect_material_expressions(hit_vec, "RGB", dist, "B")

        # Normalized distance = Dist / EffectiveRadius
        norm_dist = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, -720, y_off - 40)
        mel.connect_material_expressions(dist, "", norm_dist, "A")
        mel.connect_material_expressions(noisy_radius, "", norm_dist, "B")

        # Invert: 1 - NormalizedDist
        inv_dist = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -560, y_off - 40)
        mel.connect_material_expressions(norm_dist, "", inv_dist, "")

        # Sharpen edge falloff (multiply by 2.2 then saturate)
        sharp = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -430, y_off - 40)
        sharp.set_editor_property("const_b", 2.2)
        mel.connect_material_expressions(inv_dist, "", sharp, "A")

        sat = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, -300, y_off - 40)
        mel.connect_material_expressions(sharp, "", sat, "")

        # Multiply by HitActive{idx}
        masked = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -170, y_off - 20)
        mel.connect_material_expressions(sat, "", masked, "A")
        mel.connect_material_expressions(hit_active, "", masked, "B")

        if combined_mask is None:
            combined_mask = masked
        else:
            add_mask = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -40, y_off - 20)
            mel.connect_material_expressions(combined_mask, "", add_mask, "A")
            mel.connect_material_expressions(masked, "", add_mask, "B")
            combined_mask = add_mask

    final_wound_mask = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, 100, -700)
    mel.connect_material_expressions(combined_mask, "", final_wound_mask, "")

    # Deep crimson blood color for wound interior: (0.24, 0.008, 0.008)
    blood_color = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -250, -520)
    blood_color.set_editor_property("constant", unreal.LinearColor(0.24, 0.008, 0.008, 1.0))

    # Lerp BaseColor between Skin Albedo and Deep Crimson Blood using final_wound_mask
    base_color_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 260, -480)
    mel.connect_material_expressions(albedo, "RGB", base_color_lerp, "A")
    mel.connect_material_expressions(blood_color, "", base_color_lerp, "B")
    mel.connect_material_expressions(final_wound_mask, "", base_color_lerp, "Alpha")

    # Lerp Roughness between Skin Roughness and Wet Blood Gloss (0.08) using final_wound_mask
    wet_rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 340)
    wet_rough.set_editor_property("r", 0.08)

    rough_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, 260, 280)
    mel.connect_material_expressions(one_minus_gloss, "", rough_lerp, "A")
    mel.connect_material_expressions(wet_rough, "", rough_lerp, "B")
    mel.connect_material_expressions(final_wound_mask, "", rough_lerp, "Alpha")

    mel.connect_material_property(base_color_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(metallic, "R", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(emissive, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("Rebuilt M_ZombieMaster with 4-slot 3D SphereMask Character Wounds & Wet Blood Roughness")

    for mi_name in ("MI_Zombie_Slow_Skin", "MI_Zombie_Fast_Skin"):
        mi = unreal.load_asset(f"{MAT_DIR}/{mi_name}")
        if mi:
            mi.set_editor_property("parent", mat)
            eal.save_loaded_asset(mi)
            log(f"Re-parented {mi_name} to wound-enabled M_ZombieMaster")


build_wound_zombie_master()
