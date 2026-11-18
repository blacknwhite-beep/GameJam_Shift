"""Read-only: probe bone-pose API signature and list foot/root bones on the zombie skeleton."""
import unreal

BASE = "/Game/Dev_Blueprint/Characters/Zombie/Animations"


def log(msg):
    unreal.log_warning(f"[ZOMBIE_PROBE] {msg}")


def main():
    lib = unreal.AnimationLibrary
    seq = unreal.load_asset(f"{BASE}/Anim_Zombie_walk")
    skel = seq.get_editor_property("skeleton")
    log(f"skeleton={skel.get_path_name() if skel else None}")

    candidates = ["root", "pelvis", "Hips", "mixamorig:Hips", "spine_01", "foot_l", "foot_r",
                  "foot_L", "foot_R", "LeftFoot", "RightFoot", "mixamorig:LeftFoot", "mixamorig:RightFoot",
                  "ball_l", "ball_r", "calf_l", "calf_r", "thigh_l", "thigh_r"]
    found = [b for b in candidates if lib.does_bone_name_exist(seq, b)]
    log(f"bones found: {found}")

    try:
        t = lib.get_bone_pose_for_time(seq, found[0] if found else "root", 0.0)
        log(f"get_bone_pose_for_time(seq,bone,time) OK -> {t}")
    except Exception as e:
        log(f"get_bone_pose_for_time(seq,bone,time) ERR: {e}")

    try:
        r = lib.get_bone_pose_for_time(seq, "root", 0.0, False)
        log(f"get_bone_pose_for_time(...,False) OK -> {r}")
    except Exception as e:
        log(f"get_bone_pose_for_time 4-arg ERR: {e}")


main()
