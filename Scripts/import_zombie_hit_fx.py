import os
import unreal

SCRIPTS_DIR = r"D:\transfer\projects\GameJam projects\GameJam_Shift\Scripts"
AUDIO_DEST = "/Game/Dev_Blueprint/Audio"
UI_DEST = "/Game/Dev_Blueprint/UI"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

for _cvar in ("Interchange.FeatureFlags.Import.Texture 0",):
    unreal.SystemLibrary.execute_console_command(None, _cvar)

# 1. Import SFX_ZombieBiteHit
wav_task = unreal.AssetImportTask()
wav_task.filename = os.path.join(SCRIPTS_DIR, "SFX_ZombieBiteHit.wav")
wav_task.destination_path = AUDIO_DEST
wav_task.destination_name = "SFX_ZombieBiteHit"
wav_task.automated = True
wav_task.save = True
wav_task.replace_existing = True
asset_tools.import_asset_tasks([wav_task])

snd = unreal.load_asset(f"{AUDIO_DEST}/SFX_ZombieBiteHit")
if snd:
    snd.set_editor_property("looping", False)
    eal.save_loaded_asset(snd)
    unreal.log_warning(f"[HIT_FX] Imported {AUDIO_DEST}/SFX_ZombieBiteHit (duration={snd.get_editor_property('duration'):.2f}s)")

# 2. Import T_BloodDamageOverlay PNG texture (UI texture group, sRGB, full alpha)
tex_task = unreal.AssetImportTask()
tex_task.filename = os.path.join(SCRIPTS_DIR, "T_BloodDamageOverlay.png")
tex_task.destination_path = UI_DEST
tex_task.destination_name = "T_BloodDamageOverlay"
tex_task.automated = True
tex_task.save = True
tex_task.replace_existing = True
tex_task.factory = unreal.TextureFactory()
asset_tools.import_asset_tasks([tex_task])

tex = unreal.load_asset(f"{UI_DEST}/T_BloodDamageOverlay")
if tex:
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("srgb", True)
    eal.save_loaded_asset(tex)
    unreal.log_warning(f"[HIT_FX] Imported {UI_DEST}/T_BloodDamageOverlay")

# 3. Inspect UI_ShooterBulletCounter widget tree to see where the orange hit outline lives
wbp = unreal.load_asset("/Game/Variant_Shooter/UI/UI_ShooterBulletCounter")
if wbp:
    unreal.log_warning(f"[HIT_FX] Loaded UI_ShooterBulletCounter: {wbp.get_class().get_name()}")
