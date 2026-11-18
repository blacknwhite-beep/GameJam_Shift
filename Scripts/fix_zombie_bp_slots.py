"""Set every SK_Zombie slot on BP_Zombie_Slow / BP_Zombie_Fast to that zombie's own skin MI, then compile and save."""
import unreal

eal = unreal.EditorAssetLibrary
ENEMY_DIR = "/Game/Dev_Blueprint/Blueprints/Enemies"
MAT_DIR = "/Game/Dev_Blueprint/Characters/Zombie/Materials"


def log(msg):
    unreal.log_warning(f"[ZOMBIE_BPFIX] {msg}")


def main():
    for bp_name, mi_name in (("BP_Zombie_Slow", "MI_Zombie_Slow_Skin"), ("BP_Zombie_Fast", "MI_Zombie_Fast_Skin")):
        bp = unreal.load_asset(f"{ENEMY_DIR}/{bp_name}")
        mi = unreal.load_asset(f"{MAT_DIR}/{mi_name}")
        if not bp or not mi:
            log(f"ERROR: missing {bp_name} or {mi_name}")
            continue
        cdo = unreal.get_default_object(bp.generated_class())
        mesh = cdo.get_editor_property("Mesh")
        count = mesh.get_num_materials()
        mesh.set_editor_property("override_materials", [mi] * count)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        eal.save_loaded_asset(bp)
        names = [m.get_name() if m else "NONE" for m in mesh.get_materials()]
        log(f"{bp_name}: {count} slots -> {mi_name}; now {names}")
    log("DONE")


main()
