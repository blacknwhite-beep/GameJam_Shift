"""
Imports "D:\\transfer\\projects\\animations\\Plant A Plant.fbx" onto a Mixamo T-Pose source skeleton
(from Falling.fbx BindPose), builds IK_Mixamo_Source + IK_Farmer_Target + RTG_MixamoToFarmer with
automatic T-Pose -> Farmer A-Pose bone alignment, retargets the animation onto SKM_Farmer_male as
/Game/Dev_Blueprint/Characters/Farmer/Animations/Anim_Farmer_Plant, and logs exact Component-Space
bone poses (head, pelvis, hand_l, hand_r, foot_l, foot_r) across the entire clip to verify the arms
and head camera motion!

Run headless (do NOT pass -NullRHI):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nopause -unattended -nosound
"""
import os
import unreal

MIXAMO_SKIN_FBX = r"D:\transfer\projects\animations\Falling.fbx"
PLANT_ANIM_FBX  = r"D:\transfer\projects\animations\Plant A Plant.fbx"

FARMER_MESH_PATH = "/Game/3D_LOW_POLY_FarmerPack/Characters/Mesh/SKM_Farmer_male"
FARMER_IDLE_PATH = "/Game/3D_LOW_POLY_FarmerPack/Animations/Farmer/anim_Farmer_idle_basic"

DEST_ROOT   = "/Game/Dev_Blueprint/Characters/Farmer"
MIXAMO_DIR  = f"{DEST_ROOT}/MixamoSource"
ANIM_DIR    = f"{DEST_ROOT}/Animations"
FINAL_ANIM  = f"{ANIM_DIR}/Anim_Farmer_Plant"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

for _cvar in ("Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.Texture 0"):
    unreal.SystemLibrary.execute_console_command(None, _cvar)


def log(msg):
    unreal.log_warning(f"[PLANT_IMPORT] {msg}")


def import_mixamo_source_mesh():
    mesh_path = f"{MIXAMO_DIR}/SKM_Mixamo_Source"
    if eal.does_asset_exist(mesh_path):
        sk = unreal.load_asset(mesh_path)
        skel = sk.get_editor_property("skeleton") if sk else None
        if sk and skel:
            log(f"Loaded cached SKM_Mixamo_Source: skel={skel.get_path_name()}")
            return sk, skel

    opts = unreal.FbxImportUI()
    opts.set_editor_property("automated_import_should_detect_type", False)
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    opts.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", False)
    opts.skeletal_mesh_import_data.set_editor_property("convert_scene", True)

    task = unreal.AssetImportTask()
    task.filename = MIXAMO_SKIN_FBX
    task.destination_path = MIXAMO_DIR
    task.destination_name = "SKM_Mixamo_Source"
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.factory = unreal.FbxFactory()
    task.options = opts
    asset_tools.import_asset_tasks([task])

    sk = unreal.load_asset(mesh_path)
    skel = sk.get_editor_property("skeleton") if sk else None
    if sk:
        eal.save_loaded_asset(sk)
    if skel:
        eal.save_loaded_asset(skel)
    log(f"Imported SKM_Mixamo_Source: sk={sk}, skel={skel}")
    return sk, skel


def import_mixamo_plant_anim(skel):
    anim_path = f"{MIXAMO_DIR}/Anim_Mixamo_PlantAPlant"
    if eal.does_asset_exist(anim_path):
        existing = unreal.load_asset(anim_path)
        if isinstance(existing, unreal.AnimSequence):
            log(f"Loaded cached Anim_Mixamo_PlantAPlant: length={existing.get_play_length():.3f}s")
            return existing

    opts = unreal.FbxImportUI()
    opts.set_editor_property("automated_import_should_detect_type", False)
    opts.set_editor_property("import_mesh", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_animations", True)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    opts.set_editor_property("skeleton", skel)
    opts.anim_sequence_import_data.set_editor_property("convert_scene", True)
    opts.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME
    )

    task = unreal.AssetImportTask()
    task.filename = PLANT_ANIM_FBX
    task.destination_path = MIXAMO_DIR
    task.destination_name = "Anim_Mixamo_PlantAPlant"
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.factory = unreal.FbxFactory()
    task.options = opts
    asset_tools.import_asset_tasks([task])

    seq = unreal.load_asset(anim_path)
    if seq:
        eal.save_loaded_asset(seq)
        log(f"Imported Anim_Mixamo_PlantAPlant: length={seq.get_play_length():.3f}s")
    return seq


def build_ik_rig(pkg_path, asset_name, skel_mesh):
    full_path = f"{pkg_path}/{asset_name}"
    if eal.does_asset_exist(full_path):
        rig = unreal.load_asset(full_path)
    else:
        rig = unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(pkg_path, asset_name)
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(skel_mesh)
    auto_ok = ctrl.apply_auto_generated_retarget_definition()
    chains_info = [
        (str(c.chain_name), str(ctrl.get_retarget_chain_start_bone(c.chain_name)), str(ctrl.get_retarget_chain_end_bone(c.chain_name)))
        for c in ctrl.get_retarget_chains()
    ]
    log(f"{asset_name}: auto_ok={auto_ok}, root={ctrl.get_retarget_root()}, chains({len(chains_info)})={chains_info}")

    # Remove any IK goals on chains so batch retargeting uses clean aligned FK+Pelvis without unconfigured FBIK pinning
    for cname, _, _ in chains_info:
        ctrl.set_retarget_chain_goal(cname, "None")

    eal.save_loaded_asset(rig)
    return rig


def build_retargeter(src_rig, tgt_rig, src_mesh, tgt_mesh):
    rtg_path = f"{MIXAMO_DIR}/RTG_MixamoToFarmer"
    if eal.does_asset_exist(rtg_path):
        rtg = unreal.load_asset(rtg_path)
    else:
        rtg = asset_tools.create_asset("RTG_MixamoToFarmer", MIXAMO_DIR, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rctrl = unreal.IKRetargeterController.get_controller(rtg)
    rctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    rctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    rctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, src_mesh)
    rctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, tgt_mesh)

    rctrl.remove_all_ops()
    rctrl.add_default_ops()
    rctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)

    # Automatically align Farmer's A-Pose shoulders, arms, forearms, hands, legs, and fingers to Mixamo's T-Pose!
    rctrl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    rctrl.snap_bone_to_ground("ball_l", unreal.RetargetSourceOrTarget.TARGET)

    eal.save_loaded_asset(rtg)
    log(f"Built & aligned RTG_MixamoToFarmer (ops={rctrl.get_num_retarget_ops()})")
    return rtg


def eval_chain_cs(seq, bone_chain, t):
    lib = unreal.AnimationLibrary
    cs = unreal.Transform()
    for b in bone_chain:
        local_tf = lib.get_bone_pose_for_time(seq, b, t, False)
        # In Unreal, child_cs = local_tf * parent_cs (compose_transforms(A=local, B=parent))
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
    if not sk_farmer:
        log(f"ERROR: Could not load {FARMER_MESH_PATH}")
        return

    sk_mixamo, skel_mixamo = import_mixamo_source_mesh()
    if not sk_mixamo or not skel_mixamo:
        log("ERROR: Could not import SKM_Mixamo_Source")
        return

    mixamo_anim = import_mixamo_plant_anim(skel_mixamo)
    if not mixamo_anim:
        log("ERROR: Could not import Anim_Mixamo_PlantAPlant")
        return

    ik_src = build_ik_rig(MIXAMO_DIR, "IK_Mixamo_Source", sk_mixamo)
    ik_tgt = build_ik_rig(MIXAMO_DIR, "IK_Farmer_Target", sk_farmer)
    rtg = build_retargeter(ik_src, ik_tgt, sk_mixamo, sk_farmer)

    if eal.does_asset_exist(FINAL_ANIM):
        eal.delete_asset(FINAL_ANIM)

    src_asset_data = eal.find_asset_data(mixamo_anim.get_path_name())
    results = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        [src_asset_data],
        sk_mixamo,
        sk_farmer,
        rtg,
        "Anim_Mixamo_PlantAPlant",
        "Anim_Farmer_Plant",
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

    farmer_plant = unreal.load_asset(FINAL_ANIM)
    if not farmer_plant:
        log(f"ERROR: Could not load {FINAL_ANIM}")
        return

    eal.save_loaded_asset(farmer_plant)
    log(f"SUCCESS: Saved {FINAL_ANIM} (len={farmer_plant.get_play_length():.3f}s, skel={farmer_plant.get_editor_property('skeleton').get_path_name()})")

    farmer_probe_chains = [
        ("pelvis", ["root", "pelvis"]),
        ("head",   ["root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head"]),
        ("hand_l", ["root", "pelvis", "spine_01", "spine_02", "spine_03", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"]),
        ("hand_r", ["root", "pelvis", "spine_01", "spine_02", "spine_03", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"]),
        ("foot_l", ["root", "pelvis", "thigh_l", "calf_l", "foot_l"]),
        ("foot_r", ["root", "pelvis", "thigh_r", "calf_r", "foot_r"]),
    ]
    pfx = "mixamorig:" if unreal.AnimationLibrary.does_bone_name_exist(mixamo_anim, "mixamorig:Hips") else ""
    mixamo_probe_chains = [
        ("Hips",      [f"{pfx}Hips"]),
        ("Head",      [f"{pfx}Hips", f"{pfx}Spine", f"{pfx}Spine1", f"{pfx}Spine2", f"{pfx}Neck", f"{pfx}Head"]),
        ("LeftHand",  [f"{pfx}Hips", f"{pfx}Spine", f"{pfx}Spine1", f"{pfx}Spine2", f"{pfx}LeftShoulder", f"{pfx}LeftArm", f"{pfx}LeftForeArm", f"{pfx}LeftHand"]),
        ("RightHand", [f"{pfx}Hips", f"{pfx}Spine", f"{pfx}Spine1", f"{pfx}Spine2", f"{pfx}RightShoulder", f"{pfx}RightArm", f"{pfx}RightForeArm", f"{pfx}RightHand"]),
        ("LeftFoot",  [f"{pfx}Hips", f"{pfx}LeftUpLeg", f"{pfx}LeftLeg", f"{pfx}LeftFoot"]),
        ("RightFoot", [f"{pfx}Hips", f"{pfx}RightUpLeg", f"{pfx}RightLeg", f"{pfx}RightFoot"]),
    ]

    idle_seq = unreal.load_asset(FARMER_IDLE_PATH)
    if idle_seq:
        inspect_sequence_poses(idle_seq, "FARMER_IDLE", farmer_probe_chains, [0.0])

    inspect_sequence_poses(mixamo_anim, "MIXAMO_SOURCE_PLANT", mixamo_probe_chains, [0.0, 1.2, 2.0, 3.0, 4.0, 5.0, 6.0, 7.5])
    inspect_sequence_poses(farmer_plant, "FARMER_RETARGETED_PLANT", farmer_probe_chains, [0.0, 1.2, 2.0, 3.0, 4.0, 5.0, 6.0, 7.5])


main()
