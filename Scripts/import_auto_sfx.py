import os
import unreal

SCRIPTS_DIR = r"D:\transfer\projects\GameJam projects\GameJam_Shift\Scripts"
AUDIO_DEST = "/Game/Dev_Blueprint/Audio"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

for wav_name in ("SFX_Auto_MP5", "SFX_Auto_MP7", "SFX_Auto_M4A1", "SFX_Auto_M16", "SFX_Auto_SCAR"):
    wav_path = os.path.join(SCRIPTS_DIR, f"{wav_name}.wav")
    task = unreal.AssetImportTask()
    task.filename = wav_path
    task.destination_path = AUDIO_DEST
    task.destination_name = wav_name
    task.automated = True
    task.save = True
    task.replace_existing = True
    asset_tools.import_asset_tasks([task])

    snd = unreal.load_asset(f"{AUDIO_DEST}/{wav_name}")
    if snd:
        snd.set_editor_property("looping", True)
        eal.save_loaded_asset(snd)
        unreal.log_warning(f"[AUTO_SFX] Imported {AUDIO_DEST}/{wav_name} (looping=True, duration={snd.get_editor_property('duration'):.2f}s)")
    else:
        unreal.log_error(f"[AUTO_SFX] Failed to import {wav_name}")
