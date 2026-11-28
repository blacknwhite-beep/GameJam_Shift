"""
Upgrades the pond in BOTH /Game/Dev_Blueprint/Maps/Lvl_Farm and /Game/Dev_Blueprint/Maps/Lvl_Combat
from a flat opaque blue disc into real animated translucent refractive water with swimmable
APhysicsVolume (bWaterVolume=True) and subtle underwater post-process tint.
Also enables swimming (bCanSwim=True) on BP_ShooterCharacter.
"""
import unreal

ENV_DIR = "/Game/Dev_Blueprint/Maps/Farm_Environment"
FARM_LEVEL = "/Game/Dev_Blueprint/Maps/Lvl_Farm"
COMBAT_LEVEL = "/Game/Dev_Blueprint/Maps/Lvl_Combat"
CHAR_BP_PATH = "/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"

POND_X, POND_Y = -2600.0, 2600.0
POND_R = 950.0
WATER_Z = -55.0
POND_BOTTOM_Z = -225.0

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning(f"[WATER] {msg}")


def build_pond_water_master_material():
    mat_path = f"{ENV_DIR}/M_PondWater"
    if eal.does_asset_exist(mat_path):
        eal.delete_asset(mat_path)

    mat = asset_tools.create_asset("M_PondWater", ENV_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    mat.set_editor_property("two_sided", True)

    # Parameters
    shallow_col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, -320)
    shallow_col.set_editor_property("parameter_name", "ShallowColor")
    shallow_col.set_editor_property("default_value", unreal.LinearColor(0.14, 0.52, 0.56, 1.0))

    deep_col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, -140)
    deep_col.set_editor_property("parameter_name", "DeepColor")
    deep_col.set_editor_property("default_value", unreal.LinearColor(0.03, 0.17, 0.30, 1.0))

    # DepthFade for shallow -> deep color transition
    depth_fade_col = mel.create_material_expression(mat, unreal.MaterialExpressionDepthFade, -900, 40)
    depth_fade_col.set_editor_property("fade_distance_default", 115.0)
    depth_fade_col.set_editor_property("opacity_default", 1.0)

    color_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -520, -220)
    mel.connect_material_expressions(shallow_col, "", color_lerp, "A")
    mel.connect_material_expressions(deep_col, "", color_lerp, "B")
    mel.connect_material_expressions(depth_fade_col, "", color_lerp, "Alpha")
    mel.connect_material_property(color_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Opacity: Fresnel (0.42 straight down -> 0.90 grazing) * soft shoreline DepthFade (26 cm)
    fresnel = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -900, 220)
    fresnel.set_editor_property("exponent", 3.5)
    fresnel.set_editor_property("base_reflect_fraction", 0.08)

    opac_lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -620, 220)
    opac_lerp.set_editor_property("const_a", 0.42)
    opac_lerp.set_editor_property("const_b", 0.90)
    mel.connect_material_expressions(fresnel, "", opac_lerp, "Alpha")

    edge_fade = mel.create_material_expression(mat, unreal.MaterialExpressionDepthFade, -900, 380)
    edge_fade.set_editor_property("fade_distance_default", 26.0)
    edge_fade.set_editor_property("opacity_default", 1.0)

    opac_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -340, 260)
    mel.connect_material_expressions(opac_lerp, "", opac_mul, "A")
    mel.connect_material_expressions(edge_fade, "", opac_mul, "B")
    mel.connect_material_property(opac_mul, "", unreal.MaterialProperty.MP_OPACITY)

    # Specular, Roughness, Metallic, Refraction
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -340, -40)
    rough.set_editor_property("r", 0.04)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -340, 40)
    spec.set_editor_property("r", 0.92)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    refract = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -340, 120)
    refract.set_editor_property("r", 1.333)
    mel.connect_material_property(refract, "", unreal.MaterialProperty.MP_REFRACTION)

    # Animated dual-layer ripple normal using Time + WorldPosition -> Sin/Cos wave slopes
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1450, 620)
    time_node = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1450, 820)

    mask_x = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1220, 560)
    mask_x.set_editor_property("r", True)
    mask_x.set_editor_property("g", False)
    mask_x.set_editor_property("b", False)
    mel.connect_material_expressions(wp, "", mask_x, "")

    mask_y = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1220, 680)
    mask_y.set_editor_property("r", False)
    mask_y.set_editor_property("g", True)
    mask_y.set_editor_property("b", False)
    mel.connect_material_expressions(wp, "", mask_y, "")

    # Wave 1: sin(x * 0.035 + time * 1.4) * 0.14
    x_freq = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1020, 560)
    x_freq.set_editor_property("const_b", 0.035)
    mel.connect_material_expressions(mask_x, "", x_freq, "A")

    t1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1020, 780)
    t1.set_editor_property("const_b", 1.4)
    mel.connect_material_expressions(time_node, "", t1, "A")

    add1 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -840, 580)
    mel.connect_material_expressions(x_freq, "", add1, "A")
    mel.connect_material_expressions(t1, "", add1, "B")

    sin1 = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -680, 580)
    mel.connect_material_expressions(add1, "", sin1, "")

    nx = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -520, 580)
    nx.set_editor_property("const_b", 0.14)
    mel.connect_material_expressions(sin1, "", nx, "A")

    # Wave 2: sin(y * 0.042 - time * 1.1) * 0.14
    y_freq = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1020, 680)
    y_freq.set_editor_property("const_b", 0.042)
    mel.connect_material_expressions(mask_y, "", y_freq, "A")

    t2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1020, 880)
    t2.set_editor_property("const_b", -1.1)
    mel.connect_material_expressions(time_node, "", t2, "A")

    add2 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -840, 700)
    mel.connect_material_expressions(y_freq, "", add2, "A")
    mel.connect_material_expressions(t2, "", add2, "B")

    sin2 = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -680, 700)
    mel.connect_material_expressions(add2, "", sin2, "")

    ny = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -520, 700)
    ny.set_editor_property("const_b", 0.14)
    mel.connect_material_expressions(sin2, "", ny, "A")

    # Assemble tangent-space normal (nx, ny, 1.0)
    append_xy = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -360, 620)
    mel.connect_material_expressions(nx, "", append_xy, "A")
    mel.connect_material_expressions(ny, "", append_xy, "B")

    one_z = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -360, 740)
    one_z.set_editor_property("r", 1.0)

    append_xyz = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -200, 640)
    mel.connect_material_expressions(append_xy, "", append_xyz, "A")
    mel.connect_material_expressions(one_z, "", append_xyz, "B")

    norm = mel.create_material_expression(mat, unreal.MaterialExpressionNormalize, -40, 640)
    mel.connect_material_expressions(append_xyz, "", norm, "")
    mel.connect_material_property(norm, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log(f"Created translucent animated water master material: {mat_path}")
    return mat


def make_water_instance(parent, name, shallow_rgb, deep_rgb):
    path = f"{ENV_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, ENV_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(mi, "ShallowColor", unreal.LinearColor(*shallow_rgb, 1.0))
    mel.set_material_instance_vector_parameter_value(mi, "DeepColor", unreal.LinearColor(*deep_rgb, 1.0))
    eal.save_loaded_asset(mi)
    log(f"Saved water material instance: {path}")
    return mi


def enable_swimming_on_character():
    bp = unreal.load_asset(CHAR_BP_PATH)
    if not bp:
        log(f"WARNING: Could not load {CHAR_BP_PATH}")
        return
    gen_cls = bp.generated_class()
    cdo = unreal.get_default_object(gen_cls)
    move = cdo.get_editor_property("character_movement")
    if move:
        nav_props = move.get_editor_property("nav_agent_props")
        nav_props.set_editor_property("can_swim", True)
        move.set_editor_property("nav_agent_props", nav_props)
        move.set_editor_property("buoyancy", 1.05)
        move.set_editor_property("max_swim_speed", 300.0)
        move.set_editor_property("braking_deceleration_swimming", 150.0)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        eal.save_loaded_asset(bp)
        log("Enabled swimming (can_swim=True, buoyancy=1.05) on BP_ShooterCharacter")


def upgrade_pond_in_level(level_path, water_mat, is_farm=True):
    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not level_sub.load_level(level_path):
        log(f"ERROR: Could not load level {level_path}")
        return

    updated_water = 0
    existing_water_vol = None
    existing_underwater_pp = None

    for actor in actor_sub.get_all_level_actors():
        label = actor.get_actor_label()
        if label in ("Pond_Water", "WaterTrough_Water"):
            if isinstance(actor, unreal.StaticMeshActor):
                comp = actor.static_mesh_component
                comp.set_material(0, water_mat)
                comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                comp.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
                comp.set_cast_shadow(False)
                updated_water += 1
                log(f"  [{level_path}] Upgraded {label} -> {water_mat.get_name()} (NoCollision)")
        elif label == "Pond_WaterVolume":
            existing_water_vol = actor
        elif label == "Pond_UnderwaterPP":
            existing_underwater_pp = actor

    # Remove old volumes if re-running so placement is idempotent
    if existing_water_vol:
        actor_sub.destroy_actor(existing_water_vol)
    if existing_underwater_pp:
        actor_sub.destroy_actor(existing_underwater_pp)

    # Spawn real swimmable APhysicsVolume (bWaterVolume=True) filling the pond basin up to WATER_Z (-55)
    # Default Brush cube is 200x200x200 cm, so scale=(8.2, 8.2, 1.0) covers ~1640x1640x200 cm!
    # Center Z = WATER_Z - 100.0 = -155.0 so top face sits right at WATER_Z = -55.0 and bottom reaches -255.0
    vol_loc = unreal.Vector(POND_X, POND_Y, WATER_Z - 100.0)
    water_vol = actor_sub.spawn_actor_from_class(unreal.PhysicsVolume, vol_loc, unreal.Rotator(0.0, 0.0, 0.0))
    if water_vol:
        water_vol.set_actor_scale3d(unreal.Vector(8.2, 8.2, 1.0))
        water_vol.set_editor_property("water_volume", True)
        water_vol.set_editor_property("fluid_friction", 0.35)
        water_vol.set_editor_property("terminal_velocity", 600.0)
        water_vol.set_editor_property("priority", 10)
        water_vol.set_actor_label("Pond_WaterVolume")
        water_vol.set_folder_path("Farm/Pond" if is_farm else "Combat/Pond")
        if is_farm:
            water_vol.tags = [unreal.Name("FarmWorld")]
        log(f"  [{level_path}] Spawned swimmable Pond_WaterVolume at {vol_loc} (top Z={WATER_Z})")

    # Spawn subtle underwater PostProcessVolume below WATER_Z
    pp_vol = actor_sub.spawn_actor_from_class(unreal.PostProcessVolume, vol_loc, unreal.Rotator(0.0, 0.0, 0.0))
    if pp_vol:
        pp_vol.set_actor_scale3d(unreal.Vector(8.2, 8.2, 1.0))
        pp_vol.set_editor_property("unbound", False)
        pp_vol.set_editor_property("blend_radius", 16.0)
        pp_vol.set_editor_property("priority", 5.0)
        settings = pp_vol.get_editor_property("settings")
        settings.set_editor_property("override_scene_color_tint", True)
        tint = unreal.LinearColor(0.35, 0.72, 0.78, 1.0) if is_farm else unreal.LinearColor(0.20, 0.42, 0.48, 1.0)
        settings.set_editor_property("scene_color_tint", tint)
        settings.set_editor_property("override_vignette_intensity", True)
        settings.set_editor_property("vignette_intensity", 0.55)
        pp_vol.set_editor_property("settings", settings)
        pp_vol.set_actor_label("Pond_UnderwaterPP")
        pp_vol.set_folder_path("Farm/Pond" if is_farm else "Combat/Pond")
        if is_farm:
            pp_vol.tags = [unreal.Name("FarmWorld")]
        log(f"  [{level_path}] Spawned Pond_UnderwaterPP")

    level_sub.save_current_level()
    log(f"Saved {level_path} (updated {updated_water} water surfaces)")


master = build_pond_water_master_material()
mi_farm = make_water_instance(master, "MI_PondWater_Farm", shallow_rgb=(0.14, 0.54, 0.58), deep_rgb=(0.03, 0.16, 0.30))
mi_combat = make_water_instance(master, "MI_PondWater_Combat", shallow_rgb=(0.05, 0.22, 0.25), deep_rgb=(0.012, 0.055, 0.10))

enable_swimming_on_character()
upgrade_pond_in_level(FARM_LEVEL, mi_farm, is_farm=True)
upgrade_pond_in_level(COMBAT_LEVEL, mi_combat, is_farm=False)

log("ALL PONDS UPGRADED TO REAL SWIMMABLE TRANSLUCENT WATER!")
