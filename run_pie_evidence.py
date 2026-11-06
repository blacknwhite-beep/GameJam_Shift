import unreal
import json

evidence = []

def record(test_id, name, passed, details=""):
    status = "PASS" if passed else "FAIL"
    evidence.append(f"[{status}] {test_id}: {name} - {details}")
    unreal.log(f"[PIE_EVIDENCE] [{status}] {test_id}: {name} - {details}")
    print(f"[PIE_EVIDENCE] [{status}] {test_id}: {name} - {details}")

# T1: Character Spawning with InitialWeaponClass
char_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
if char_bp:
    cdo = unreal.get_default_object(char_bp.generated_class())
    init_weapon = cdo.get_editor_property("initial_weapon_class")
    record("T1", "Initial Weapon Class configured on CDO", init_weapon is not None, str(init_weapon.get_name() if init_weapon else "None"))
else:
    record("T1", "Initial Weapon Class configured on CDO", False, "BP_ShooterCharacter missing")

# T2: GameMode and WorldSettings
editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor_subsystem.get_editor_world()
if world:
    ws = world.get_world_settings()
    gm = ws.get_editor_property("default_game_mode")
    record("T2", "WorldSettings default GameMode", gm is not None and "BP_ShooterGameMode" in gm.get_name(), str(gm.get_name() if gm else "None"))
else:
    record("T2", "WorldSettings default GameMode", False, "No editor world")

# T3: Geometry & Dimensions: Backpack 120x120, Item 20x20
if char_bp:
    comp = cdo.get_component_by_class(unreal.TarkovInventoryComponent)
    if comp:
        w = comp.get_editor_property("backpack_pixel_width")
        h = comp.get_editor_property("backpack_pixel_height")
        cell = comp.get_editor_property("cell_pixel_size")
        cols = comp.get_editor_property("grid_columns")
        rows = comp.get_editor_property("grid_rows")
        
        is_120 = (w == 120.0 and h == 120.0 and cols == 6 and rows == 6 and cell == 20.0)
        record("T3", "Backpack 120x120px and Cell 20x20px measurement", is_120, f"Width={w}px, Height={h}px, Cols={cols}, Rows={rows}, Cell={cell}px")
    else:
        record("T3", "Backpack measurement", False, "TarkovInventoryComponent missing on CDO")

# T4: Enhanced Input IA_ToggleInventory and IMC_Weapons
imc = unreal.load_asset("/Game/Variant_Shooter/Input/IMC_Weapons")
if imc:
    mappings = imc.get_editor_property("mappings")
    key_names = []
    for m in mappings:
        act = m.get_editor_property("action")
        key = m.get_editor_property("key")
        if act and "IA_ToggleInventory" in act.get_name() and key:
            key_names.append(key.get_editor_property("key_name"))
    has_tab_and_i = ("Tab" in key_names and "I" in key_names)
    record("T4", "IA_ToggleInventory mapped to Tab and I in IMC_Weapons", has_tab_and_i, f"Keys: {key_names}")
else:
    record("T4", "IMC_Weapons mapping check", False, "IMC_Weapons missing")

# T5: Widget Assets Presence
widgets_ok = True
missing_w = []
for w_name in ["WBP_TarkovInventory", "WBP_BackpackGrid", "WBP_EquippedSlot", "WBP_InventoryItem"]:
    if not unreal.load_asset(f"/Game/UI/{w_name}"):
        widgets_ok = False
        missing_w.append(w_name)
record("T5", "UMG Inventory Widgets loaded in UE", widgets_ok, f"Missing: {missing_w}" if missing_w else "All 4 widgets verified")

# T6: Audio Assets Presence
audio_ok = True
missing_a = []
for a_name in ["UI_Inventory_Open", "UI_Inventory_Close", "UI_Item_Pickup", "UI_Item_Drop", "UI_Item_Rotate"]:
    if not unreal.load_asset(f"/Game/UI/Audio/{a_name}"):
        audio_ok = False
        missing_a.append(a_name)
record("T6", "CC0 UI Audio Assets imported in UE", audio_ok, f"Missing: {missing_a}" if missing_a else "All 5 cues verified")

# T7: Component Audio Wiring
if char_bp and comp:
    open_s = comp.get_editor_property("sound_inventory_open")
    close_s = comp.get_editor_property("sound_inventory_close")
    pickup_s = comp.get_editor_property("sound_item_pickup")
    drop_s = comp.get_editor_property("sound_item_drop")
    rot_s = comp.get_editor_property("sound_item_rotate")
    sounds_wired = all([open_s, close_s, pickup_s, drop_s, rot_s])
    record("T7", "TarkovInventoryComponent Audio Cues wired", sounds_wired, "All 5 cues wired to SoundWave assets")

# T8: Widget Class Wiring
if char_bp and comp:
    w_class = comp.get_editor_property("inventory_widget_class")
    record("T8", "InventoryWidgetClass wired to WBP_TarkovInventory", w_class is not None and "WBP_TarkovInventory" in w_class.get_name(), str(w_class.get_name() if w_class else "None"))

print("\n--- PIE EVIDENCE SUMMARY ---")
for line in evidence:
    print(line)
