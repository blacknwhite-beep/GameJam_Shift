"""Read-only: confirm the Zombie Blueprint CDOs expose the new C++ properties and list zombie actors in Lvl_Combat."""
import unreal


def log(msg):
    unreal.log_warning(f"[ZOMBIE_RUNTIME] {msg}")


def main():
    for bp_name in ("BP_Zombie_Slow", "BP_Zombie_Fast"):
        bp = unreal.load_asset(f"/Game/Dev_Blueprint/Blueprints/Enemies/{bp_name}")
        if not bp:
            log(f"{bp_name} MISSING")
            continue
        cdo = unreal.get_default_object(bp.generated_class())
        try:
            dpr = cdo.get_editor_property("DeathPlayRate")
        except Exception as e:
            dpr = f"not present ({e})"
        log(f"{bp_name}: DeathPlayRate={dpr} ReferenceAnimSpeed={cdo.get_editor_property('ReferenceAnimSpeed')} ZombieMoveSpeed={cdo.get_editor_property('ZombieMoveSpeed')}")

    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Dev_Blueprint/Maps/Lvl_Combat")
    world = unreal.EditorLevelLibrary.get_editor_world()
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name() == "ShooterNPCSpawner":
            npc = actor.get_editor_property("NPCClass")
            log(f"{actor.get_name()}: NPCClass={npc.get_path_name() if npc else None}")


main()
