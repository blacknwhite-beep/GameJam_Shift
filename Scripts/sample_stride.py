"""Read-only: sample foot/hips bone translations across the walk and run clips to estimate stride."""
import unreal

BASE = "/Game/Dev_Blueprint/Characters/Zombie/Animations"


def log(msg):
    unreal.log_warning(f"[ZOMBIE_STRIDE] {msg}")


def main():
    lib = unreal.AnimationLibrary
    for clip in ("walk", "run"):
        seq = unreal.load_asset(f"{BASE}/Anim_Zombie_{clip}")
        length = seq.get_play_length()
        log(f"{clip}: length={length:.3f}s")
        for bone in ("Zombie_LeftUpLeg", "Zombie_LeftLeg", "Zombie_RightUpLeg", "Zombie_RightLeg"):
            pts = []
            n = 12
            for i in range(n + 1):
                t = length * i / n
                tf = lib.get_bone_pose_for_time(seq, bone, t, False)
                e = tf.rotation.rotator()
                pts.append((round(e.pitch, 1), round(e.yaw, 1), round(e.roll, 1)))
            log(f"{clip} {bone} rot(p,y,r): " + " | ".join(f"({p},{y},{r})" for p, y, r in pts))


main()
