"""Read-only: report zombie animation lengths, frame counts, and root travel distance."""
import unreal

NAMES = ["idle1", "walk", "run", "atack1", "death1", "death2"]
BASE = "/Game/Dev_Blueprint/Characters/Zombie/Animations"


def log(msg):
    unreal.log_warning(f"[ZOMBIE_ANIM] {msg}")


def main():
    lib = unreal.AnimationLibrary
    for n in NAMES:
        seq = unreal.load_asset(f"{BASE}/Anim_Zombie_{n}")
        if not seq:
            log(f"{n}: MISSING")
            continue
        length = seq.get_play_length()
        frames = "n/a"
        try:
            a = lib.get_bone_transform_at_time(seq, "root", 0.0)
            b = lib.get_bone_transform_at_time(seq, "root", length)
            travel = (b.translation - a.translation).size()
        except Exception as e:
            travel = f"unavailable ({e})"
        log(f"{n}: length={length:.3f}s frames={frames} root_travel={travel}")


main()
