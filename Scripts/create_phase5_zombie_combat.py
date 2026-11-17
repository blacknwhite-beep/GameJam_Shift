"""
Builds Phase 5 Zombie-Themed Combat World & Melee Zombie Enemies (Checklist 5.1 - 5.4):
  - /Game/Dev_Blueprint/Blueprints/Enemies/BP_Zombie_Slow (Slow Shambler, Melee)
  - /Game/Dev_Blueprint/Blueprints/Enemies/BP_Zombie_Fast (Fast Sprinter, Melee)
  - /Game/Dev_Blueprint/Maps/Lvl_Combat.umap (at Z = 0, aligned with Lvl_Farm at Z = -20,000)

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nopause -unattended -NullRHI -nosound
"""
import math
import os
import unreal

LEVEL_PATH = "/Game/Dev_Blueprint/Maps/Lvl_Combat"
ENV_DIR = "/Game/Dev_Blueprint/Maps/Combat_Environment"
ENEMY_DIR = "/Game/Dev_Blueprint/Blueprints/Enemies"
AUDIO_DIR = "/Game/Dev_Blueprint/Audio"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) or unreal.EditorLevelLibrary

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")


def log(msg):
    unreal.log_warning(f"[COMBAT] {msg}")


# ---------------------------------------------------------------------------------------------
# Materials & Audio
# ---------------------------------------------------------------------------------------------
def make_combat_base_material():
    path = f"{ENV_DIR}/M_CombatSolid"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = asset_tools.create_asset("M_CombatSolid", ENV_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.2, 0.2, 0.2, 1.0))

    emissive = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 80)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 240)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.85)

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


def make_mi(parent, name, color, roughness=0.85, metallic=0.0, emissive=(0.0, 0.0, 0.0)):
    path = f"{ENV_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, ENV_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
    mel.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(*emissive, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    eal.save_loaded_asset(mi)
    return mi


def import_wav(name, looping=False):
    wav = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", f"{name}.wav")
    path = f"{AUDIO_DIR}/{name}"
    if not eal.does_asset_exist(path) and os.path.exists(wav):
        task = unreal.AssetImportTask()
        task.filename = wav
        task.destination_path = AUDIO_DIR
        task.destination_name = name
        task.automated = True
        task.save = True
        task.replace_existing = True
        asset_tools.import_asset_tasks([task])
    snd = unreal.load_asset(path)
    if snd and looping:
        snd.set_editor_property("looping", True)
        eal.save_loaded_asset(snd)
    return snd


# ---------------------------------------------------------------------------------------------
# Melee Zombie Blueprints (BP_Zombie_Slow & BP_Zombie_Fast)
# ---------------------------------------------------------------------------------------------
def create_zombie_blueprint(bp_name, hp, speed, damage, melee_range, cooldown, scale, skin_mat, attack_snd, template_mesh, template_anim):
    bp_path = f"{ENEMY_DIR}/{bp_name}"
    if eal.does_asset_exist(bp_path):
        eal.delete_asset(bp_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.ZombieEnemy)
    bp = asset_tools.create_asset(bp_name, ENEMY_DIR, unreal.Blueprint, factory)
    if not bp:
        log(f"ERROR creating {bp_path}")
        return None

    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("CurrentHP", hp)
    cdo.set_editor_property("ZombieMoveSpeed", speed)
    cdo.set_editor_property("MeleeDamage", damage)
    cdo.set_editor_property("MeleeRange", melee_range)
    cdo.set_editor_property("AttackCooldown", cooldown)
    cdo.set_editor_property("ZombieSkinMaterial", skin_mat)
    if attack_snd:
        cdo.set_editor_property("AttackGroanSound", attack_snd)

    mesh_comp = cdo.get_editor_property("Mesh")
    if mesh_comp:
        if template_mesh:
            mesh_comp.set_skeletal_mesh(template_mesh)
        if template_anim:
            mesh_comp.set_anim_instance_class(template_anim)
        mesh_comp.set_material(0, skin_mat)
        mesh_comp.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    log(f"Created {bp_path} (HP={hp}, Speed={speed}, MeleeDmg={damage})")
    return bp


# ---------------------------------------------------------------------------------------------
# Level Placement Helpers
# ---------------------------------------------------------------------------------------------
def place(mesh, loc, scale, mat, label, folder, rot=(0.0, 0.0, 0.0), collision=True):
    actor = actor_sub.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*loc),
        unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]),
    )
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    comp.set_material(0, mat)
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def add_point_light(loc, color_rgb, intensity, radius, label, folder):
    pl = actor_sub.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc), unreal.Rotator(0, 0, 0))
    lc = pl.point_light_component
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property("light_color", unreal.Color(r=color_rgb[0], g=color_rgb[1], b=color_rgb[2], a=255))
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    pl.set_actor_label(label)
    pl.set_folder_path(folder)
    return pl


def build_ruined_fortress(m):
    """Aligned directly above Lvl_Farm's Barn at (-700, -3300)!"""
    fx, fy = -700.0, -3300.0
    f = "Combat/RuinedFortress"
    # Cracked foundation slab
    place(CUBE, (fx, fy, 10), (13.0, 10.0, 0.2), m["concrete"], "Fortress_Foundation", f)
    # Broken jagged concrete outer walls
    place(CUBE, (fx - 560, fy, 210), (0.8, 9.2, 4.2), m["concrete_dark"], "Fortress_Wall_W", f)
    place(CUBE, (fx + 560, fy - 120, 160), (0.8, 6.8, 3.2), m["concrete_dark"], "Fortress_Wall_E", f)
    place(CUBE, (fx, fy - 440, 240), (11.2, 0.8, 4.8), m["concrete"], "Fortress_Wall_Back", f)
    # Front breached wall halves with doorway gap
    place(CUBE, (fx - 360, fy + 440, 175), (4.2, 0.8, 3.5), m["concrete_dark"], "Fortress_Front_L", f)
    place(CUBE, (fx + 360, fy + 440, 140), (4.2, 0.8, 2.8), m["concrete_dark"], "Fortress_Front_R", f)
    # Corner quarantine watchtower + Red Emergency Beacon
    tx, ty = fx + 450, fy - 350
    place(CUBE, (tx, ty, 380), (2.4, 2.4, 7.6), m["concrete_dark"], "Fortress_Watchtower", f)
    place(SPHERE, (tx, ty, 780), (0.45, 0.45, 0.45), m["emergency_red"], "Fortress_BeaconMesh", f, collision=False)
    add_point_light((tx, ty, 810), (255, 25, 20), 6500.0, 2600.0, "Fortress_EmergencyLight", f)


def build_arena_cover_and_barricades(m):
    f = "Combat/Obstacles"
    # Quarantine perimeter concrete walls + watch posts around X +-1750, Y +-2050
    for sgn, lbl in ((-1, "West"), (1, "East")):
        place(CUBE, (sgn * 1850, 0, 160), (0.7, 42.0, 3.2), m["concrete_dark"], f"PerimeterWall_{lbl}", f)
    for sgn, lbl in ((-1, "South"), (1, "North")):
        place(CUBE, (0, sgn * 2150, 160), (37.0, 0.7, 3.2), m["concrete_dark"], f"PerimeterWall_{lbl}", f)

    # Broken concrete cover walls inside the combat yard
    walls = [
        (-550, -500, 120, 4.5, 0.6, 2.4, 15.0),
        (600, -450, 105, 4.2, 0.6, 2.1, -20.0),
        (-650, 620, 125, 4.8, 0.6, 2.5, -12.0),
        (580, 580, 110, 4.0, 0.6, 2.2, 22.0),
        (0, 950, 95, 5.2, 0.6, 1.9, 0.0),
        (0, -950, 95, 5.2, 0.6, 1.9, 0.0),
    ]
    for i, (x, y, z, sx, sy, sz, yaw) in enumerate(walls):
        place(CUBE, (x, y, z), (sx, sy, sz), m["concrete"], f"BrokenWall_{i + 1}", f, rot=(0.0, yaw, 0.0))

    # Stacked Sandbag Bunkers
    bunkers = [(-320, 180, 0.0), (340, -180, 90.0), (-1100, 0, 90.0), (1100, 0, 90.0)]
    for i, (bx, by, yaw) in enumerate(bunkers):
        for row_z, cnt in ((18, 3), (48, 2)):
            for k in range(cnt):
                off = (k - (cnt - 1) * 0.5) * 85.0
                dx = math.cos(math.radians(yaw)) * off
                dy = math.sin(math.radians(yaw)) * off
                place(CUBE, (bx + dx, by + dy, row_z), (0.8, 0.45, 0.3), m["sandbag"], f"Sandbag_{i + 1}_{row_z}_{k}", f, rot=(0, yaw, 0))

    # Rusted Quarantine Metal Barriers with Yellow Hazard Trim
    barriers = [(-950, -1050, 35.0), (950, -1050, -35.0), (-950, 1050, -35.0), (950, 1050, 35.0)]
    for i, (x, y, yaw) in enumerate(barriers):
        place(CUBE, (x, y, 55), (2.8, 0.25, 1.1), m["rust_metal"], f"MetalBarrier_{i + 1}", f, rot=(0, yaw, 0))
        place(CUBE, (x, y, 102), (2.82, 0.28, 0.16), m["hazard"], f"BarrierHazard_{i + 1}", f, rot=(0, yaw, 0))

    # Burning Barrels (eerie orange combat glow) + Emergency Lights
    for i, (x, y) in enumerate([(-450, -700), (450, 700), (950, -300), (-950, 300)]):
        place(CYLINDER, (x, y, 48), (0.6, 0.6, 0.96), m["rust_metal"], f"FireBarrel_{i + 1}", f)
        place(SPHERE, (x, y, 105), (0.42, 0.42, 0.55), m["fire_glow"], f"BarrelFlame_{i + 1}", f, collision=False)
        add_point_light((x, y, 145), (255, 125, 35), 4200.0, 1300.0, f"BarrelLight_{i + 1}", f)

    # Red quarantine gate emergency beacons at North & South entrances
    for i, (x, y) in enumerate([(0, 1850), (0, -1850)]):
        place(SPHERE, (x, y, 290), (0.35, 0.35, 0.35), m["emergency_red"], f"GateBeacon_{i + 1}", f, collision=False)
        add_point_light((x, y, 310), (255, 20, 20), 4800.0, 1600.0, f"GateEmergencyLight_{i + 1}", f)

    # Toxic Biohazard Crater aligned above the Farm Pond (-2600, 2600)
    place(CYLINDER, (-2600, 2600, 5), (14.0, 14.0, 0.08), m["toxic_glow"], "Biohazard_Spill_AbovePond", f, collision=False)
    add_point_light((-2600, 2600, 160), (90, 255, 70), 3800.0, 1500.0, "Biohazard_Glow", f)

    # Tall quarantine floodlight poles casting eerie pools of pale sodium light in the dark fog
    for i, (lx, ly) in enumerate([(-1100, -1100), (1100, 1100), (-1100, 1100), (1100, -1100)]):
        place(CYLINDER, (lx, ly, 260), (0.18, 0.18, 5.2), m["rust_metal"], f"LightPole_{i + 1}", f)
        add_point_light((lx, ly, 510), (210, 185, 125), 2600.0, 1200.0, f"StreetLamp_{i + 1}", f)

    # Dead leafless spooky trees ringing the foggy compound perimeter
    dead_spots = [
        (-2150, -1400), (-2300, 400), (-2100, 1650), (2150, -1500),
        (2250, 350), (2100, 1600), (-1450, 2400), (1450, 2400), (1300, -2500),
    ]
    for i, (tx, ty) in enumerate(dead_spots):
        place(CYLINDER, (tx, ty, 220), (0.45, 0.45, 4.4), m["concrete_dark"], f"DeadTree_{i + 1}_Trunk", "Combat/DeadTrees")
        place(CYLINDER, (tx + 55, ty, 330), (0.14, 0.14, 1.8), m["concrete_dark"], f"DeadTree_{i + 1}_BranchA", "Combat/DeadTrees", rot=(0, 38, 25))
        place(CYLINDER, (tx - 50, ty + 35, 360), (0.12, 0.12, 1.6), m["concrete_dark"], f"DeadTree_{i + 1}_BranchB", "Combat/DeadTrees", rot=(20, -42, -30))


def build_atmosphere_and_audio(amb_sound):
    f = "Combat/Atmosphere"
    moon = actor_sub.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1500), unreal.Rotator(roll=0, pitch=-28, yaw=125))
    mlc = moon.light_component
    mlc.set_mobility(unreal.ComponentMobility.MOVABLE)
    mlc.set_editor_property("intensity", 0.45)
    mlc.set_editor_property("light_color", unreal.Color(r=95, g=120, b=175, a=255))
    moon.set_actor_label("Combat_OvercastMoon")
    moon.set_folder_path(f)

    sky = actor_sub.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1400), unreal.Rotator(0, 0, 0))
    slc = sky.light_component
    slc.set_mobility(unreal.ComponentMobility.MOVABLE)
    slc.set_editor_property("real_time_capture", True)
    slc.set_editor_property("intensity", 0.16)
    slc.set_editor_property("light_color", unreal.Color(r=45, g=58, b=75, a=255))
    sky.set_actor_label("Combat_DarkSkyLight")
    sky.set_folder_path(f)

    atmo = actor_sub.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    atmo.set_actor_label("Combat_SkyAtmosphere")
    atmo.set_folder_path(f)

    fog = actor_sub.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.065)
    fc.set_editor_property("fog_height_falloff", 0.25)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.025, 0.035, 0.03, 1.0))
    fog.set_actor_label("Combat_SmokeFog")
    fog.set_folder_path(f)

    # Lock exposure and add horror vignette so dark areas stay genuinely dark and spooky
    ppv = actor_sub.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    ppv.set_editor_property("unbound", True)
    settings = ppv.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 0.25)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 1.15)
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.58)
    ppv.set_editor_property("settings", settings)
    ppv.set_actor_label("Combat_DarkHorrorPostProcess")
    ppv.set_folder_path(f)

    if amb_sound:
        amb = actor_sub.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(0, 0, 300), unreal.Rotator(0, 0, 0))
        ac = amb.get_component_by_class(unreal.AudioComponent)
        ac.set_editor_property("sound", amb_sound)
        ac.set_editor_property("override_attenuation", True)
        att = ac.get_editor_property("attenuation_overrides")
        att.set_editor_property("attenuation_shape_extents", unreal.Vector(5000.0, 0.0, 0.0))
        att.set_editor_property("falloff_distance", 4000.0)
        ac.set_editor_property("attenuation_overrides", att)
        amb.set_actor_label("Combat_ZombieOutbreakAmbience")
        amb.set_folder_path("Combat/Audio")


def place_spawners_pickups_and_starts(slow_cls, fast_cls, m):
    # 6 Zombie Spawners behind cover (3 slow shamblers, 3 fast sprinters)
    spawner_cls = unreal.load_class(None, "/Script/GameJam_Shift_2026.ShooterNPCSpawner")
    spawns = [
        (-850, -1350, 45.0, slow_cls, "ZombieSpawner_Slow_1", 1.5),
        (850, -1350, 135.0, fast_cls, "ZombieSpawner_Fast_1", 2.0),
        (-1350, 200, 0.0, slow_cls, "ZombieSpawner_Slow_2", 2.5),
        (1350, -200, 180.0, fast_cls, "ZombieSpawner_Fast_2", 2.5),
        (-750, 1350, -45.0, fast_cls, "ZombieSpawner_Fast_3", 3.0),
        (750, 1350, -135.0, slow_cls, "ZombieSpawner_Slow_3", 3.5),
    ]
    for x, y, yaw, npc_cls, label, delay in spawns:
        sp = actor_sub.spawn_actor_from_class(spawner_cls, unreal.Vector(x, y, 0.0), unreal.Rotator(0, yaw, 0))
        sp.set_editor_property("NPCClass", npc_cls)
        sp.set_editor_property("InitialSpawnDelay", delay)
        sp.set_editor_property("SpawnCount", 6)
        sp.set_editor_property("RespawnDelay", 4.5)
        sp.set_actor_label(label)
        sp.set_folder_path("Combat/Zombies")

    # Weapon pickup crates (if BP_ShooterPickup exists)
    pickup_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/Pickups/BP_ShooterPickup")
    pickup_cls = pickup_bp.generated_class() if pickup_bp else unreal.load_class(None, "/Script/GameJam_Shift_2026.ShooterPickup")
    if pickup_cls:
        crate_locs = [(-260, 120), (260, -120), (-900, 0), (900, 0)]
        for i, (cx, cy) in enumerate(crate_locs):
            place(CUBE, (cx, cy, 24), (0.9, 0.6, 0.48), m["crate"], f"SupplyCrate_{i + 1}", "Combat/Pickups")
            pk = actor_sub.spawn_actor_from_class(pickup_cls, unreal.Vector(cx, cy, 55.0), unreal.Rotator(0, 0, 0))
            pk.set_actor_label(f"WeaponPickup_{i + 1}")
            pk.set_folder_path("Combat/Pickups")

    # PlayerStarts in center compound
    for i, (px, py, tag) in enumerate([(0, -250, "RED"), (0, 250, "BLUE")]):
        ps = actor_sub.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(px, py, 95.0), unreal.Rotator(0, 0, 0))
        ps.tags = [unreal.Name(tag)]
        ps.set_actor_label(f"PlayerStart_{i + 1}")
        ps.set_folder_path("Combat/Spawns")


def main():
    eal.make_directory(ENV_DIR)
    eal.make_directory(ENEMY_DIR)
    eal.make_directory("/Game/Dev_Blueprint/Maps")

    base = make_combat_base_material()
    m = {
        "asphalt": make_mi(base, "MI_Combat_Asphalt", (0.06, 0.065, 0.07), roughness=0.92),
        "concrete": make_mi(base, "MI_Combat_Concrete", (0.24, 0.25, 0.26), roughness=0.95),
        "concrete_dark": make_mi(base, "MI_Combat_ConcreteDark", (0.14, 0.145, 0.15), roughness=0.96),
        "sandbag": make_mi(base, "MI_Combat_Sandbag", (0.28, 0.26, 0.18), roughness=0.97),
        "rust_metal": make_mi(base, "MI_Combat_RustMetal", (0.22, 0.13, 0.09), roughness=0.65, metallic=0.6),
        "hazard": make_mi(base, "MI_Combat_Hazard", (0.85, 0.62, 0.05), roughness=0.5),
        "crate": make_mi(base, "MI_Combat_WoodCrate", (0.24, 0.16, 0.08), roughness=0.88),
        "emergency_red": make_mi(base, "MI_Combat_EmergencyRed", (0.9, 0.05, 0.05), roughness=0.2, emissive=(6.0, 0.2, 0.15)),
        "fire_glow": make_mi(base, "MI_Combat_FireGlow", (1.0, 0.4, 0.05), roughness=0.3, emissive=(4.5, 1.5, 0.1)),
        "toxic_glow": make_mi(base, "MI_Combat_ToxicGlow", (0.1, 0.45, 0.08), roughness=0.15, emissive=(0.2, 1.6, 0.15)),
        "zombie_slow": make_mi(base, "MI_Zombie_Slow", (0.18, 0.29, 0.14), roughness=0.85),
        "zombie_fast": make_mi(base, "MI_Zombie_Fast", (0.36, 0.10, 0.10), roughness=0.72),
    }

    attack_snd = import_wav("SFX_ZombieAttack", looping=False)
    amb_snd = import_wav("SFX_ZombieAmbience", looping=True)

    # Copy SkeletalMesh + AnimInstance from existing BP_ShooterNPC so zombies are animated humanoids
    template_mesh = None
    template_anim = None
    bp_npc = unreal.load_asset("/Game/Variant_Shooter/Blueprints/AI/BP_ShooterNPC")
    if bp_npc:
        npc_cdo = unreal.get_default_object(bp_npc.generated_class())
        npc_mesh = npc_cdo.get_editor_property("Mesh")
        if npc_mesh:
            template_mesh = npc_mesh.get_editor_property("skeletal_mesh_asset")
            template_anim = npc_mesh.get_editor_property("anim_class")
            log(f"Copied humanoid template from BP_ShooterNPC: mesh={template_mesh}, anim={template_anim}")

    bp_slow = create_zombie_blueprint(
        "BP_Zombie_Slow", hp=140.0, speed=200.0, damage=25.0, melee_range=175.0,
        cooldown=1.35, scale=1.12, skin_mat=m["zombie_slow"], attack_snd=attack_snd,
        template_mesh=template_mesh, template_anim=template_anim,
    )
    bp_fast = create_zombie_blueprint(
        "BP_Zombie_Fast", hp=70.0, speed=480.0, damage=12.0, melee_range=160.0,
        cooldown=0.70, scale=0.94, skin_mat=m["zombie_fast"], attack_snd=attack_snd,
        template_mesh=template_mesh, template_anim=template_anim,
    )

    if eal.does_asset_exist(LEVEL_PATH):
        eal.delete_asset(LEVEL_PATH)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

    # Assign Shooter GameMode to WorldSettings
    gm_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode")
    if gm_bp:
        ws = world.get_world_settings()
        ws.set_editor_property("default_game_mode", gm_bp.generated_class())

    # Ground tarmac slab (top surface Z = 0.0)
    place(CUBE, (0, 0, -50), (80.0, 80.0, 1.0), m["asphalt"], "Combat_Ground_Asphalt", "Combat/Terrain")

    # NavMeshBoundsVolume covering the compound so AIController pathing works smoothly
    nav = actor_sub.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
    if nav:
        nav.set_actor_scale3d(unreal.Vector(22.0, 24.0, 6.0))
        nav.set_actor_label("Combat_NavMeshBounds")
        nav.set_folder_path("Combat/AI")

    build_ruined_fortress(m)
    build_arena_cover_and_barricades(m)
    build_atmosphere_and_audio(amb_snd)
    place_spawners_pickups_and_starts(bp_slow.generated_class(), bp_fast.generated_class(), m)

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log(f"Saved {LEVEL_PATH}: {ok}, actors={len(actor_sub.get_all_level_actors())}")


main()
