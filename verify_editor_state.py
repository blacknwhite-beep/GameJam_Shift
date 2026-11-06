import unreal

def log(msg):
    unreal.log(f"[VERIFY] {msg}")
    print(f"[VERIFY] {msg}")

log("Starting in-editor verification...")

# 1. Check BP_ShooterCharacter CDO
char_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
if not char_bp:
    log("ERROR: Could not load BP_ShooterCharacter")
else:
    cdo = unreal.get_default_object(char_bp.generated_class())
    initial_weapon = cdo.get_editor_property("initial_weapon_class")
    log(f"BP_ShooterCharacter.initial_weapon_class = {initial_weapon}")
    
    toggle_action = cdo.get_editor_property("toggle_inventory_action")
    log(f"BP_ShooterCharacter.toggle_inventory_action = {toggle_action}")
    
    # If not set, let's configure them right now and save
    needs_save = False
    if not initial_weapon:
        pistol_class = unreal.load_asset("/Game/Variant_Shooter/Blueprints/Weapons/BP_ShooterWeapon_Pistol")
        if pistol_class:
            cdo.set_editor_property("initial_weapon_class", pistol_class.generated_class())
            log("Configured initial_weapon_class to BP_ShooterWeapon_Pistol")
            needs_save = True
            
    if not toggle_action:
        ia_toggle = unreal.load_asset("/Game/Variant_Shooter/Input/Actions/IA_ToggleInventory")
        if ia_toggle:
            cdo.set_editor_property("toggle_inventory_action", ia_toggle)
            log("Configured toggle_inventory_action to IA_ToggleInventory")
            needs_save = True

    if needs_save:
        unreal.EditorAssetLibrary.save_loaded_asset(char_bp)
        log("Saved BP_ShooterCharacter with updated properties")

# 2. Check WorldSettings of current editor world / Lvl_FirstPerson
editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor_subsystem.get_editor_world()
if world:
    ws = world.get_world_settings()
    gm = ws.get_editor_property("default_game_mode")
    log(f"WorldSettings.default_game_mode = {gm}")
    shooter_gm = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode")
    if shooter_gm and gm != shooter_gm.generated_class():
        ws.set_editor_property("default_game_mode", shooter_gm.generated_class())
        log("Updated WorldSettings default_game_mode to BP_ShooterGameMode_C")
        unreal.EditorLevelLibrary.save_current_level()

# 3. Check IMC_Weapons
imc = unreal.load_asset("/Game/Variant_Shooter/Input/IMC_Weapons")
if imc:
    mappings = imc.get_editor_property("mappings")
    log(f"IMC_Weapons mapping count: {len(mappings)}")
    mapped_actions = []
    for m in mappings:
        action = m.get_editor_property("action")
        key = m.get_editor_property("key")
        key_name = key.get_editor_property("key_name") if key else "None"
        act_name = action.get_name() if action else "None"
        mapped_actions.append(f"{act_name}:{key_name}")
    log(f"Mapped actions: {mapped_actions}")

# 4. Check UI assets
for name in ["WBP_TarkovInventory", "WBP_BackpackGrid", "WBP_EquippedSlot", "WBP_InventoryItem"]:
    w_asset = unreal.load_asset(f"/Game/UI/{name}")
    log(f"Asset /Game/UI/{name} exists: {w_asset is not None}")

# 5. Check Audio assets
for name in ["UI_Inventory_Open", "UI_Inventory_Close", "UI_Item_Pickup", "UI_Item_Drop", "UI_Item_Rotate"]:
    s_asset = unreal.load_asset(f"/Game/UI/Audio/{name}")
    log(f"Audio /Game/UI/Audio/{name} exists: {s_asset is not None}")

log("In-editor verification complete.")
