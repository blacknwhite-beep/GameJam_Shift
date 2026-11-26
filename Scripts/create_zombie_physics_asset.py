"""Create and assign a ragdoll Physics Asset for the zombie skeletal mesh.

Run in UE 5.7 with:
    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=Scripts/create_zombie_physics_asset.py

SK_Zombie was imported without physics geometry, so SetSimulatePhysics alone could
not produce a real ragdoll. This uses the editor's normal FBX-style body/constraint
generation and assigns the result to the shared zombie mesh used by both variants.
"""

import unreal

MESH_PATH = "/Game/Dev_Blueprint/Characters/Zombie/SK_Zombie.SK_Zombie"
LOG_PATH = r"C:\UnrealProjects\GameJam_Shift_2026\Saved\create_zombie_physics_asset.log"


def log(message):
    text = "[ZombiePhysics] " + message
    unreal.log(text)
    print(text, flush=True)
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as handle:
            handle.write(text + "\n")
    except Exception:
        pass


def main():
    mesh = unreal.load_asset(MESH_PATH)
    if not mesh:
        raise RuntimeError("Could not load zombie skeletal mesh: " + MESH_PATH)

    physics_asset = mesh.get_editor_property("physics_asset")
    if not physics_asset:
        physics_asset = unreal.SkeletalMeshEditorSubsystem.create_physics_asset(mesh, True, 0)
        if not physics_asset:
            raise RuntimeError("UE failed to generate a Physics Asset for SK_Zombie")
        log("Generated " + physics_asset.get_path_name())
    else:
        log("Using existing " + physics_asset.get_path_name())

    if not unreal.SkeletalMeshEditorSubsystem.assign_physics_asset(mesh, physics_asset):
        raise RuntimeError("Could not assign the Physics Asset to SK_Zombie")

    unreal.EditorAssetLibrary.save_loaded_asset(physics_asset, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    log("Assigned and saved zombie ragdoll Physics Asset")


try:
    main()
except Exception:
    import traceback
    log("FAILED:\n" + traceback.format_exc())
    raise
