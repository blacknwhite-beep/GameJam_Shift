import os
import unreal

ICONS_DIR = r"C:\UnrealProjects\GameJam_Shift_2026\Scripts\InventoryIcons"
DEST_PATH = "/Game/Dev_Blueprint/UI/Icons"

ICON_FILES = [
    "T_Icon_Ammo_9mm.png",
    "T_Icon_Ammo_12G.png",
    "T_Icon_Ammo_556.png",
    "T_Icon_Medkit.png",
    "T_Icon_Landmine.png",
    "T_Icon_FarmPlot.png",
    "T_Icon_Weapon_Pistol.png",
    "T_Icon_Weapon_SMG.png",
    "T_Icon_Weapon_Rifle.png",
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
tex_factory = unreal.TextureFactory()
tasks = []

for fname in ICON_FILES:
    src = os.path.join(ICONS_DIR, fname)
    if not os.path.isfile(src):
        unreal.log_error(f"[InventoryIcons] Missing PNG: {src}")
        continue
    asset_name = os.path.splitext(fname)[0]
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = DEST_PATH
    task.destination_name = asset_name
    task.factory = tex_factory
    task.replace_existing = True
    task.automated = True
    task.save = False
    tasks.append(task)

asset_tools.import_asset_tasks(tasks)

for fname in ICON_FILES:
    asset_name = os.path.splitext(fname)[0]
    obj_path = f"{DEST_PATH}/{asset_name}.{asset_name}"
    tex = unreal.load_object(None, obj_path)
    if not tex:
        unreal.log_error(f"[InventoryIcons] Failed to load imported texture: {obj_path}")
        continue
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_asset(f"{DEST_PATH}/{asset_name}", only_if_is_dirty=False)
    unreal.log(f"[InventoryIcons] Saved UI Texture: {DEST_PATH}/{asset_name} ({tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()})")

unreal.log("[InventoryIcons] ALL ICONS IMPORTED SUCCESSFULLY.")
