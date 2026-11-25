"""
Reusable UE 5.7 Mixamo FBX -> Target SkeletalMesh IK Retargeting template
(exact pipeline used for GameJam_Shift_2026 Anim_Farmer_Plant).

Run with -ExecutePythonScript (NEVER -run=pythonscript, which asserts Slate on DuplicateAndRetarget):
  UnrealEditor-Cmd.exe "<uproject>" -ExecutePythonScript="<this_script.py>" -nopause -unattended -nosound
"""
import unreal

MIXAMO_SKIN_FBX  = r"D:\transfer\projects\animations\Falling.fbx"
MIXAMO_ANIM_FBX  = r"D:\transfer\projects\animations\Plant A Plant.fbx"
TARGET_MESH_PATH = "/Game/3D_LOW_POLY_FarmerPack/Characters/Mesh/SKM_Farmer_male"

DEST_ROOT       = "/Game/Dev_Blueprint/Characters/Farmer"
MIXAMO_DIR      = f"{DEST_ROOT}/MixamoSource"
ANIM_DIR        = f"{DEST_ROOT}/Animations"
SRC_ANIM_NAME   = "Anim_Mixamo_PlantAPlant"
FINAL_ANIM_NAME = "Anim_Farmer_Plant"
FINAL_ANIM_PATH = f"{ANIM_DIR}/{FINAL_ANIM_NAME}"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

for _cvar in ("Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.Texture 0"):
    unreal.SystemLibrary.execute_console_command(None, _cvar)


def log(msg):
    unreal.log_warning(f"[MIXAMO_RETARGET] {msg}")


def import_mixamo_source_mesh():
    mesh_path = f"{MIXAMO_DIR}/SKM_Mixamo_Source"
    if eal.does_asset_exist(mesh_path):
        sk = unreal.load_asset(mesh_path)
        skel = sk.get_editor_property("skeleton") if sk else None
        if sk and skel:
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
    return sk, skel


def import_mixamo_anim(skel):
    anim_path = f"{MIXAMO_DIR}/{SRC_ANIM_NAME}"
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
    task.filename = MIXAMO_ANIM_FBX
    task.destination_path = MIXAMO_DIR
    task.destination_name = SRC_ANIM_NAME
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.factory = unreal.FbxFactory()
    task.options = opts
    asset_tools.import_asset_tasks([task])

    seq = unreal.load_asset(anim_path)
    if seq:
        eal.save_loaded_asset(seq)
    return seq


def build_ik_rig(pkg_path, asset_name, skel_mesh):
    full_path = f"{pkg_path}/{asset_name}"
    rig = unreal.load_asset(full_path) if eal.does_asset_exist(full_path) else unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(pkg_path, asset_name)
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(skel_mesh)
    ctrl.apply_auto_generated_retarget_definition()
    for c in ctrl.get_retarget_chains():
        ctrl.set_retarget_chain_goal(str(c.chain_name), "None")
    eal.save_loaded_asset(rig)
    return rig


def build_retargeter(src_rig, tgt_rig, src_mesh, tgt_mesh):
    rtg_path = f"{MIXAMO_DIR}/RTG_MixamoToTarget"
    rtg = unreal.load_asset(rtg_path) if eal.does_asset_exist(rtg_path) else asset_tools.create_asset("RTG_MixamoToTarget", MIXAMO_DIR, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rctrl = unreal.IKRetargeterController.get_controller(rtg)
    rctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    rctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    rctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, src_mesh)
    rctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, tgt_mesh)
    rctrl.remove_all_ops()
    rctrl.add_default_ops()
    rctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    rctrl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    rctrl.snap_bone_to_ground("ball_l", unreal.RetargetSourceOrTarget.TARGET)
    eal.save_loaded_asset(rtg)
    return rtg


def main():
    for d in (DEST_ROOT, MIXAMO_DIR, ANIM_DIR):
        eal.make_directory(d)

    sk_target = unreal.load_asset(TARGET_MESH_PATH)
    sk_mixamo, skel_mixamo = import_mixamo_source_mesh()
    mixamo_anim = import_mixamo_anim(skel_mixamo)

    ik_src = build_ik_rig(MIXAMO_DIR, "IK_Mixamo_Source", sk_mixamo)
    ik_tgt = build_ik_rig(MIXAMO_DIR, "IK_Target", sk_target)
    rtg = build_retargeter(ik_src, ik_tgt, sk_mixamo, sk_target)

    if eal.does_asset_exist(FINAL_ANIM_PATH):
        eal.delete_asset(FINAL_ANIM_PATH)

    src_data = eal.find_asset_data(mixamo_anim.get_path_name())
    results = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        [src_data], sk_mixamo, sk_target, rtg, SRC_ANIM_NAME, FINAL_ANIM_NAME, "", "", False, True
    )
    if results:
        gen_pkg = str(results[0].package_name)
        if gen_pkg != FINAL_ANIM_PATH:
            eal.rename_asset(gen_pkg, FINAL_ANIM_PATH)
        out_anim = unreal.load_asset(FINAL_ANIM_PATH)
        if out_anim:
            eal.save_loaded_asset(out_anim)
            log(f"SUCCESS: {FINAL_ANIM_PATH} (len={out_anim.get_play_length():.3f}s)")


main()
