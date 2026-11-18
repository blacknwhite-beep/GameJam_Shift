"""
Builds Phase 5 Zombie Quarantine Compound & Melee Zombie Enemies (Checklist 5.1 - 5.4):
  - /Game/Dev_Blueprint/Blueprints/Enemies/BP_Zombie_Slow (Slow Shambler, Melee)
  - /Game/Dev_Blueprint/Blueprints/Enemies/BP_Zombie_Fast (Fast Sprinter, Melee)
  - /Game/Dev_Blueprint/Maps/Lvl_Combat.umap (at Z = 0, aligned with Lvl_Farm at Z = -20,000)

Procedural world-space materials (M_Combat_Asphalt, M_Combat_Concrete, M_Combat_RustMetal) and
bespoke SM_Combat_* meshes are generated from StaticMeshDescription, mirroring
create_phase2_farm_level.py.

Run headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nopause -unattended -NullRHI -nosound
"""
import math
import os
import random

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

LIGHT_MIN_CD, LIGHT_MAX_CD = 12.0, 28.0
AXES = [(1, 2), (0, 2), (0, 1)]


def log(msg):
    unreal.log_warning(f"[COMBAT] {msg}")


# ---------------------------------------------------------------------------------------------
# Vector helpers
# ---------------------------------------------------------------------------------------------
def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_mul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def v_norm(a):
    length = math.sqrt(v_dot(a, a)) or 1.0
    return v_mul(a, 1.0 / length)


def rotate(p, rot):
    """rot = (pitch, yaw, roll) in degrees, applied as roll (X), pitch (Y), yaw (Z)."""
    pitch, yaw, roll = (math.radians(r) for r in rot)
    x, y, z = p
    y, z = y * math.cos(roll) - z * math.sin(roll), y * math.sin(roll) + z * math.cos(roll)
    x, z = x * math.cos(pitch) + z * math.sin(pitch), -x * math.sin(pitch) + z * math.cos(pitch)
    x, y = x * math.cos(yaw) - y * math.sin(yaw), x * math.sin(yaw) + y * math.cos(yaw)
    return (x, y, z)


def uv_of(p, n):
    ax, ay, az = abs(n[0]), abs(n[1]), abs(n[2])
    if az >= ax and az >= ay:
        return (p[0] / 100.0, p[1] / 100.0)
    if ax >= ay:
        return (p[1] / 100.0, p[2] / 100.0)
    return (p[0] / 100.0, p[2] / 100.0)


def clamp_cd(intensity):
    return max(LIGHT_MIN_CD, min(LIGHT_MAX_CD, intensity))


# ---------------------------------------------------------------------------------------------
# Mesh builder over StaticMeshDescription (same API calls as the Phase 2 terrain builder)
# ---------------------------------------------------------------------------------------------
class MeshBuilder:
    def __init__(self, desc, slots):
        self.desc = desc
        self.pg = {}
        for slot in slots:
            pg = desc.create_polygon_group()
            desc.set_polygon_group_material_slot_name(pg, slot)
            self.pg[slot] = pg

    def _vert(self, p, n):
        vid = self.desc.create_vertex()
        self.desc.set_vertex_position(vid, unreal.Vector(*p))
        vi = self.desc.create_vertex_instance(vid)
        self.desc.set_vertex_instance_uv(vi, unreal.Vector2D(*uv_of(p, n)), 0)
        return vi

    def tri(self, a, b, c, n, slot):
        # UE front faces satisfy cross(b - a, c - a) . n < 0; flip the winding when needed.
        if v_dot(v_cross(v_sub(b, a), v_sub(c, a)), n) > 0:
            b, c = c, b
        ids = [self._vert(p, n) for p in (a, b, c)]
        self.desc.create_triangle(self.pg[slot], ids)

    def quad(self, a, b, c, d, n, slot):
        self.tri(a, b, c, n, slot)
        self.tri(a, c, d, n, slot)

    def box(self, c, size, slot, rot=(0.0, 0.0, 0.0)):
        h = [size[0] / 2.0, size[1] / 2.0, size[2] / 2.0]
        for axis in range(3):
            t1, t2 = AXES[axis]
            for sgn in (-1.0, 1.0):
                normal = [0.0, 0.0, 0.0]
                normal[axis] = sgn
                pts = []
                for u, v in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    p = [0.0, 0.0, 0.0]
                    p[axis] = sgn * h[axis]
                    p[t1] = u * h[t1]
                    p[t2] = v * h[t2]
                    pts.append(v_add(c, rotate(p, rot)))
                self.quad(pts[0], pts[1], pts[2], pts[3], rotate(normal, rot), slot)

    def cylinder(self, c, radius, height, slot, seg=10, rot=(0.0, 0.0, 0.0)):
        hh = height / 2.0
        for i in range(seg):
            a0 = 2 * math.pi * i / seg
            a1 = 2 * math.pi * (i + 1) / seg
            n0 = (math.cos(a0), math.sin(a0), 0.0)
            n1 = (math.cos(a1), math.sin(a1), 0.0)
            nm = v_norm(v_add(n0, n1))
            p00 = v_add(c, rotate((radius * n0[0], radius * n0[1], -hh), rot))
            p01 = v_add(c, rotate((radius * n0[0], radius * n0[1], hh), rot))
            p11 = v_add(c, rotate((radius * n1[0], radius * n1[1], hh), rot))
            p10 = v_add(c, rotate((radius * n1[0], radius * n1[1], -hh), rot))
            self.quad(p00, p10, p11, p01, rotate(nm, rot), slot)
        top = v_add(c, rotate((0.0, 0.0, hh), rot))
        bot = v_add(c, rotate((0.0, 0.0, -hh), rot))
        for i in range(seg):
            a0 = 2 * math.pi * i / seg
            a1 = 2 * math.pi * (i + 1) / seg
            t0 = v_add(c, rotate((radius * math.cos(a0), radius * math.sin(a0), hh), rot))
            t1 = v_add(c, rotate((radius * math.cos(a1), radius * math.sin(a1), hh), rot))
            b0 = v_add(c, rotate((radius * math.cos(a0), radius * math.sin(a0), -hh), rot))
            b1 = v_add(c, rotate((radius * math.cos(a1), radius * math.sin(a1), -hh), rot))
            self.tri(top, t0, t1, rotate((0.0, 0.0, 1.0), rot), slot)
            self.tri(bot, b1, b0, rotate((0.0, 0.0, -1.0), rot), slot)

    def tube(self, p0, p1, r0, r1, slot, seg=7, caps=(True, True)):
        ax = v_norm(v_sub(p1, p0))
        helper = (0.0, 0.0, 1.0) if abs(ax[2]) < 0.9 else (1.0, 0.0, 0.0)
        u = v_norm(v_cross(ax, helper))
        v = v_cross(ax, u)
        ring0, ring1 = [], []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            d = v_add(v_mul(u, math.cos(a)), v_mul(v, math.sin(a)))
            ring0.append(v_add(p0, v_mul(d, r0)))
            ring1.append(v_add(p1, v_mul(d, r1)))
        for i in range(seg):
            j = (i + 1) % seg
            d = v_norm(v_add(v_sub(ring0[i], p0), v_sub(ring0[j], p0)))
            self.quad(ring0[i], ring1[i], ring1[j], ring0[j], d, slot)
        for i in range(seg):
            j = (i + 1) % seg
            if caps[0]:
                self.tri(p0, ring0[j], ring0[i], v_mul(ax, -1.0), slot)
            if caps[1]:
                self.tri(p1, ring1[i], ring1[j], ax, slot)

    def jagged_wall(self, xs, hs, thickness, slot):
        y0, y1 = -thickness / 2.0, thickness / 2.0
        for i in range(len(xs) - 1):
            x0, x1 = xs[i], xs[i + 1]
            h0, h1 = hs[i], hs[i + 1]
            self.quad((x0, y0, 0.0), (x1, y0, 0.0), (x1, y0, h1), (x0, y0, h0), (0.0, -1.0, 0.0), slot)
            self.quad((x0, y1, 0.0), (x1, y1, 0.0), (x1, y1, h1), (x0, y1, h0), (0.0, 1.0, 0.0), slot)
            self.quad((x0, y0, h0), (x1, y0, h1), (x1, y1, h1), (x0, y1, h0), (0.0, 0.0, 1.0), slot)
        self.quad((xs[0], y0, 0.0), (xs[0], y1, 0.0), (xs[0], y1, hs[0]), (xs[0], y0, hs[0]), (-1.0, 0.0, 0.0), slot)
        self.quad((xs[-1], y0, 0.0), (xs[-1], y0, hs[-1]), (xs[-1], y1, hs[-1]), (xs[-1], y1, 0.0), (1.0, 0.0, 0.0), slot)

    def pillow(self, c, size, slot, n=6, rnd=0.45, rot=(0.0, 0.0, 0.0)):
        h = (size[0] / 2.0, size[1] / 2.0, size[2] / 2.0)

        def surf(axis, sgn, u, v):
            q = [0.0, 0.0, 0.0]
            q[axis] = sgn
            t1, t2 = AXES[axis]
            q[t1] = u
            q[t2] = v
            s = v_norm(tuple(q))
            p = [((1.0 - rnd) * q[k] + rnd * s[k]) * h[k] for k in range(3)]
            return v_add(c, rotate(tuple(p), rot))

        for axis in range(3):
            for sgn in (-1.0, 1.0):
                for i in range(n):
                    for j in range(n):
                        u0, u1 = -1 + 2.0 * i / n, -1 + 2.0 * (i + 1) / n
                        v0, v1 = -1 + 2.0 * j / n, -1 + 2.0 * (j + 1) / n
                        a = surf(axis, sgn, u0, v0)
                        b = surf(axis, sgn, u1, v0)
                        cc = surf(axis, sgn, u1, v1)
                        d = surf(axis, sgn, u0, v1)
                        mid = v_mul(v_add(v_add(a, b), v_add(cc, d)), 0.25)
                        self.quad(a, b, cc, d, v_norm(v_sub(mid, c)), slot)


# ---------------------------------------------------------------------------------------------
# Materials (procedural, world-space, same node pattern as the Phase 2 terrain material)
# ---------------------------------------------------------------------------------------------
def recreate(path):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)


def mat_noise(mat, wp, scale, levels, x, y):
    n = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, x, y)
    n.set_editor_property("scale", scale)
    n.set_editor_property("levels", levels)
    n.set_editor_property("output_min", 0.0)
    n.set_editor_property("output_max", 1.0)
    mel.connect_material_expressions(wp, "", n, "Position")
    return n


def mat_const3(mat, rgb, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    return c


def mat_const1(mat, value, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", value)
    return c


def mat_lerp(mat, a, b, alpha, x, y):
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    mel.connect_material_expressions(a, "", lp, "A")
    mel.connect_material_expressions(b, "", lp, "B")
    mel.connect_material_expressions(alpha, "", lp, "Alpha")
    return lp


def mat_mul(mat, a, b, x, y):
    m = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, "", m, "A")
    mel.connect_material_expressions(b, "", m, "B")
    return m


def mat_tint(mat, x, y):
    t = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    t.set_editor_property("parameter_name", "Tint")
    t.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    return t


def finish_material(mat, base=None, metallic=None, roughness=None):
    if base is not None:
        mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if metallic is not None:
        mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    if roughness is not None:
        mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def make_master(name, graph):
    path = f"{ENV_DIR}/{name}"
    recreate(path)
    mat = asset_tools.create_asset(name, ENV_DIR, unreal.Material, unreal.MaterialFactoryNew())
    outputs = graph(mat)
    return finish_material(mat, *outputs)


def graph_asphalt(mat):
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 0)
    grime = mat_noise(mat, wp, 0.006, 4, -850, 0)
    crack = mat_noise(mat, wp, 0.05, 3, -850, 200)
    puddle = mat_noise(mat, wp, 0.0014, 2, -850, 400)
    dark_a = mat_const3(mat, (0.03, 0.032, 0.036), -350, -100)
    dark_b = mat_const3(mat, (0.085, 0.09, 0.095), -350, 100)
    col = mat_lerp(mat, dark_a, dark_b, grime, -150, 0)
    crack_amt = mat_mul(mat, crack, mat_const1(mat, 0.4, -600, 260), -450, 200)
    crack_col = mat_const3(mat, (0.01, 0.01, 0.012), -150, 220)
    col = mat_lerp(mat, col, crack_col, crack_amt, 50, 0)
    rough_wet = mat_const1(mat, 0.18, -150, 360)
    rough_dry = mat_const1(mat, 0.92, -150, 480)
    rough = mat_lerp(mat, rough_dry, rough_wet, puddle, 100, 300)
    return col, None, rough


def graph_concrete(mat):
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 0)
    fine = mat_noise(mat, wp, 0.06, 4, -850, 0)
    mottle = mat_noise(mat, wp, 0.012, 4, -850, 200)
    grime = mat_noise(mat, wp, 0.003, 3, -850, 400)
    light = mat_const3(mat, (0.46, 0.46, 0.44), -350, -100)
    dark = mat_const3(mat, (0.17, 0.17, 0.165), -350, 100)
    mottled = mat_lerp(mat, light, dark, mottle, -150, 0)
    grime_amt = mat_mul(mat, grime, mat_const1(mat, 0.55, -600, 420), -450, 380)
    grime_col = mat_const3(mat, (0.07, 0.068, 0.064), -150, 220)
    col = mat_lerp(mat, mottled, grime_col, grime_amt, 50, 0)
    col = mat_mul(mat, col, mat_tint(mat, 250, 0), 300, 0)
    rough = mat_lerp(mat, mat_const1(mat, 0.98, -150, 560), mat_const1(mat, 0.74, -150, 680), fine, 100, 560)
    return col, None, rough


def graph_rust(mat):
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 0)
    rust = mat_noise(mat, wp, 0.02, 4, -850, 0)
    fine = mat_noise(mat, wp, 0.09, 4, -850, 200)
    metal = mat_const3(mat, (0.24, 0.24, 0.25), -350, -100)
    rust_c = mat_const3(mat, (0.45, 0.18, 0.07), -350, 100)
    col = mat_lerp(mat, metal, rust_c, rust, -150, 0)
    col = mat_mul(mat, col, mat_tint(mat, 250, 0), 300, 0)
    metallic = mat_lerp(mat, mat_const1(mat, 0.85, -150, 200), mat_const1(mat, 0.12, -150, 320), rust, 100, 200)
    rough = mat_lerp(mat, mat_const1(mat, 0.42, -150, 440), mat_const1(mat, 0.9, -150, 560), fine, 100, 480)
    return col, metallic, rough


def make_combat_solid_material():
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


def make_tint_mi(parent, name, tint):
    path = f"{ENV_DIR}/{name}"
    recreate(path)
    mi = asset_tools.create_asset(name, ENV_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
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
# Bespoke SM_Combat_* meshes (built from StaticMeshDescription, origin at base centre)
# ---------------------------------------------------------------------------------------------
def sm_broken_wall(b):
    rng = random.Random(7)
    xs = [-300.0 + 25.0 * i for i in range(25)]
    hs = [max(150.0, min(380.0, 300.0 + rng.uniform(-110.0, 80.0))) for _ in xs]
    b.jagged_wall(xs, hs, 40.0, "Concrete")
    for x in (-180.0, 40.0, 220.0):
        i = min(range(len(xs)), key=lambda k: abs(xs[k] - x))
        top = hs[i]
        tip = (x + rng.uniform(-20.0, 20.0), rng.uniform(-6.0, 6.0), top + rng.uniform(55.0, 95.0))
        b.tube((x, 0.0, top - 12.0), tip, 2.4, 1.2, "Rebar", seg=6)


def sm_sandbag_cluster(b):
    bags = [(-88.0, 15.0), (0.0, 15.0), (88.0, 15.0), (-44.0, 42.0), (44.0, 42.0)]
    for i, (x, z) in enumerate(bags):
        b.pillow((x, 0.0, z), (84.0, 48.0, 30.0), "Sandbag", n=6, rnd=0.45, rot=(0.0, (i - 2) * 2.5, 0.0))


def sm_metal_barricade(b):
    w = 260.0
    b.box((-w / 2 + 5, 0.0, 55.0), (10.0, 12.0, 110.0), "Metal")
    b.box((w / 2 - 5, 0.0, 55.0), (10.0, 12.0, 110.0), "Metal")
    b.box((0.0, 0.0, 10.0), (w, 12.0, 14.0), "Metal")
    b.box((0.0, 0.0, 104.0), (w + 8.0, 16.0, 14.0), "Hazard")
    xs = [-w / 2 + 12.0 + i * (w - 24.0) / 14.0 for i in range(15)]
    ys = [0.0 if i % 2 == 0 else 9.0 for i in range(15)]
    for i in range(14):
        x0, x1, y0, y1 = xs[i], xs[i + 1], ys[i], ys[i + 1]
        nrm = v_norm((-(y1 - y0), x1 - x0, 0.0))
        a, bb, cc, d = (x0, y0, 20.0), (x1, y1, 20.0), (x1, y1, 96.0), (x0, y0, 96.0)
        b.quad(a, bb, cc, d, nrm, "Metal")
        b.quad(a, bb, cc, d, v_mul(nrm, -1.0), "Metal")


def sm_watchtower(b):
    big, small, height = 230.0, 200.0, 900.0
    bot = [(big, -big), (big, big), (-big, big), (-big, -big)]
    top = [(small, -small), (small, small), (-small, small), (-small, -small)]
    for k in range(4):
        (x0, y0), (x1, y1) = bot[k], bot[(k + 1) % 4]
        (tx0, ty0), (tx1, ty1) = top[k], top[(k + 1) % 4]
        mid = ((x0 + x1) / 2.0, (y0 + y1) / 2.0)
        ml = math.hypot(mid[0], mid[1])
        n = (mid[0] / ml, mid[1] / ml, 0.12)
        b.quad((x0, y0, 0.0), (x1, y1, 0.0), (tx1, ty1, height), (tx0, ty0, height), n, "Concrete")
    b.box((0.0, 0.0, -2.0), (big * 2.0, big * 2.0, 4.0), "Concrete")
    b.box((0.0, 0.0, 909.0), (520.0, 520.0, 18.0), "Concrete")
    segments = [(-255.0, -150.0, 170.0), (-130.0, -40.0, 80.0), (-20.0, 120.0, 170.0), (135.0, 255.0, 110.0)]
    for yaw in (0.0, 90.0, 180.0, 270.0):
        for s, e, h in segments:
            local = (s + e) / 2.0, 252.0, 918.0 + h / 2.0
            centre = rotate(local, (0.0, yaw, 0.0))
            b.box(centre, (e - s, 22.0, h), "Concrete", rot=(0.0, yaw, 0.0))
    b.tube((0.0, 0.0, 918.0), (0.0, 0.0, 1085.0), 14.0, 12.0, "Metal", seg=8)
    b.box((0.0, 0.0, 1040.0), (90.0, 8.0, 8.0), "Metal")
    b.box((0.0, 0.0, 1092.0), (44.0, 44.0, 14.0), "Metal")


def sm_dead_tree(b):
    pts = [(22.0 * math.sin(k * 0.95), 16.0 * math.cos(k * 1.2), k * 95.0) for k in range(7)]
    radii = [34.0, 30.0, 25.0, 21.0, 17.0, 14.0, 11.0]
    for k in range(6):
        b.tube(pts[k], pts[k + 1], radii[k], radii[k + 1], "Bark", seg=7, caps=(k == 0, k == 5))
    for k, ang, length in ((3, 2.3, 110.0), (4, 4.0, 95.0), (5, 0.9, 100.0), (6, 5.2, 80.0)):
        p = pts[k]
        d = v_norm((math.cos(ang), math.sin(ang), 0.6))
        mid = v_add(p, v_add(v_mul(d, length), (0.0, 0.0, 20.0)))
        tip = v_add(mid, v_add(v_mul(d, length * 0.8), (0.0, 0.0, 60.0)))
        b.tube(p, mid, radii[k] * 0.55, radii[k] * 0.36, "Bark", seg=6, caps=(False, False))
        b.tube(mid, tip, radii[k] * 0.36, 2.2, "Bark", seg=5, caps=(False, True))


def sm_supply_crate(b):
    b.box((0.0, 0.0, 45.0), (150.0, 100.0, 90.0), "Body")
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            b.box((sx * 72.0, sy * 52.0, 47.0), (10.0, 10.0, 96.0), "Frame")
    b.box((0.0, -52.0, 45.0), (120.0, 4.0, 60.0), "Frame")
    b.box((0.0, 0.0, 92.0), (156.0, 106.0, 6.0), "Frame")
    ang = math.degrees(math.atan2(90.0, 150.0))
    b.box((0.0, -53.0, 45.0), (170.0, 3.0, 6.0), "Frame", rot=(ang, 0.0, 0.0))
    b.box((0.0, -53.0, 45.0), (170.0, 3.0, 6.0), "Frame", rot=(-ang, 0.0, 0.0))


SM_SPECS = {
    "SM_Combat_BrokenWall": (["Concrete", "Rebar"], sm_broken_wall),
    "SM_Combat_SandbagCluster": (["Sandbag"], sm_sandbag_cluster),
    "SM_Combat_MetalBarricade": (["Metal", "Hazard"], sm_metal_barricade),
    "SM_Combat_Watchtower": (["Concrete", "Metal"], sm_watchtower),
    "SM_Combat_DeadTree": (["Bark"], sm_dead_tree),
    "SM_Combat_SupplyCrate": (["Body", "Frame"], sm_supply_crate),
}
SM_ASSETS = {}


def build_static_mesh(name, slots, mats, build_fn):
    path = f"{ENV_DIR}/{name}"
    recreate(path)
    mesh = eal.duplicate_asset("/Engine/BasicShapes/Cube", path)
    if not mesh:
        raise RuntimeError(f"could not create {name}")
    mesh.set_editor_property(
        "static_materials",
        [unreal.StaticMaterial(material_interface=mats[i], material_slot_name=slots[i]) for i in range(len(slots))],
    )
    desc = mesh.create_static_mesh_description()
    desc.empty()
    builder = MeshBuilder(desc, slots)
    build_fn(builder)
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
    log(f"{name} built: bounds min={bb.min} max={bb.max}")
    return mesh


# ---------------------------------------------------------------------------------------------
# Melee Zombie Blueprints (BP_Zombie_Slow & BP_Zombie_Fast)
# ---------------------------------------------------------------------------------------------
def create_zombie_blueprint(
    bp_name, hp, speed, ref_anim_speed, use_run_anim, damage, melee_range, cooldown,
    scale, skin_mat, attack_snd, zombie_mesh, anims_dict, attack_key, death_key, idle_key
):
    bp_path = f"{ENEMY_DIR}/{bp_name}"
    if eal.does_asset_exist(bp_path):
        bp = unreal.load_asset(bp_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.ZombieEnemy)
        bp = asset_tools.create_asset(bp_name, ENEMY_DIR, unreal.Blueprint, factory)
    if not bp:
        log(f"ERROR creating {bp_path}")
        return None

    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("CurrentHP", hp)
    cdo.set_editor_property("ZombieMoveSpeed", speed)
    cdo.set_editor_property("bUseRunAnim", use_run_anim)
    cdo.set_editor_property("ReferenceAnimSpeed", ref_anim_speed)
    cdo.set_editor_property("MeshYawOffset", -90.0)
    cdo.set_editor_property("MeleeDamage", damage)
    cdo.set_editor_property("MeleeRange", melee_range)
    cdo.set_editor_property("AttackCooldown", cooldown)
    cdo.set_editor_property("ZombieSkinMaterial", skin_mat)
    if attack_snd:
        cdo.set_editor_property("AttackGroanSound", attack_snd)

    for prop, key in (("IdleAnim", idle_key), ("WalkAnim", "walk"), ("RunAnim", "run"), ("AttackAnim", attack_key), ("DeathAnim", death_key)):
        if anims_dict.get(key):
            cdo.set_editor_property(prop, anims_dict[key])

    mesh_comp = cdo.get_editor_property("Mesh")
    if mesh_comp:
        if zombie_mesh:
            mesh_comp.set_skeletal_mesh(zombie_mesh)
        mesh_comp.set_anim_instance_class(None)
        if skin_mat:
            mesh_comp.set_material(0, skin_mat)
        mesh_comp.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -90.0))
        mesh_comp.set_editor_property("relative_rotation", unreal.Rotator(0.0, -90.0, 0.0))
        mesh_comp.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    log(f"Ready {bp_path} (HP={hp}, Speed={speed}, RefAnimSpeed={ref_anim_speed}, RunAnim={use_run_anim})")
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


def place_sm(name, loc, label, folder, rot=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
    mesh, slots, mesh_mats = SM_ASSETS[name]
    actor = actor_sub.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*loc),
        unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]),
    )
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    for i, mat in enumerate(mesh_mats):
        comp.set_material(i, mat)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def wall_run(p0, p1, label, folder, height_scale=1.0):
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    length = math.hypot(dx, dy)
    yaw = math.degrees(math.atan2(dy, dx))
    count = max(1, math.ceil(length / 600.0))
    seg = length / count
    for k in range(count):
        t = (k + 0.5) / count
        centre = (p0[0] + dx * t, p0[1] + dy * t, 0.0)
        place_sm("SM_Combat_BrokenWall", centre, f"{label}_{k + 1}", folder,
                 rot=(0.0, yaw, 0.0), scale=(seg / 600.0, 1.0, height_scale))


def add_point_light(loc, color_rgb, intensity, radius, label, folder):
    pl = actor_sub.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc), unreal.Rotator(0, 0, 0))
    lc = pl.point_light_component
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property("light_color", unreal.Color(r=color_rgb[0], g=color_rgb[1], b=color_rgb[2], a=255))
    lc.set_editor_property("intensity", clamp_cd(intensity))
    lc.set_editor_property("attenuation_radius", radius)
    pl.set_actor_label(label)
    pl.set_folder_path(folder)
    return pl


def build_ruined_fortress(m):
    """Aligned directly above Lvl_Farm's Barn at (-700, -3300)!"""
    fx, fy = -700.0, -3300.0
    f = "Combat/RuinedFortress"
    place(CUBE, (fx, fy, 10), (13.0, 10.0, 0.2), m["concrete"], "Fortress_Foundation", f)
    wall_run((fx - 560.0, fy - 460.0), (fx - 560.0, fy + 460.0), "Fortress_Wall_W", f)
    wall_run((fx + 560.0, fy - 460.0), (fx + 560.0, fy + 220.0), "Fortress_Wall_E", f)
    wall_run((fx - 560.0, fy - 440.0), (fx + 560.0, fy - 440.0), "Fortress_Wall_Back", f)
    wall_run((fx - 570.0, fy + 440.0), (fx - 150.0, fy + 440.0), "Fortress_Front_L", f)
    wall_run((fx + 150.0, fy + 440.0), (fx + 570.0, fy + 440.0), "Fortress_Front_R", f)
    tx, ty = fx + 450.0, fy - 350.0
    place_sm("SM_Combat_Watchtower", (tx, ty, 0.0), "Fortress_Watchtower", f)
    place(SPHERE, (tx, ty, 1135.0), (0.45, 0.45, 0.45), m["emergency_red"], "Fortress_BeaconMesh", f, collision=False)
    add_point_light((tx, ty, 1160.0), (255, 25, 20), 28.0, 2200.0, "Fortress_EmergencyLight", f)


def build_arena_cover_and_barricades(m):
    f = "Combat/Obstacles"
    wall_run((-1850.0, -2100.0), (-1850.0, 2100.0), "PerimeterWall_West", f)
    wall_run((1850.0, -2100.0), (1850.0, 2100.0), "PerimeterWall_East", f)
    wall_run((-1850.0, -2150.0), (1850.0, -2150.0), "PerimeterWall_South", f)
    wall_run((-1850.0, 2150.0), (1850.0, 2150.0), "PerimeterWall_North", f)

    walls = [
        (-550, -500, 4.5, 15.0),
        (600, -450, 4.2, -20.0),
        (-650, 620, 4.8, -12.0),
        (580, 580, 4.0, 22.0),
        (0, 950, 5.2, 0.0),
        (0, -950, 5.2, 0.0),
    ]
    for i, (x, y, length, yaw) in enumerate(walls):
        place_sm("SM_Combat_BrokenWall", (x, y, 0.0), f"BrokenWall_{i + 1}", f,
                 rot=(0.0, yaw, 0.0), scale=(length * 100.0 / 600.0, 1.0, 1.0))

    bunkers = [(-320, 180, 0.0), (340, -180, 90.0), (-1100, 0, 90.0), (1100, 0, 90.0)]
    for i, (bx, by, yaw) in enumerate(bunkers):
        place_sm("SM_Combat_SandbagCluster", (bx, by, 0.0), f"SandbagBunker_{i + 1}", f, rot=(0.0, yaw, 0.0))

    barriers = [(-950, -1050, 35.0), (950, -1050, -35.0), (-950, 1050, -35.0), (950, 1050, 35.0)]
    for i, (x, y, yaw) in enumerate(barriers):
        place_sm("SM_Combat_MetalBarricade", (x, y, 0.0), f"MetalBarricade_{i + 1}", f, rot=(0.0, yaw, 12.0))

    for i, (x, y) in enumerate([(-450, -700), (450, 700), (950, -300), (-950, 300)]):
        place(CYLINDER, (x, y, 48), (0.6, 0.6, 0.96), m["rust_metal"], f"FireBarrel_{i + 1}", f)
        place(SPHERE, (x, y, 105), (0.42, 0.42, 0.55), m["fire_glow"], f"BarrelFlame_{i + 1}", f, collision=False)
        add_point_light((x, y, 145), (255, 115, 30), 12.0, 1100.0, f"BarrelLight_{i + 1}", f)

    for i, (x, y) in enumerate([(0, 1850), (0, -1850)]):
        place(SPHERE, (x, y, 290), (0.35, 0.35, 0.35), m["emergency_red"], f"GateBeacon_{i + 1}", f, collision=False)
        add_point_light((x, y, 310), (255, 20, 20), 16.0, 1400.0, f"GateEmergencyLight_{i + 1}", f)

    place(CYLINDER, (-2600, 2600, 5), (14.0, 14.0, 0.08), m["toxic_glow"], "Biohazard_Spill_AbovePond", f, collision=False)
    add_point_light((-2600, 2600, 160), (90, 255, 70), 18.0, 1400.0, "Biohazard_Glow", f)

    for i, (lx, ly) in enumerate([(-1100, -1100), (1100, 1100), (-1100, 1100), (1100, -1100)]):
        place(CYLINDER, (lx, ly, 260), (0.18, 0.18, 5.2), m["rust_metal"], f"LightPole_{i + 1}", f)
        add_point_light((lx, ly, 510), (200, 180, 130), 18.0, 1200.0, f"StreetLamp_{i + 1}", f)

    dead_spots = [
        (-2150, -1400), (-2300, 400), (-2100, 1650), (2150, -1500),
        (2250, 350), (2100, 1600), (-1450, 2400), (1450, 2400), (1300, -2500),
    ]
    for i, (tx, ty) in enumerate(dead_spots):
        place_sm("SM_Combat_DeadTree", (tx, ty, 0.0), f"DeadTree_{i + 1}", "Combat/DeadTrees",
                 rot=(0.0, i * 37.0, 0.0), scale=(1.2, 1.2, 1.2))

    for i, (cx, cy) in enumerate([(-260, 120), (260, -120), (-900, 0), (900, 0)]):
        place_sm("SM_Combat_SupplyCrate", (cx, cy, 0.0), f"SupplyCrate_{i + 1}", "Combat/Pickups",
                 rot=(0.0, i * 45.0, 0.0))


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

    ppv = actor_sub.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
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
    spawner_cls = unreal.load_class(None, "/Script/GameJam_Shift_2026.ShooterNPCSpawner")
    spawns = [
        (-750, -1250, 45.0, slow_cls, "ZombieSpawner_Slow_1", 1.5),
        (750, -1250, 135.0, fast_cls, "ZombieSpawner_Fast_1", 2.0),
        (-1350, 0, 0.0, slow_cls, "ZombieSpawner_Slow_2", 2.5),
        (1350, 0, 180.0, fast_cls, "ZombieSpawner_Fast_2", 2.5),
        (-750, 1250, -45.0, fast_cls, "ZombieSpawner_Fast_3", 3.0),
        (750, 1250, -135.0, slow_cls, "ZombieSpawner_Slow_3", 3.5),
    ]
    for x, y, yaw, npc_cls, label, delay in spawns:
        sp = actor_sub.spawn_actor_from_class(spawner_cls, unreal.Vector(x, y, 0.0), unreal.Rotator(0, yaw, 0))
        sp.set_editor_property("NPCClass", npc_cls)
        sp.set_editor_property("InitialSpawnDelay", delay)
        sp.set_editor_property("SpawnCount", 6)
        sp.set_editor_property("RespawnDelay", 4.5)
        sp.set_actor_label(label)
        sp.set_folder_path("Combat/Zombies")

    pickup_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/Pickups/BP_ShooterPickup")
    pickup_cls = pickup_bp.generated_class() if pickup_bp else unreal.load_class(None, "/Script/GameJam_Shift_2026.ShooterPickup")
    if pickup_cls:
        crate_locs = [(-260, 120), (260, -120), (-900, 0), (900, 0)]
        for i, (cx, cy) in enumerate(crate_locs):
            pk = actor_sub.spawn_actor_from_class(pickup_cls, unreal.Vector(cx, cy, 55.0), unreal.Rotator(0, 0, 0))
            pk.set_actor_label(f"WeaponPickup_{i + 1}")
            pk.set_folder_path("Combat/Pickups")

    for i, (px, py, tag) in enumerate([(0, -250, "RED"), (0, 250, "BLUE")]):
        ps = actor_sub.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(px, py, 95.0), unreal.Rotator(0, 0, 0))
        ps.tags = [unreal.Name(tag)]
        ps.set_actor_label(f"PlayerStart_{i + 1}")
        ps.set_folder_path("Combat/Spawns")


def main():
    eal.make_directory(ENV_DIR)
    eal.make_directory(ENEMY_DIR)
    eal.make_directory("/Game/Dev_Blueprint/Maps")

    base = make_combat_solid_material()
    asphalt = make_master("M_Combat_Asphalt", graph_asphalt)
    concrete = make_master("M_Combat_Concrete", graph_concrete)
    rust = make_master("M_Combat_RustMetal", graph_rust)
    mats = {
        "asphalt": asphalt,
        "concrete": make_tint_mi(concrete, "MI_Combat_Concrete", (1.0, 1.0, 1.0)),
        "concrete_dark": make_tint_mi(concrete, "MI_Combat_ConcreteDark", (0.55, 0.55, 0.56)),
        "sandbag": make_tint_mi(concrete, "MI_Combat_Sandbag", (0.86, 0.74, 0.52)),
        "bark": make_tint_mi(concrete, "MI_Combat_DeadBark", (0.42, 0.3, 0.2)),
        "rust_metal": make_tint_mi(rust, "MI_Combat_RustMetal", (1.0, 1.0, 1.0)),
        "rebar": make_tint_mi(rust, "MI_Combat_Rebar", (0.6, 0.6, 0.62)),
        "olive": make_tint_mi(rust, "MI_Combat_CrateOlive", (0.5, 0.56, 0.32)),
        "dark_metal": make_tint_mi(rust, "MI_Combat_DarkMetal", (0.45, 0.45, 0.48)),
        "hazard_mesh": make_tint_mi(rust, "MI_Combat_HazardMesh", (1.0, 0.72, 0.08)),
        "emergency_red": make_mi(base, "MI_Combat_EmergencyRed", (0.9, 0.05, 0.05), roughness=0.2, emissive=(6.0, 0.2, 0.15)),
        "fire_glow": make_mi(base, "MI_Combat_FireGlow", (1.0, 0.4, 0.05), roughness=0.3, emissive=(4.5, 1.5, 0.1)),
        "toxic_glow": make_mi(base, "MI_Combat_ToxicGlow", (0.1, 0.45, 0.08), roughness=0.15, emissive=(0.2, 1.6, 0.15)),
        "zombie_slow": make_mi(base, "MI_Zombie_Slow", (0.18, 0.29, 0.14), roughness=0.85),
        "zombie_fast": make_mi(base, "MI_Zombie_Fast", (0.36, 0.10, 0.10), roughness=0.72),
    }
    mesh_mats = {
        "SM_Combat_BrokenWall": {"Concrete": mats["concrete"], "Rebar": mats["rebar"]},
        "SM_Combat_SandbagCluster": {"Sandbag": mats["sandbag"]},
        "SM_Combat_MetalBarricade": {"Metal": mats["dark_metal"], "Hazard": mats["hazard_mesh"]},
        "SM_Combat_Watchtower": {"Concrete": mats["concrete_dark"], "Metal": mats["rust_metal"]},
        "SM_Combat_DeadTree": {"Bark": mats["bark"]},
        "SM_Combat_SupplyCrate": {"Body": mats["olive"], "Frame": mats["dark_metal"]},
    }
    for name, (slots, build_fn) in SM_SPECS.items():
        table = mesh_mats[name]
        mesh = build_static_mesh(name, slots, [table[s] for s in slots], build_fn)
        SM_ASSETS[name] = (mesh, slots, [table[s] for s in slots])

    attack_snd = import_wav("SFX_ZombieAttack", looping=False)
    amb_snd = import_wav("SFX_ZombieAmbience", looping=True)

    zombie_mesh = unreal.load_asset("/Game/Dev_Blueprint/Characters/Zombie/SK_Zombie")
    mi_slow = unreal.load_asset("/Game/Dev_Blueprint/Characters/Zombie/Materials/MI_Zombie_Slow_Skin") or mats["zombie_slow"]
    mi_fast = unreal.load_asset("/Game/Dev_Blueprint/Characters/Zombie/Materials/MI_Zombie_Fast_Skin") or mats["zombie_fast"]
    anim_base = "/Game/Dev_Blueprint/Characters/Zombie/Animations"
    z_anims = {
        "idle1": unreal.load_asset(f"{anim_base}/Anim_Zombie_idle1"),
        "idle2": unreal.load_asset(f"{anim_base}/Anim_Zombie_idle2"),
        "walk": unreal.load_asset(f"{anim_base}/Anim_Zombie_walk"),
        "run": unreal.load_asset(f"{anim_base}/Anim_Zombie_run"),
        "atack1": unreal.load_asset(f"{anim_base}/Anim_Zombie_atack1"),
        "atack2": unreal.load_asset(f"{anim_base}/Anim_Zombie_atack2"),
        "death1": unreal.load_asset(f"{anim_base}/Anim_Zombie_death1"),
        "death2": unreal.load_asset(f"{anim_base}/Anim_Zombie_death2"),
    }
    log(f"Custom Zombie model loaded: mesh={zombie_mesh}, anims={sum(1 for v in z_anims.values() if v)}/8")

    bp_slow = create_zombie_blueprint(
        "BP_Zombie_Slow", hp=140.0, speed=140.0, ref_anim_speed=140.0, use_run_anim=False,
        damage=25.0, melee_range=165.0, cooldown=1.35, scale=1.08, skin_mat=mi_slow,
        attack_snd=attack_snd, zombie_mesh=zombie_mesh, anims_dict=z_anims,
        attack_key="atack1", death_key="death1", idle_key="idle1",
    )
    bp_fast = create_zombie_blueprint(
        "BP_Zombie_Fast", hp=70.0, speed=380.0, ref_anim_speed=380.0, use_run_anim=True,
        damage=12.0, melee_range=155.0, cooldown=0.75, scale=0.96, skin_mat=mi_fast,
        attack_snd=attack_snd, zombie_mesh=zombie_mesh, anims_dict=z_anims,
        attack_key="atack2", death_key="death2", idle_key="idle2",
    )

    if eal.does_asset_exist(LEVEL_PATH):
        eal.delete_asset(LEVEL_PATH)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

    gm_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode")
    if gm_bp:
        ws = world.get_world_settings()
        ws.set_editor_property("default_game_mode", gm_bp.generated_class())

    place(CUBE, (0, 0, -50), (80.0, 80.0, 1.0), mats["asphalt"], "Combat_Ground_Asphalt", "Combat/Terrain")

    nav = actor_sub.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
    if nav:
        nav.set_actor_scale3d(unreal.Vector(22.0, 24.0, 6.0))
        nav.set_actor_label("Combat_NavMeshBounds")
        nav.set_folder_path("Combat/AI")

    build_ruined_fortress(mats)
    build_arena_cover_and_barricades(mats)
    build_atmosphere_and_audio(amb_snd)
    place_spawners_pickups_and_starts(bp_slow.generated_class(), bp_fast.generated_class(), mats)

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log(f"Saved {LEVEL_PATH}: {ok}, actors={len(actor_sub.get_all_level_actors())}")


main()
