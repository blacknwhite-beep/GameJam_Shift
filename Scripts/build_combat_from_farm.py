"""
Duplicates Lvl_Farm into Lvl_Combat so the Zombie/Shooter world is an exact dark,
moody nightmare mirror of the Farm world (including Cozy Nature trees, bushes,
flowers, rocks, Barn, Silo, Pond, Field, Fences, Paths), with the original dark
moody Combat lighting, horror ambience, NavMeshBoundsVolume, PlayerStarts,
BP_ShooterGameMode, and 10 difficulty-ranked Zombie Spawners (BP_Zombie_Slow / BP_Zombie_Fast).

Run via:
  UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript=<this file> -unattended -nop4 -nosplash
"""
import unreal

FARM_LEVEL = "/Game/Dev_Blueprint/Maps/Lvl_Farm"
COMBAT_LEVEL = "/Game/Dev_Blueprint/Maps/Lvl_Combat"
ENEMY_DIR = "/Game/Dev_Blueprint/Blueprints/Enemies"
AUDIO_DIR = "/Game/Dev_Blueprint/Audio"
FARM_TAG = unreal.Name("FarmWorld")

eal = unreal.EditorAssetLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) or unreal.EditorLevelLibrary


def log(msg):
    unreal.log_warning(f"[COMBAT_FARM] {msg}")


def build_combat_atmosphere_and_audio():
    f = "Combat/Atmosphere"
    moon = actor_sub.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 1500.0),
        unreal.Rotator(roll=0.0, pitch=-28.0, yaw=125.0),
    )
    mlc = moon.light_component
    mlc.set_mobility(unreal.ComponentMobility.MOVABLE)
    mlc.set_editor_property("intensity", 0.45)
    mlc.set_editor_property("light_color", unreal.Color(r=95, g=120, b=175, a=255))
    moon.set_actor_label("Combat_OvercastMoon")
    moon.set_folder_path(f)

    sky = actor_sub.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 1400.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    slc = sky.light_component
    slc.set_mobility(unreal.ComponentMobility.MOVABLE)
    slc.set_editor_property("real_time_capture", True)
    slc.set_editor_property("intensity", 0.16)
    slc.set_editor_property("light_color", unreal.Color(r=45, g=58, b=75, a=255))
    sky.set_actor_label("Combat_DarkSkyLight")
    sky.set_folder_path(f)

    atmo = actor_sub.spawn_actor_from_class(
        unreal.SkyAtmosphere,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    atmo.set_actor_label("Combat_SkyAtmosphere")
    atmo.set_folder_path(f)

    fog = actor_sub.spawn_actor_from_class(
        unreal.ExponentialHeightFog,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.065)
    fc.set_editor_property("fog_height_falloff", 0.25)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.025, 0.035, 0.03, 1.0))
    fog.set_actor_label("Combat_SmokeFog")
    fog.set_folder_path(f)

    ppv = actor_sub.spawn_actor_from_class(
        unreal.PostProcessVolume,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    ppv.set_editor_property("unbound", True)
    settings = ppv.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 1.0)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 1.0)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", -0.85)
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.65)
    ppv.set_editor_property("settings", settings)
    ppv.set_actor_label("Combat_DarkHorrorPostProcess")
    ppv.set_folder_path(f)

    amb_sound = unreal.load_asset(f"{AUDIO_DIR}/SFX_ZombieAmbience")
    if amb_sound:
        amb = actor_sub.spawn_actor_from_class(
            unreal.AmbientSound,
            unreal.Vector(0.0, 0.0, 300.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        ac = amb.get_component_by_class(unreal.AudioComponent)
        ac.set_editor_property("sound", amb_sound)
        ac.set_editor_property("override_attenuation", True)
        att = ac.get_editor_property("attenuation_overrides")
        att.set_editor_property("attenuation_shape_extents", unreal.Vector(6000.0, 0.0, 0.0))
        att.set_editor_property("falloff_distance", 5000.0)
        ac.set_editor_property("attenuation_overrides", att)
        amb.set_actor_label("Combat_ZombieOutbreakAmbience")
        amb.set_folder_path("Combat/Audio")


def ensure_fast_zombie_blueprint_ready():
    mat_dir = "/Game/Dev_Blueprint/Characters/Zombie/Materials"
    tex_dir = "/Game/Dev_Blueprint/Characters/Zombie/Textures"
    anim_dir = "/Game/Dev_Blueprint/Characters/Zombie/Animations"

    mi_fast = unreal.load_asset(f"{mat_dir}/MI_Zombie_Fast_Skin")
    if mi_fast:
        for param, suffix in (
            ("Albedo", "Albedo"),
            ("Normal", "Normal"),
            ("Metallic", "Metallic"),
            ("Gloss", "Gloss"),
            ("AO", "AO"),
            ("Emission", "Emission"),
        ):
            tex = unreal.load_asset(f"{tex_dir}/T_ZombieSet2_{suffix}")
            if tex:
                unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi_fast, param, tex)
        eal.save_loaded_asset(mi_fast)

    bp_fast = unreal.load_asset(f"{ENEMY_DIR}/BP_Zombie_Fast")
    if bp_fast:
        cdo = unreal.get_default_object(bp_fast.generated_class())
        cdo.set_editor_property("CurrentHP", 75.0)
        cdo.set_editor_property("ZombieMoveSpeed", 380.0)
        cdo.set_editor_property("ReferenceAnimSpeed", 380.0)
        cdo.set_editor_property("bUseRunAnim", True)
        cdo.set_editor_property("MeleeDamage", 12.0)
        cdo.set_editor_property("MeleeRange", 155.0)
        cdo.set_editor_property("AttackCooldown", 0.75)
        if mi_fast:
            cdo.set_editor_property("ZombieSkinMaterial", mi_fast)
        for prop, anim_name in (
            ("IdleAnim", "Anim_Zombie_idle2"),
            ("WalkAnim", "Anim_Zombie_walk"),
            ("RunAnim", "Anim_Zombie_run"),
            ("AttackAnim", "Anim_Zombie_atack2"),
            ("DeathAnim", "Anim_Zombie_death2"),
        ):
            anim = unreal.load_asset(f"{anim_dir}/{anim_name}")
            if anim:
                cdo.set_editor_property(prop, anim)
        mesh_comp = cdo.get_editor_property("Mesh")
        if mesh_comp and mi_fast:
            count = max(1, mesh_comp.get_num_materials())
            mesh_comp.set_editor_property("override_materials", [mi_fast] * count)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp_fast)
        eal.save_loaded_asset(bp_fast)
        log("Verified BP_Zombie_Fast (Speed=380, bUseRunAnim=True, T_ZombieSet2 skin, full dismemberment & head look-at)")


def place_combat_spawners_nav_and_starts():
    ensure_fast_zombie_blueprint_ready()

    # Large NavMesh covering the entire flat Farm valley (Barn, Field, Paths, Meadows, Pond bank)
    nav = actor_sub.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume,
        unreal.Vector(-400.0, -800.0, 200.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if nav:
        nav.set_actor_scale3d(unreal.Vector(42.0, 46.0, 8.0))
        nav.set_actor_label("Combat_NavMeshBounds")
        nav.set_folder_path("Combat/AI")

    slow_cls = unreal.load_object(None, f"{ENEMY_DIR}/BP_Zombie_Slow.BP_Zombie_Slow_C")
    fast_cls = unreal.load_object(None, f"{ENEMY_DIR}/BP_Zombie_Fast.BP_Zombie_Fast_C")
    spawner_cls = unreal.load_class(None, "/Script/GameJam_Shift_2026.ShooterNPCSpawner")

    if spawner_cls and slow_cls and fast_cls:
        spawns = [
            # Immediate central road & yard spawners (mix of Fast Sprinters & Slow Shamblers in clear sight of (0,0))
            (-920.0, -320.0, 20.0, fast_cls, "ZombieSpawner_CenterWest_Fast", 1.0),
            (-1350.0, 0.0, 0.0, slow_cls, "ZombieSpawner_MainPath_Slow", 1.2),
            (-700.0, -980.0, 55.0, fast_cls, "ZombieSpawner_BarnPath_Fast", 1.5),
            (-520.0, 920.0, -60.0, slow_cls, "ZombieSpawner_NorthYard_Slow", 1.8),
            (320.0, -920.0, 110.0, fast_cls, "ZombieSpawner_FieldGate_Fast", 2.0),
            # Outer perimeter wave spawners
            (-1150.0, -1850.0, 50.0, slow_cls, "ZombieSpawner_BarnWest_Slow", 2.2),
            (-350.0, -1950.0, 80.0, fast_cls, "ZombieSpawner_BarnFront_Fast", 2.5),
            (-1450.0, 1250.0, -40.0, fast_cls, "ZombieSpawner_PondPath_Fast", 2.8),
            (1450.0, -850.0, 150.0, slow_cls, "ZombieSpawner_FieldSE_Slow", 3.0),
            (1450.0, 850.0, -150.0, slow_cls, "ZombieSpawner_FieldNE_Slow", 3.2),
        ]
        for unlock_order, (x, y, yaw, npc_cls, label, delay) in enumerate(spawns):
            sp = actor_sub.spawn_actor_from_class(
                spawner_cls,
                unreal.Vector(x, y, 5.0),
                unreal.Rotator(0.0, yaw, 0.0),
            )
            sp.set_editor_property("NPCClass", npc_cls)
            sp.set_editor_property("InitialSpawnDelay", delay)
            sp.set_editor_property("SpawnCount", 8)
            sp.set_editor_property("RespawnDelay", 4.0)
            sp.set_editor_property("UnlockOrder", unlock_order)
            sp.set_actor_label(label)
            sp.set_folder_path("Combat/Zombies")
        log(f"Placed {len(spawns)} Zombie Spawners (5 Fast + 5 Slow) around the dark farm valley")
    else:
        log(f"WARNING: Missing spawner/zombie classes: spawner={spawner_cls}, slow={slow_cls}, fast={fast_cls}")

    for i, (px, py, tag) in enumerate([(0.0, -250.0, "RED"), (0.0, 250.0, "BLUE")]):
        ps = actor_sub.spawn_actor_from_class(
            unreal.PlayerStart,
            unreal.Vector(px, py, 95.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        ps.tags = [unreal.Name(tag)]
        ps.set_actor_label(f"PlayerStart_{i + 1}")
        ps.set_folder_path("Combat/Spawns")


def main():
    if not eal.does_asset_exist(FARM_LEVEL):
        raise RuntimeError(f"Source farm level does not exist: {FARM_LEVEL}")

    # Load Lvl_Farm directly (automatically unloads Lvl_Combat), transform in memory, and Save-As Lvl_Combat
    world = unreal.EditorLoadingAndSavingUtils.load_map(FARM_LEVEL)
    if not world:
        raise RuntimeError(f"Failed to load source farm map: {FARM_LEVEL}")
    log(f"Loaded {FARM_LEVEL} cleanly; converting in-memory copy into {COMBAT_LEVEL}")

    # Assign Shooter GameMode on WorldSettings
    gm_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode")
    if gm_bp:
        ws = world.get_world_settings()
        ws.set_editor_property("default_game_mode", gm_bp.generated_class())
        log("Assigned BP_ShooterGameMode to Lvl_Combat WorldSettings")

    # Remove Farm-only lighting, sky, fog, clouds, and daytime birds ambience
    removed = 0
    for actor in list(actor_sub.get_all_level_actors()):
        if not actor:
            continue
        label = actor.get_actor_label()
        if FARM_TAG in actor.tags or label.startswith("Farm_Ambience") or isinstance(
            actor,
            (
                unreal.DirectionalLight,
                unreal.SkyLight,
                unreal.SkyAtmosphere,
                unreal.ExponentialHeightFog,
                unreal.VolumetricCloud,
                unreal.AmbientSound,
            ),
        ):
            actor_sub.destroy_actor(actor)
            removed += 1
    log(f"Removed {removed} daytime farm lighting/ambience actors from Lvl_Combat")

    build_combat_atmosphere_and_audio()
    place_combat_spawners_nav_and_starts()

    ok_save = unreal.EditorLoadingAndSavingUtils.save_map(world, COMBAT_LEVEL)
    log(f"Saved {COMBAT_LEVEL}: {ok_save}, total actors={len(actor_sub.get_all_level_actors())}")


main()
