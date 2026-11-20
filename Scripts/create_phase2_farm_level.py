"""
Builds /Game/Dev_Blueprint/Maps/Lvl_Farm (Blueprint Checklist Phase 2: tasks 2.1 - 2.4).

Content is authored around the local origin (ground = Z 0). At runtime UDimensionShiftSubsystem
streams this level into the combat world at FarmWorldOffset (0, 0, -20000).

Layout notes (local coordinates, cm):
  * Combat arena footprint is X +-1500, Y +-1800. Every combat XY maps onto flat, walkable farm
    ground (flat valley X +-3200, Y +-3600) so a Shift is never denied for lack of floor.
  * Field / soil beds / plots sit in the middle of the valley; barn, silo, pond and trees sit
    outside the arena footprint so they never block a Shift arrival point.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nopause -unattended -NullRHI -nosound
"""
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Dev_Blueprint/Maps/Lvl_Farm"
ENV_DIR = "/Game/Dev_Blueprint/Maps/Farm_Environment"
AUDIO_DIR = "/Game/Dev_Blueprint/Audio"
FARM_TAG = "FarmWorld"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) or unreal.EditorLevelLibrary

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone")


def log(msg):
    unreal.log_warning(f"[FARM] {msg}")


# ---------------------------------------------------------------------------------------------
# Terrain height field (shared by the mesh builder and prop placement)
# ---------------------------------------------------------------------------------------------
HALF_SIZE = 30000.0          # terrain spans +-300 m
GRID = 241                   # 241 x 241 vertices -> 250 cm spacing
POND_X, POND_Y, POND_R, POND_DEPTH = -2600.0, 2600.0, 950.0, 170.0
WATER_Z = -55.0
TROUGH_X, TROUGH_Y = 40.0, 330.0


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def hill_mask(x, y):
    dx = max(0.0, abs(x) - 3200.0)
    dy = max(0.0, abs(y) - 3600.0)
    return smoothstep(math.hypot(dx, dy) / 9000.0)


def terrain_height(x, y):
    m = hill_mask(x, y)
    hills = 2600.0 * m * (0.62 + 0.24 * math.sin(x * 0.00023 + 0.8) + 0.14 * math.cos(y * 0.00031 + 0.3))
    roll = 240.0 * m * math.sin(x * 0.0009 + 1.1) * math.sin(y * 0.0011 + 0.4)
    h = max(0.0, hills + roll)
    r = math.hypot(x - POND_X, y - POND_Y)
    if r < POND_R:
        h -= POND_DEPTH * smoothstep(1.0 - r / POND_R)
    return h


# ---------------------------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------------------------
def recreate(path):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)


def make_solid_material():
    path = f"{ENV_DIR}/M_FarmSolid"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = asset_tools.create_asset("M_FarmSolid", ENV_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 200)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.85)
    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 320)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.0)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def make_instance(parent, name, color, roughness=0.85, metallic=0.0):
    path = f"{ENV_DIR}/{name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, ENV_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    eal.save_loaded_asset(mi)
    return mi


def make_terrain_material():
    path = f"{ENV_DIR}/M_FarmTerrain"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = asset_tools.create_asset("M_FarmTerrain", ENV_DIR, unreal.Material, unreal.MaterialFactoryNew())
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 0)

    def noise(scale, y):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -850, y)
        n.set_editor_property("scale", scale)
        n.set_editor_property("levels", 4)
        n.set_editor_property("output_min", 0.0)
        n.set_editor_property("output_max", 1.0)
        mel.connect_material_expressions(wp, "", n, "Position")
        return n

    n_fine = noise(0.004, -150)
    n_patch = noise(0.0006, 150)

    def const3(rgb, y):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, y)
        c.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
        return c

    grass_dark = const3((0.045, 0.13, 0.025), -300)
    grass_light = const3((0.11, 0.24, 0.04), -180)
    grass_dry = const3((0.26, 0.24, 0.08), 300)

    lerp1 = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, -200)
    mel.connect_material_expressions(grass_dark, "", lerp1, "A")
    mel.connect_material_expressions(grass_light, "", lerp1, "B")
    mel.connect_material_expressions(n_fine, "", lerp1, "Alpha")

    patch_scale = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 150)
    half = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -800, 300)
    half.set_editor_property("r", 0.45)
    mel.connect_material_expressions(n_patch, "", patch_scale, "A")
    mel.connect_material_expressions(half, "", patch_scale, "B")

    lerp2 = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -150, 0)
    mel.connect_material_expressions(lerp1, "", lerp2, "A")
    mel.connect_material_expressions(grass_dry, "", lerp2, "B")
    mel.connect_material_expressions(patch_scale, "", lerp2, "Alpha")

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -150, 200)
    rough.set_editor_property("r", 0.95)
    mel.connect_material_property(lerp2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


# ---------------------------------------------------------------------------------------------
# Terrain mesh (built from a mesh description: no importer, no axis conversion)
# ---------------------------------------------------------------------------------------------
def build_terrain_mesh(material):
    path = f"{ENV_DIR}/SM_FarmTerrain"
    if eal.does_asset_exist(path):
        mesh = unreal.load_asset(path)
        if mesh:
            bb = mesh.get_bounding_box()
            log(f"SM_FarmTerrain loaded from cache: bounds min={bb.min} max={bb.max}")
            return mesh
    recreate(path)
    mesh = eal.duplicate_asset("/Engine/BasicShapes/Plane", path)
    if not mesh:
        log("ERROR: could not create SM_FarmTerrain")
        return None

    mesh.set_editor_property("static_materials", [unreal.StaticMaterial(material_interface=material, material_slot_name="Terrain")])

    desc = mesh.create_static_mesh_description()
    pg = desc.create_polygon_group()
    desc.set_polygon_group_material_slot_name(pg, "Terrain")

    step = (2.0 * HALF_SIZE) / (GRID - 1)
    vis = []
    for j in range(GRID):
        row = []
        y = -HALF_SIZE + j * step
        for i in range(GRID):
            x = -HALF_SIZE + i * step
            vid = desc.create_vertex()
            desc.set_vertex_position(vid, unreal.Vector(x, y, terrain_height(x, y)))
            vi = desc.create_vertex_instance(vid)
            desc.set_vertex_instance_uv(vi, unreal.Vector2D(x / 400.0, y / 400.0), 0)
            row.append(vi)
        vis.append(row)

    for j in range(GRID - 1):
        for i in range(GRID - 1):
            a, b = vis[j][i], vis[j][i + 1]
            c, d = vis[j + 1][i], vis[j + 1][i + 1]
            # Clockwise winding seen from above (UE front faces)
            desc.create_triangle(pg, [a, c, b])
            desc.create_triangle(pg, [b, c, d])

    # Let the regular build compute normals/tangents when StaticMeshEditorSubsystem is available
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if sm_sub:
        settings = sm_sub.get_lod_build_settings(mesh, 0)
        settings.set_editor_property("recompute_normals", True)
        settings.set_editor_property("recompute_tangents", True)
        sm_sub.set_lod_build_settings(mesh, 0, settings)

    mesh.build_from_static_mesh_descriptions([desc], False, False)

    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh)
    bb = mesh.get_bounding_box()
    log(f"SM_FarmTerrain built: {GRID}x{GRID} verts, bounds min={bb.min} max={bb.max}")
    return mesh


# ---------------------------------------------------------------------------------------------
# Actor helpers
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


def tag_farm(actor, label, folder):
    actor.tags = [unreal.Name(FARM_TAG)]
    actor.set_actor_label(label)
    actor.set_folder_path(folder)


# ---------------------------------------------------------------------------------------------
# 2.2 Environment: barn, silo, fences, hay bales, trees, pond, path
# ---------------------------------------------------------------------------------------------
def build_barn(m):
    bx, by = -700.0, -3300.0
    f = "Farm/Barn"
    wall_h, half_w, depth = 600.0, 600.0, 900.0
    place(CUBE, (bx, by, wall_h / 2 - 30), (12.0, 9.0, (wall_h + 60) / 100.0), m["barn_red"], "Barn_Walls", f)
    # Gable fill (45 deg diamond, lower half hidden inside the walls)
    side = half_w * math.sqrt(2.0) / 100.0
    place(CUBE, (bx, by, wall_h), (side, depth / 100.0 - 0.02, side), m["barn_red"], "Barn_Gable", f, rot=(45.0, 0.0, 0.0))
    # Roof slabs at 45 deg with 60 cm overhang
    span = half_w + 60.0
    slab_w = span / math.cos(math.radians(45.0)) / 100.0
    for sgn, name in ((-1, "Barn_Roof_L"), (1, "Barn_Roof_R")):
        place(CUBE, (bx + sgn * span / 2.0, by, wall_h + span / 2.0 + 10.0), (slab_w, (depth + 120.0) / 100.0, 0.25),
              m["roof"], name, f, rot=(-sgn * 45.0, 0.0, 0.0))
    place(CUBE, (bx, by, wall_h + half_w + 18.0), (0.5, (depth + 130.0) / 100.0, 0.3), m["roof"], "Barn_RidgeCap", f)
    # Front (+Y) big door with white X-brace trim, plus hay loft door
    fy = by + depth / 2.0 + 3.0
    place(CUBE, (bx, fy, 230.0), (3.6, 0.06, 4.6), m["barn_door"], "Barn_Door", f)
    for pitch in (50.0, -50.0):
        place(CUBE, (bx, fy + 4.0, 230.0), (0.14, 0.04, 5.6), m["trim"], "Barn_DoorBrace", f, rot=(pitch, 0.0, 0.0))
    place(CUBE, (bx, fy + 4.0, 460.0), (3.8, 0.04, 0.14), m["trim"], "Barn_DoorHeader", f)
    for sx in (-1, 1):
        place(CUBE, (bx + sx * 185.0, fy + 4.0, 230.0), (0.14, 0.04, 4.6), m["trim"], "Barn_DoorFrame", f)
    place(CUBE, (bx, fy, 760.0), (1.4, 0.06, 1.3), m["barn_door"], "Barn_LoftDoor", f)
    place(CUBE, (bx, fy + 4.0, 760.0), (1.6, 0.04, 0.12), m["trim"], "Barn_LoftTrim", f)
    # Corner trims
    for sx in (-1, 1):
        for sy in (-1, 1):
            place(CUBE, (bx + sx * 600.0, by + sy * 450.0, wall_h / 2 - 30), (0.2, 0.2, (wall_h + 60) / 100.0), m["trim"], "Barn_CornerTrim", f)

    # Silo
    sx_, sy_ = bx + 1050.0, by - 150.0
    place(CYLINDER, (sx_, sy_, 560.0), (4.0, 4.0, 11.6), m["silo"], "Silo_Body", "Farm/Silo")
    place(SPHERE, (sx_, sy_, 1130.0), (4.1, 4.1, 2.6), m["silo_roof"], "Silo_Dome", "Farm/Silo")
    for z in (250.0, 550.0, 850.0):
        place(CYLINDER, (sx_, sy_, z), (4.08, 4.08, 0.12), m["roof"], "Silo_Band", "Farm/Silo")


def build_field_and_plots(m):
    """2.4 Soil layout: flattened central plot area with tilled rows and a 4x4 grid of farm plots."""
    f = "Farm/Field"
    xs = [325.0, 575.0, 825.0, 1075.0]
    ys = [-375.0, -125.0, 125.0, 375.0]
    # Tilled soil bed under the whole field + one raised row per column of plots
    # Soil bed top at Z = 3.2 sits above the paths (top 2.2), so the two never share a surface
    place(CUBE, (700.0, 0.0, 1.2), (12.4, 11.4, 0.04), m["soil_dry"], "Field_SoilBed", f, collision=False)
    for x in xs:
        place(CUBE, (x, 0.0, 3.0), (1.6, 10.4, 0.06), m["soil_tilled"], "Field_TilledRow", f, collision=False)
    plot_cls = unreal.load_class(None, "/Script/GameJam_Shift_2026.FarmPlotBase")
    count = 0
    for r, x in enumerate(xs):
        for c, y in enumerate(ys):
            plot = actor_sub.spawn_actor_from_class(plot_cls, unreal.Vector(x, y, 0.0), unreal.Rotator(0, 0, 0))
            plot.set_actor_label(f"FarmPlot_R{r + 1}_C{c + 1}")
            plot.set_folder_path(f"{f}/Plots")
            count += 1
    log(f"Placed {count} FarmPlotBase actors")

    # Fence around the field (entrance gap facing the path on the -X side)
    ff = "Farm/Fences"
    x0, x1, y0, y1 = 150.0, 1250.0, -650.0, 650.0

    def post(x, y):
        place(CUBE, (x, y, 55.0), (0.12, 0.12, 1.1), m["wood"], "Fence_Post", ff)

    def rail(xa, ya, xb, yb):
        length = math.hypot(xb - xa, yb - ya)
        yaw = math.degrees(math.atan2(yb - ya, xb - xa))
        for z in (45.0, 88.0):
            place(CUBE, ((xa + xb) / 2, (ya + yb) / 2, z), (length / 100.0, 0.06, 0.1), m["wood"], "Fence_Rail", ff, rot=(0.0, yaw, 0.0))

    for y in (y0, y1):
        xs_ = [x0 + i * (x1 - x0) / 5 for i in range(6)]
        for x in xs_:
            post(x, y)
        rail(x0, y, x1, y)
    for y in [y0 + i * (y1 - y0) / 6 for i in range(1, 6)]:
        post(x1, y)
    rail(x1, y0, x1, y1)
    for ya, yb in ((y0, -150.0), (150.0, y1)):
        post(x0, -150.0 if ya == y0 else 150.0)
        rail(x0, ya, x0, yb)

    # Scarecrow in the field corner
    sf = "Farm/Field/Scarecrow"
    place(CUBE, (1175.0, 560.0, 110.0), (0.1, 0.1, 2.2), m["wood"], "Scarecrow_Pole", sf)
    place(CUBE, (1175.0, 560.0, 170.0), (0.08, 1.3, 0.08), m["wood"], "Scarecrow_Arms", sf)
    place(CUBE, (1175.0, 560.0, 150.0), (0.35, 0.55, 0.6), m["cloth"], "Scarecrow_Shirt", sf)
    place(SPHERE, (1175.0, 560.0, 215.0), (0.32, 0.32, 0.34), m["hay"], "Scarecrow_Head", sf)
    place(CONE, (1175.0, 560.0, 250.0), (0.55, 0.55, 0.3), m["hay"], "Scarecrow_Hat", sf)


def build_props(m):
    f = "Farm/Props"
    # Hay bales: round bales stacked beside the barn, square bales by the field gate
    for i, (x, y, z) in enumerate([(-1650, -2650, 70), (-1650, -2490, 70), (-1650, -2570, 190), (-1800, -2950, 70)]):
        place(CYLINDER, (x, y, z), (1.4, 1.4, 1.5), m["hay"], f"HayBale_Round_{i + 1}", f, rot=(0.0, 0.0, 90.0))
    for i, (x, y, z) in enumerate([(60, -330, 23), (60, -450, 23), (60, -390, 68)]):
        place(CUBE, (x, y, z), (0.55, 1.1, 0.45), m["hay"], f"HayBale_Square_{i + 1}", f)
    # Hollow wooden water trough by the gate (outer 70 x 160 cm, 7 cm walls, top at Z = 60)
    tx, ty = TROUGH_X, TROUGH_Y
    place(CUBE, (tx, ty, 8.0), (0.7, 1.6, 0.16), m["wood"], "WaterTrough_Base", f)
    place(CUBE, (tx - 31.5, ty, 38.0), (0.07, 1.6, 0.44), m["wood"], "WaterTrough_WallL", f)
    place(CUBE, (tx + 31.5, ty, 38.0), (0.07, 1.6, 0.44), m["wood"], "WaterTrough_WallR", f)
    place(CUBE, (tx, ty - 76.5, 38.0), (0.56, 0.07, 0.44), m["wood"], "WaterTrough_WallFront", f)
    place(CUBE, (tx, ty + 76.5, 38.0), (0.56, 0.07, 0.44), m["wood"], "WaterTrough_WallBack", f)
    # Water sits inside the basin: bottom 2 cm above the base, top 8 cm below the rim, 1 cm inset from the walls
    place(CUBE, (tx, ty, 35.0), (0.54, 1.44, 0.34), m["water"], "WaterTrough_Water", f, collision=False)

    # Dirt paths (decals made of thin slabs, no collision so they never trip the player). Top surface at Z = 2.2
    pf = "Farm/Paths"
    place(CUBE, (-1150.0, 0.0, 0.7), (26.0, 2.6, 0.03), m["dirt"], "Path_Main", pf, collision=False)
    place(CUBE, (-700.0, -1400.0, 0.7), (2.2, 25.0, 0.03), m["dirt"], "Path_ToBarn", pf, collision=False)
    place(CUBE, (-2000.0, 1250.0, 0.7), (2.0, 22.0, 0.03), m["dirt"], "Path_ToPond", pf, collision=False, rot=(0.0, -35.0, 0.0))

    # Pond: water disc sits below the rim so the terrain hides its edge
    wf = "Farm/Pond"
    place(CYLINDER, (POND_X, POND_Y, WATER_Z), (POND_R * 2 * 0.86 / 100.0, POND_R * 2 * 0.86 / 100.0, 0.02), m["water"], "Pond_Water", wf, collision=False)
    rng = random.Random(11)
    for i in range(9):
        a = i / 9.0 * 2 * math.pi + rng.uniform(-0.2, 0.2)
        r = POND_R * rng.uniform(0.92, 1.08)
        x, y = POND_X + math.cos(a) * r, POND_Y + math.sin(a) * r
        s = rng.uniform(0.5, 1.1)
        place(SPHERE, (x, y, terrain_height(x, y) + 5.0), (s * 1.3, s, s * 0.55), m["stone"], f"Pond_Rock_{i + 1}", wf, rot=(0.0, rng.uniform(0, 360), 0.0))
    for i in range(14):
        a = rng.uniform(0, 2 * math.pi)
        r = POND_R * rng.uniform(0.75, 0.9)
        x, y = POND_X + math.cos(a) * r, POND_Y + math.sin(a) * r
        place(CYLINDER, (x, y, WATER_Z + 45.0), (0.05, 0.05, 1.2), m["reed"], f"Pond_Reed_{i + 1}", wf, collision=False)


def build_trees(m):
    f = "Farm/Trees"
    rng = random.Random(2026)
    placed = 0
    attempts = 0
    trunks = []
    while placed < 46 and attempts < 1000:
        attempts += 1
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(4300, 14000)
        x, y = math.cos(a) * r, math.sin(a) * r
        if abs(x) < 1800 and abs(y) < 2100:
            continue  # keep the arena footprint clear
        if math.hypot(x - POND_X, y - POND_Y) < POND_R + 500:
            continue
        if -2400 < x < 1200 and -4500 < y < -2300:
            continue  # barn yard
        z = terrain_height(x, y) - 20.0
        s = rng.uniform(0.8, 1.35)
        leaves = m["leaves"] if rng.random() < 0.6 else m["leaves2"]
        place(CYLINDER, (x, y, z + 220 * s), (0.55 * s, 0.55 * s, 4.4 * s), m["trunk"], f"Tree_{placed + 1}_Trunk", f)
        place(SPHERE, (x, y, z + 560 * s), (4.2 * s, 4.2 * s, 3.6 * s), leaves, f"Tree_{placed + 1}_Canopy", f, collision=False)
        place(SPHERE, (x + 90 * s, y - 70 * s, z + 720 * s), (2.9 * s, 2.9 * s, 2.5 * s), leaves, f"Tree_{placed + 1}_CanopyTop", f, collision=False)
        trunks.append((x, y))
        placed += 1
    # Bushes along the valley edge
    for i in range(30):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(3600, 6000)
        x, y = math.cos(a) * r, math.sin(a) * r
        if math.hypot(x - POND_X, y - POND_Y) < POND_R + 200 or (-2400 < x < 1200 and -4500 < y < -2300):
            continue
        s = rng.uniform(0.7, 1.3)
        place(SPHERE, (x, y, terrain_height(x, y) + 40 * s), (1.6 * s, 1.4 * s, 1.1 * s), m["leaves2"], f"Bush_{i + 1}", f, collision=False)
    log(f"Placed {placed} trees")
    return trunks


# ---------------------------------------------------------------------------------------------
# Foliage, wildflowers, planters, stepping stones and lily pads (all non-blocking except planters and stones)
# ---------------------------------------------------------------------------------------------
PATH_SEGMENTS = []  # (ax, ay, bx, by, half_width) for each dirt path; foliage must stay clear of these
FLOWER_MATS = ["flower_yellow", "flower_white", "flower_lavender", "flower_coral"]
STEPPING_STONES = [(-850.0, -2360.0), (-1110.0, -2360.0), (-1370.0, -2360.0), (-1700.0, -2330.0), (-1950.0, -2440.0)]


def build_path_segments():
    PATH_SEGMENTS.clear()
    PATH_SEGMENTS.append((-2450.0, 0.0, 150.0, 0.0, 130.0))  # Path_Main (26 x 2.6 m)
    PATH_SEGMENTS.append((-700.0, -2650.0, -700.0, -150.0, 110.0))  # Path_ToBarn (2.2 x 25 m)
    yaw = math.radians(-35.0)
    dx, dy = -math.sin(yaw), math.cos(yaw)
    cx, cy = -2000.0, 1250.0
    PATH_SEGMENTS.append((cx - 1100.0 * dx, cy - 1100.0 * dy, cx + 1100.0 * dx, cy + 1100.0 * dy, 100.0))  # Path_ToPond


def seg_distance(px, py, ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * abx + (py - ay) * aby) / (abx * abx + aby * aby)))
    return math.hypot(px - (ax + t * abx), py - (ay + t * aby))


def in_box(x, y, x0, x1, y0, y1, pad=0.0):
    return x0 - pad <= x <= x1 + pad and y0 - pad <= y <= y1 + pad


def is_clear(x, y, pad=40.0):
    """True when (x, y) is outside the crop field, barn, farmhouse, water trough, paths and pond."""
    if in_box(x, y, 140.0, 1270.0, -670.0, 670.0, pad):
        return False
    if in_box(x, y, -1350.0, -50.0, -3800.0, -2700.0, pad):
        return False
    if in_box(x, y, -2350.0, -1550.0, -3550.0, -2650.0, pad):
        return False
    if in_box(x, y, -10.0, 110.0, 240.0, 420.0, pad):
        return False
    for ax, ay, bx, by, hw in PATH_SEGMENTS:
        if seg_distance(x, y, ax, ay, bx, by) < hw + pad:
            return False
    return math.hypot(x - POND_X, y - POND_Y) >= POND_R + pad


def build_wildflower_cluster(m, rng, x, y, idx):
    f = "Farm/Details/Wildflowers"
    z = terrain_height(x, y)
    for k in range(rng.randint(3, 5)):
        ox, oy = rng.uniform(-28.0, 28.0), rng.uniform(-28.0, 28.0)
        stem = rng.uniform(0.22, 0.32)  # 200 cm cylinder scaled -> 44..64 cm stems
        yaw = math.radians(rng.uniform(0.0, 360.0))
        tilt = math.radians(rng.uniform(4.0, 10.0))
        sx, sy = x + ox, y + oy
        half = stem * 100.0
        place(CYLINDER, (sx, sy, z + half), (0.025, 0.025, stem), m["weed_green"], f"Wildflower_{idx}_{k}_Stem", f,
              rot=(math.degrees(tilt), math.degrees(yaw), 0.0), collision=False)
        top = (sx - math.sin(tilt) * math.cos(yaw) * half, sy - math.sin(tilt) * math.sin(yaw) * half, z + half + math.cos(tilt) * half)
        place(SPHERE, top, (0.12, 0.12, 0.08), m[rng.choice(FLOWER_MATS)], f"Wildflower_{idx}_{k}_Bloom", f, collision=False)


def build_grass_tuft(m, rng, x, y, idx):
    f = "Farm/Details/Grass"
    z = terrain_height(x, y)
    for b in range(rng.randint(2, 3)):
        h = rng.uniform(0.25, 0.42)  # 100 cm cone scaled -> 25..42 cm blades
        mat = m["weed_green"] if rng.random() < 0.6 else m["weed_dark"]
        place(CONE, (x + rng.uniform(-6.0, 6.0), y + rng.uniform(-6.0, 6.0), z + h * 45.0), (0.04, 0.04, h), mat,
              f"GrassTuft_{idx}_{b}", f, rot=(rng.uniform(12.0, 24.0), rng.uniform(0.0, 360.0), 0.0), collision=False)


def build_flower_planter(m, rng, x, y, yaw, idx):
    f = "Farm/Details/Planters"
    place(CUBE, (x, y, 17.5), (1.2, 0.4, 0.35), m["wood"], f"Planter_{idx}_Box", f, rot=(0.0, yaw, 0.0))
    place(CUBE, (x, y, 35.0), (1.12, 0.32, 0.03), m["soil_dry"], f"Planter_{idx}_Soil", f, rot=(0.0, yaw, 0.0), collision=False)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for k, off in enumerate((-45.0, -22.0, 0.0, 22.0, 45.0)):
        bx, by = x + off * c, y + off * s
        place(CYLINDER, (bx, by, 36.5 + 12.0), (0.025, 0.025, 0.12), m["weed_green"], f"Planter_{idx}_Stem{k}", f, collision=False)
        place(SPHERE, (bx, by, 36.5 + 28.0), (0.12, 0.12, 0.12), m[rng.choice(FLOWER_MATS)], f"Planter_{idx}_Bloom{k}", f, collision=False)


def build_lily_pads(m, rng):
    f = "Farm/Pond/Lilies"
    pads = []
    for _ in range(400):
        if len(pads) >= 12:
            break
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = POND_R * rng.uniform(0.12, 0.5)  # deep water only; reeds and rocks stay on the rim
        x, y = POND_X + math.cos(a) * r, POND_Y + math.sin(a) * r
        if all(math.hypot(x - px, y - py) >= 150.0 for px, py in pads):
            pads.append((x, y))
    for i, (x, y) in enumerate(pads):
        place(CYLINDER, (x, y, WATER_Z + 1.5), (0.6, 0.6, 0.02), m["lily_pad"], f"LilyPad_{i + 1}", f, collision=False)
    for i, mat_key in enumerate(("lily_pink", "lily_white")):
        if i * 6 + 2 < len(pads):
            x, y = pads[i * 6 + 2]
            place(SPHERE, (x, y, WATER_Z + 4.0), (0.16, 0.16, 0.09), m[mat_key], f"WaterLily_{i + 1}", f, collision=False)
    log(f"Placed {len(pads)} lily pads on the pond")


def build_foliage_and_details(m, trunks):
    rng = random.Random(777)
    build_path_segments()

    # Wildflower clusters at natural focal points (each one is checked against the clear zones)
    flower_spots = [
        (80.0, -730.0), (1340.0, -730.0), (1340.0, 730.0), (80.0, 730.0),  # field fence corners
        (-420.0, 210.0), (-1050.0, -215.0), (-2250.0, -220.0),  # Path_Main flanks
        (-420.0, -950.0), (-980.0, -1600.0),  # Path_ToBarn flanks
        (-1534.0, 1461.0), (-1960.0, 1759.0),  # Path_ToPond flanks
        (-430.0, -2520.0), (-980.0, -2520.0),  # Barn entrance
        (-2440.0, -2480.0),  # Farmhouse yard, west of the porch
    ]
    for a in (0.9, 4.3):  # Pond bank, just outside the rim
        flower_spots.append((POND_X + math.cos(a) * POND_R * 1.12, POND_Y + math.sin(a) * POND_R * 1.12))
    flowers = 0
    for idx, (x, y) in enumerate(flower_spots):
        if is_clear(x, y, 25.0):
            build_wildflower_cluster(m, rng, x, y, idx + 1)
            flowers += 1
        else:
            log(f"Skipped wildflower cluster at ({x:.0f}, {y:.0f}): inside a clear zone")
    log(f"Placed {flowers} wildflower clusters")

    # Grass tufts and weeds along fence bases, barn corners, pond shore, rocks and tree trunks
    candidates = []
    for i in range(11):
        x = 150.0 + i * 110.0
        candidates += [(x, -720.0), (x, 720.0)]
    for i in range(11):
        y = -650.0 + i * 130.0
        candidates += [(1340.0, y), (80.0, y)]
    for sx in (-1, 1):
        for sy in (-1, 1):
            candidates.append((-700.0 + sx * 710.0, -3300.0 + sy * 560.0))
    for _ in range(14):
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = POND_R * rng.uniform(1.05, 1.2)
        candidates.append((POND_X + math.cos(a) * r, POND_Y + math.sin(a) * r))
    for tx_, ty_ in trunks:
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = rng.uniform(90.0, 130.0)
        candidates.append((tx_ + math.cos(a) * r, ty_ + math.sin(a) * r))
    rng.shuffle(candidates)
    tufts = 0
    for x, y in candidates:
        if tufts >= 32:
            break
        if is_clear(x, y, 40.0):
            build_grass_tuft(m, rng, x, y, tufts + 1)
            tufts += 1
    log(f"Placed {tufts} grass tufts")

    # Flower planter boxes: barn front, beside the field gate, and a second one by the barn
    build_flower_planter(m, rng, -400.0, -2640.0, 0.0, 1)
    build_flower_planter(m, rng, -1080.0, -2640.0, 0.0, 2)
    build_flower_planter(m, rng, 100.0, -245.0, 90.0, 3)

    # Stepping stones from the barn path to the farmhouse porch (smooth, low, seated flush in grass)
    for i, (x, y) in enumerate(STEPPING_STONES):
        place(CYLINDER, (x, y, 2.0), (0.45, 0.45, 0.08), m["stone"], f"StepStone_{i + 1}", "Farm/Details/Stones")
    log(f"Placed {len(STEPPING_STONES)} stepping stones")

    build_lily_pads(m, rng)


# ---------------------------------------------------------------------------------------------
# 2.3 Atmosphere, lighting and ambient sound
# ---------------------------------------------------------------------------------------------
def build_lighting():
    f = "Farm/Lighting"
    sun = actor_sub.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1500), unreal.Rotator(roll=0.0, pitch=-24.0, yaw=-55.0))
    lc = sun.light_component
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property("intensity", 7.5)
    lc.set_editor_property("light_color", unreal.Color(r=255, g=196, b=128, a=255))  # warm golden hour
    lc.set_editor_property("atmosphere_sun_light", True)
    tag_farm(sun, "Farm_Sun_GoldenHour", f)

    sky = actor_sub.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1400), unreal.Rotator(0, 0, 0))
    sc = sky.light_component
    sc.set_mobility(unreal.ComponentMobility.MOVABLE)
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("intensity", 1.1)
    sc.set_editor_property("light_color", unreal.Color(r=200, g=222, b=255, a=255))  # soft blue sky fill
    tag_farm(sky, "Farm_SkyLight", f)

    atmo = actor_sub.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    ac = atmo.get_component_by_class(unreal.SkyAtmosphereComponent)
    # Planet top follows the actor so the sky is correct at Z = -20,000 after streaming
    ac.set_editor_property("transform_mode", unreal.SkyAtmosphereTransformMode.PLANET_TOP_AT_COMPONENT_TRANSFORM)
    tag_farm(atmo, "Farm_SkyAtmosphere", f)

    fog = actor_sub.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.012)
    fc.set_editor_property("fog_height_falloff", 0.15)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.55, 0.45, 0.32, 1.0))
    tag_farm(fog, "Farm_HeightFog", f)

    cloud = actor_sub.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    tag_farm(cloud, "Farm_VolumetricCloud", f)
    log("Lighting: golden sun, blue sky light, atmosphere (planet top at component), warm fog, clouds")


def build_ambient_sound():
    wav_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts")
    wav = os.path.join(wav_dir, "SFX_FarmAmbience.wav")
    asset_path = f"{AUDIO_DIR}/SFX_FarmAmbience"
    if not eal.does_asset_exist(asset_path):
        task = unreal.AssetImportTask()
        task.filename = wav
        task.destination_path = AUDIO_DIR
        task.destination_name = "SFX_FarmAmbience"
        task.automated = True
        task.save = True
        task.replace_existing = True
        asset_tools.import_asset_tasks([task])
    sound = unreal.load_asset(asset_path)
    if not sound:
        log("ERROR: SFX_FarmAmbience import failed")
        return
    sound.set_editor_property("looping", True)
    eal.save_loaded_asset(sound)

    amb = actor_sub.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(300, 0, 300), unreal.Rotator(0, 0, 0))
    comp = amb.get_component_by_class(unreal.AudioComponent)
    comp.set_editor_property("sound", sound)
    comp.set_editor_property("override_attenuation", True)
    att = comp.get_editor_property("attenuation_overrides")
    att.set_editor_property("attenuation_shape_extents", unreal.Vector(6000.0, 0.0, 0.0))  # full volume across the valley
    att.set_editor_property("falloff_distance", 5000.0)                                    # silent long before Z = 0
    comp.set_editor_property("attenuation_overrides", att)
    amb.set_actor_label("Farm_Ambience_BirdsWind")
    amb.set_folder_path("Farm/Audio")
    log("Ambient sound placed (birds, wind, leaves; 11,000 cm audible radius)")


# ---------------------------------------------------------------------------------------------
def main():
    eal.make_directory(ENV_DIR)
    eal.make_directory("/Game/Dev_Blueprint/Maps")

    solid = make_solid_material()
    m = {
        "barn_red": make_instance(solid, "MI_Farm_BarnRed", (0.32, 0.035, 0.025), 0.8),
        "barn_door": make_instance(solid, "MI_Farm_BarnDoor", (0.2, 0.03, 0.02), 0.85),
        "trim": make_instance(solid, "MI_Farm_Trim", (0.78, 0.75, 0.68), 0.7),
        "roof": make_instance(solid, "MI_Farm_Roof", (0.06, 0.055, 0.055), 0.6),
        "silo": make_instance(solid, "MI_Farm_SiloMetal", (0.55, 0.56, 0.58), 0.35, 0.8),
        "silo_roof": make_instance(solid, "MI_Farm_SiloRoof", (0.42, 0.12, 0.08), 0.5, 0.5),
        "wood": make_instance(solid, "MI_Farm_Wood", (0.22, 0.13, 0.06), 0.9),
        "hay": make_instance(solid, "MI_Farm_Hay", (0.72, 0.55, 0.2), 0.95),
        "cloth": make_instance(solid, "MI_Farm_Cloth", (0.15, 0.2, 0.42), 0.95),
        "soil_dry": make_instance(solid, "MI_Farm_SoilDry", (0.16, 0.1, 0.055), 0.97),
        "soil_tilled": make_instance(solid, "MI_Farm_SoilTilled", (0.075, 0.045, 0.025), 0.98),
        "dirt": make_instance(solid, "MI_Farm_DirtPath", (0.27, 0.19, 0.1), 0.97),
        "water": make_instance(solid, "MI_Farm_Water", (0.04, 0.22, 0.30), 0.06, 0.05),
        "stone": make_instance(solid, "MI_Farm_Stone", (0.3, 0.29, 0.27), 0.85),
        "reed": make_instance(solid, "MI_Farm_Reed", (0.2, 0.3, 0.08), 0.9),
        "flower_yellow": make_instance(solid, "MI_Farm_FlowerYellow", (0.92, 0.72, 0.12), 0.6),
        "flower_white": make_instance(solid, "MI_Farm_FlowerWhite", (0.94, 0.93, 0.86), 0.6),
        "flower_lavender": make_instance(solid, "MI_Farm_FlowerLavender", (0.58, 0.44, 0.85), 0.6),
        "flower_coral": make_instance(solid, "MI_Farm_FlowerCoral", (0.86, 0.28, 0.22), 0.6),
        "weed_green": make_instance(solid, "MI_Farm_WeedGreen", (0.10, 0.28, 0.05), 0.9),
        "weed_dark": make_instance(solid, "MI_Farm_WeedDark", (0.06, 0.19, 0.035), 0.9),
        "lily_pad": make_instance(solid, "MI_Farm_LilyPad", (0.12, 0.36, 0.10), 0.6),
        "lily_pink": make_instance(solid, "MI_Farm_LilyPink", (0.95, 0.50, 0.66), 0.5),
        "lily_white": make_instance(solid, "MI_Farm_LilyWhite", (0.97, 0.96, 0.92), 0.5),
        "trunk": make_instance(solid, "MI_Farm_TreeTrunk", (0.13, 0.08, 0.045), 0.95),
        "leaves": make_instance(solid, "MI_Farm_Leaves", (0.06, 0.2, 0.035), 0.9),
        "leaves2": make_instance(solid, "MI_Farm_LeavesLight", (0.12, 0.27, 0.05), 0.9),
    }
    terrain_mat = make_terrain_material()
    terrain_mesh = build_terrain_mesh(terrain_mat)

    # 2.1 Create level (non-partitioned so it can be streamed as a level instance)
    if eal.does_asset_exist(LEVEL_PATH):
        eal.delete_asset(LEVEL_PATH)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    log(f"New blank map created: {world}")

    if terrain_mesh:
        place(terrain_mesh, (0, 0, 0), (1.0, 1.0, 1.0), terrain_mat, "Farm_Terrain_RollingHills", "Farm/Terrain")
    else:
        # Fallback flat ground so the farm is always walkable
        place(CUBE, (0, 0, -50), (600.0, 600.0, 1.0), m["leaves2"], "Farm_Terrain_FlatFallback", "Farm/Terrain")

    build_barn(m)
    build_field_and_plots(m)
    build_props(m)
    trunks = build_trees(m)
    build_foliage_and_details(m, trunks)
    build_lighting()
    build_ambient_sound()

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log(f"Saved {LEVEL_PATH}: {ok}")
    actors = actor_sub.get_all_level_actors()
    log(f"Lvl_Farm actor count: {len(actors)}")


main()
