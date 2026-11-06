import unreal

print("=== STARTING TARKOV INVENTORY SYSTEM SETUP ===")

# -------------------------------------------------------------------------
# 1. Import UI Audio Assets
# -------------------------------------------------------------------------
audio_dir = "C:/Users/admin/OneDrive/Documents/Unreal Projects/GameJam_Shift_2026/Content/UI/Audio"
audio_files = [
    "UI_Inventory_Open.wav",
    "UI_Inventory_Close.wav",
    "UI_Item_Pickup.wav",
    "UI_Item_Drop.wav",
    "UI_Item_Rotate.wav"
]
imported_sounds = {}

for af in audio_files:
    asset_name = af.replace(".wav", "")
    dest_path = f"/Game/UI/Audio/{asset_name}"
    sound = unreal.load_asset(dest_path)
    if not sound:
        print(f"Importing {af}...")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", f"{audio_dir}/{af}")
        task.set_editor_property("destination_path", "/Game/UI/Audio")
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        asset_tools.import_asset_tasks([task])
        sound = unreal.load_asset(dest_path)
        print(f"Imported {asset_name}:", sound)
    else:
        print(f"Sound {asset_name} already exists:", sound)
    imported_sounds[asset_name] = sound

# -------------------------------------------------------------------------
# 2. Create IA_ToggleInventory & Map Tab + I in IMC_Weapons
# -------------------------------------------------------------------------
actions_dir = "/Game/Variant_Shooter/Input/Actions"
ia_inv_path = f"{actions_dir}/IA_ToggleInventory"
ia_inv = unreal.load_asset(ia_inv_path)
if not ia_inv:
    print("Creating IA_ToggleInventory...")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    ia_inv = asset_tools.create_asset("IA_ToggleInventory", actions_dir, unreal.InputAction, None)
    if ia_inv:
        unreal.EditorAssetLibrary.save_loaded_asset(ia_inv)
        print("Created IA_ToggleInventory:", ia_inv)
else:
    print("IA_ToggleInventory exists:", ia_inv)

# Map into IMC_Weapons
imc_path = "/Game/Variant_Shooter/Input/IMC_Weapons"
imc = unreal.load_asset(imc_path)
if imc and ia_inv:
    mappings = list(imc.get_editor_property("mappings"))
    existing_keys = [str(m.get_editor_property("key").get_editor_property("key_name")) for m in mappings if m.get_editor_property("action") == ia_inv]
    print("Existing ToggleInventory keys in IMC_Weapons:", existing_keys)
    
    dirty = False
    for k_name in ["Tab", "I"]:
        if k_name not in existing_keys:
            k = unreal.Key()
            k.set_editor_property("key_name", k_name)
            m = unreal.EnhancedActionKeyMapping()
            m.set_editor_property("action", ia_inv)
            m.set_editor_property("key", k)
            mappings.append(m)
            dirty = True
            print(f"Added {k_name} mapping for IA_ToggleInventory")
            
    if dirty:
        imc.set_editor_property("mappings", mappings)
        unreal.EditorAssetLibrary.save_loaded_asset(imc)
        print("Saved updated IMC_Weapons")

# -------------------------------------------------------------------------
# 3. Create WBP_TarkovInventory Widget Blueprint
# -------------------------------------------------------------------------
ui_dir = "/Game/UI"
wbp_inv_path = f"{ui_dir}/WBP_TarkovInventory"
wbp_inv = unreal.load_asset(wbp_inv_path)
if not wbp_inv:
    print("Creating WBP_TarkovInventory...")
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.UserWidget)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    wbp_inv = asset_tools.create_asset("WBP_TarkovInventory", ui_dir, unreal.WidgetBlueprint, factory)
    if wbp_inv:
        unreal.EditorAssetLibrary.save_loaded_asset(wbp_inv)
        print("Created WBP_TarkovInventory:", wbp_inv)
else:
    print("WBP_TarkovInventory exists:", wbp_inv)

# -------------------------------------------------------------------------
# 4. Configure BP_ShooterCharacter CDO
# -------------------------------------------------------------------------
bp_char_path = "/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"
bp_char = unreal.load_asset(bp_char_path)
bp_pistol_path = "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol"
bp_pistol = unreal.load_asset(bp_pistol_path)

if bp_char and bp_pistol:
    cdo_char = unreal.get_default_object(bp_char.generated_class())
    
    # Set InitialWeaponClass to Pistol Generated Class
    cdo_char.set_editor_property("initial_weapon_class", bp_pistol.generated_class())
    print("Set BP_ShooterCharacter.initial_weapon_class -> BP_ShooterWeapon_Pistol_C")
    
    # Set ToggleInventoryAction
    if ia_inv:
        cdo_char.set_editor_property("toggle_inventory_action", ia_inv)
        print("Set BP_ShooterCharacter.toggle_inventory_action -> IA_ToggleInventory")
        
    unreal.EditorAssetLibrary.save_loaded_asset(bp_char)
    print("Saved BP_ShooterCharacter!")

# -------------------------------------------------------------------------
# 5. Configure WorldSettings on Lvl_FirstPerson to use BP_ShooterGameMode
# -------------------------------------------------------------------------
gm_shooter_path = "/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode"
gm_shooter = unreal.load_asset(gm_shooter_path)
gm_shooter_class = gm_shooter.generated_class() if gm_shooter else None

# Check current world
world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    ws = world.get_world_settings()
    if ws and gm_shooter_class:
        ws.set_editor_property("default_game_mode", gm_shooter_class)
        print(f"Set {world.get_name()} WorldSettings.default_game_mode -> BP_ShooterGameMode_C")
        unreal.EditorLevelLibrary.save_current_level()
        print("Saved current level!")

# Also load Lvl_FirstPerson and set GameMode explicitly
unreal.EditorLevelLibrary.load_level("/Game/FirstPerson/Lvl_FirstPerson")
world_fp = unreal.EditorLevelLibrary.get_editor_world()
if world_fp and gm_shooter_class:
    ws_fp = world_fp.get_world_settings()
    if ws_fp:
        ws_fp.set_editor_property("default_game_mode", gm_shooter_class)
        unreal.EditorLevelLibrary.save_current_level()
        print("Configured and saved Lvl_FirstPerson with BP_ShooterGameMode!")

print("=== TARKOV INVENTORY SYSTEM SETUP COMPLETE ===")
