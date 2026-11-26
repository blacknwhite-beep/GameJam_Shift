"""Assign deterministic daily unlock ranks to the placed Combat zombie spawners.

Run in UE 5.7 after building the new UnlockOrder property:
    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=Scripts/assign_zombie_spawner_unlock_order.py

The global order is converted at runtime into separate slow/fast sequences:
all slow spawners unlock on Night 1, while fast spawners unlock one per later
night. The C++ spawner also retains a numeric-name fallback for legacy actors.
"""

import re
import unreal

LEVEL_PATH = "/Game/Dev_Blueprint/Maps/Lvl_Combat"
SPAWNER_CLASS_PATH = "/Script/GameJam_Shift_2026.ShooterNPCSpawner"
LOG_PATH = r"C:\UnrealProjects\GameJam_Shift_2026\Saved\assign_zombie_spawner_unlock_order.log"


def log(message):
    text = "[SpawnerUnlock] " + message
    unreal.log(text)
    print(text, flush=True)
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as handle:
            handle.write(text + "\n")
    except Exception:
        pass


def natural_key(actor):
    return [int(part) if part.isdigit() else part.lower()
            for part in re.split(r"(\d+)", actor.get_name())]


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_editor.load_level(LEVEL_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    spawner_class = unreal.load_class(None, SPAWNER_CLASS_PATH)
    if not world or not spawner_class:
        raise RuntimeError("Could not load Combat level or ShooterNPCSpawner class")

    spawners = sorted(
        unreal.GameplayStatics.get_all_actors_of_class(world, spawner_class),
        key=natural_key,
    )
    if not spawners:
        raise RuntimeError("No ShooterNPCSpawner actors found in Lvl_Combat")

    for unlock_order, spawner in enumerate(spawners):
        spawner.set_editor_property("unlock_order", unlock_order)
        log(f"{spawner.get_name()} -> UnlockOrder {unlock_order}")

    level_editor.save_current_level()
    log(f"Assigned and saved {len(spawners)} spawner ranks")


try:
    main()
except Exception:
    import traceback
    log("FAILED:\n" + traceback.format_exc())
    raise
