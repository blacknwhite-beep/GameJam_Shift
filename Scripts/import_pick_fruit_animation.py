"""
Imports "D:\\transfer\\projects\\animations\\Pick Fruit.fbx" onto the Mixamo T-Pose source skeleton
(SKM_Mixamo_Source from Falling.fbx BindPose), reuses IK_Mixamo_Source + IK_Farmer_Target +
RTG_MixamoToFarmer, retargets the clip onto SKM_Farmer_male as
/Game/Dev_Blueprint/Characters/Farmer/Animations/Anim_Farmer_PickFruit (with automatic
T-Pose -> Farmer A-Pose alignment), and logs Component-Space head/pelvis/hand poses across the
clip to verify the arms reach out to pick fruit.

PLAYBOOK (fw-boss commit 1538f30): MUST run via -ExecutePythonScript (NOT -run=pythonscript),
because IKRetargetBatchOperation.duplicate_and_retarget calls Slate NotifyUserOfResults which
asserts CurrentApplication.IsValid() under -run=pythonscript. auto_align_all_bones(TARGET) aligns
Mixamo T-Pose to the Farmer A-Pose. Do NOT pass -NullRHI.

  UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="<this file>" -nopause -unattended -nosound
"""
import unreal

PICK_FRUIT_FBX = r"D:\transfer\projects\animations\Pick Fruit.fbx"

FARMER_MESH_PATH = "/Game/3D_LOW_POLY_FarmerPack/Characters/Mesh/SKM_Farmer_male"

DEST_ROOT  = "/Game/Dev_Blueprint/Characters/Farmer"
MIXAMO_DIR = f"{DEST_ROOT}/MixamoSource"
ANIM_DIR   = f"{DEST_ROOT}/Animations"
SRC_ANIM   = f"{MIXAMO_DIR}/Anim_Mixamo_PickFruit"
FINAL_ANIM = f"{ANIM_DIR}/Anim_Farmer_PickFruit"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

for _cvar in ("Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.Texture 0"):
    unreal.SystemLibrary.execute_console_command(None, _cvar)


def log(msg):
    unreal.log_warning(f"[PICK_IMPORT] {msg}")


def import_mixamo_pick_anim(skel_mixamo):
    if eal.does_asset_exist(SRC_ANIM):
        existing = unreal.load_asset(SRC_ANIM)
        if isinstance(existing, unreal.AnimSequence):
            log(f"Loaded cached Anim_Mixamo_PickFruit: length={existing.get_play_length():.3f}s")
            return existing

    opts = unreal.FbxImportUI()
    opts.set_editor_property("automated_import_should_detect_type", False)
    opts.set_editor_property("import_mesh", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_animations", True)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    opts.set_editor_property("skeleton", skel_mixamo)
    opts.anim_sequence_import_data.set_editor_property("convert_scene", True)
    opts.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME
    )

    task = unreal.AssetImportTask()
    task.filename = PICK_FRUIT_FBX
    task.destination_path = MIXAMO_DIR
    task.destination_name = "Anim_Mixamo_PickFruit"
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.factory = unreal.FbxFactory()
    task.options = opts
    asset_tools.import_asset_tasks([task])

    seq = unreal.load_asset(SRC_ANIM)
    if seq:
        eal.save_loaded_asset(seq)
        log(f"Imported Anim_Mixamo_PickFruit: length={seq.get_play_length():.3f}s")
    return seq


def eval_chain_cs(seq, bone_chain, t):
    lib = unreal.AnimationLibrary
    cs = unreal.Transform()
    for b in bone_chain:
        local_tf = lib.get_bone_pose_for_time(seq, b, t, False)
        cs = unreal.MathLibrary.compose_transforms(local_tf, cs)
    return cs


def inspect_sequence_poses(seq, label, named_chains, sample_times):
    log(f"=== POSE CHECK: {label} ({seq.get_name()}, len={seq.get_play_length():.3f}s) ===")
    for t in sample_times:
        parts = []
        for name, chain in named_chains:
            tf = eval_chain_cs(seq, chain, t)
            loc = tf.translation
            rot = tf.rotation.rotator()
            parts.append(f"{name}=({loc.x:.1f},{loc.y:.1f},{loc.z:.1f} | P={rot.pitch:.0f},Y={rot.yaw:.0f},R={rot.roll:.0f})")
        log(f"  t={t:4.2f}s: " + "  ".join(parts))


def main():
    for d in (DEST_ROOT, MIXAMO_DIR, ANIM_DIR):
        eal.make_directory(d)

    sk_farmer = unreal.load_asset(FARMER_MESH_PATH)
    sk_mixamo = unreal.load_asset(f"{MIXAMO_DIR}/SKM_Mixamo_Source")
    ik_src = unreal.load_asset(f"{MIXAMO_DIR}/IK_Mixamo_Source")
    ik_tgt = unreal.load_asset(f"{MIXAMO_DIR}/IK_Farmer_Target")
    rtg = unreal.load_asset(f"{MIXAMO_DIR}/RTG_MixamoToFarmer")
    if not sk_farmer or not sk_mixamo or not ik_src or not ik_tgt or not rtg:
        log(f"ERROR: missing prerequisite - farmer={bool(sk_farmer)} mixamo={bool(sk_mixamo)} "
            f"ikSrc={bool(ik_src)} ikTgt={bool(ik_tgt)} rtg={bool(rtg)}. Run import_plant_animation.py first.")
        return

    skel_mixamo = sk_mixamo.get_editor_property("skeleton")
    mixamo_anim = import_mixamo_pick_anim(skel_mixamo)
    if not mixamo_anim:
        log("ERROR: Could not import Anim_Mixamo_PickFruit")
        return

    # Re-assert playbook alignment on the shared retargeter before generating the clip.
    # (Chain IK goals were already cleared on IK_Mixamo_Source / IK_Farmer_Target by import_plant_animation.py.)
    rctrl = unreal.IKRetargeterController.get_controller(rtg)
    rctrl.remove_all_ops()
    rctrl.add_default_ops()
    rctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    rctrl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)  # PLAYBOOK: T-Pose -> Farmer A-Pose
    rctrl.snap_bone_to_ground("ball_l", unreal.RetargetSourceOrTarget.TARGET)
    eal.save_loaded_asset(rtg)

    if eal.does_asset_exist(FINAL_ANIM):
        eal.delete_asset(FINAL_ANIM)

    src_asset_data = eal.find_asset_data(mixamo_anim.get_path_name())
    results = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        [src_asset_data],
        sk_mixamo,
        sk_farmer,
        rtg,
        "Anim_Mixamo_PickFruit",
        "Anim_Farmer_PickFruit",
        "",
        "",
        False,
        True,
    )
    log(f"duplicate_and_retarget returned {len(results)} asset(s): {[str(r.package_name) for r in results]}")
    if not results:
        log("ERROR: duplicate_and_retarget returned empty!")
        return

    gen_pkg = str(results[0].package_name)
    if gen_pkg != FINAL_ANIM:
        eal.rename_asset(gen_pkg, FINAL_ANIM)

    farmer_pick = unreal.load_asset(FINAL_ANIM)
    if not farmer_pick:
        log(f"ERROR: Could not load {FINAL_ANIM}")
        return
    eal.save_loaded_asset(farmer_pick)
    log(f"SUCCESS: Saved {FINAL_ANIM} (len={farmer_pick.get_play_length():.3f}s, "
        f"skel={farmer_pick.get_editor_property('skeleton').get_path_name()})")

    farmer_probe_chains = [
        ("pelvis", ["root", "pelvis"]),
        ("head",   ["root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head"]),
        ("hand_l", ["root", "pelvis", "spine_01", "spine_02", "spine_03", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"]),
        ("hand_r", ["root", "pelvis", "spine_01", "spine_02", "spine_03", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"]),
    ]
    len_s = farmer_pick.get_play_length()
    times = [round(len_s * f, 2) for f in (0.0, 0.2, 0.4, 0.6, 0.8, 0.99)]
    inspect_sequence_poses(farmer_pick, "FARMER_RETARGETED_PICK_FRUIT", farmer_probe_chains, times)


main()
