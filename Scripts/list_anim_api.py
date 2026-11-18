"""Read-only: list animation-related Python API names available in this Unreal build."""
import unreal


def log(msg):
    unreal.log_warning(f"[ZOMBIE_API] {msg}")


def main():
    names = [n for n in dir(unreal.AnimationLibrary) if not n.startswith("_")]
    log("AnimationLibrary: " + ", ".join(names))
    seq_names = [n for n in dir(unreal.AnimSequence) if any(k in n.lower() for k in ("root", "bone", "pose", "frame", "key", "rate", "extract"))]
    log("AnimSequence: " + ", ".join(seq_names))


main()
