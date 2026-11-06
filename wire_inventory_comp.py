import unreal

bp_char = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
cdo_char = unreal.get_default_object(bp_char.generated_class())

wbp_inv = unreal.load_asset("/Game/UI/WBP_TarkovInventory")
s_open = unreal.load_asset("/Game/UI/Audio/UI_Inventory_Open")
s_close = unreal.load_asset("/Game/UI/Audio/UI_Inventory_Close")
s_pickup = unreal.load_asset("/Game/UI/Audio/UI_Item_Pickup")
s_drop = unreal.load_asset("/Game/UI/Audio/UI_Item_Drop")
s_rotate = unreal.load_asset("/Game/UI/Audio/UI_Item_Rotate")

# Get TarkovInventory component template
comp = cdo_char.get_editor_property("TarkovInventory")
print("TarkovInventory component template:", comp)
if comp:
    if wbp_inv:
        comp.set_editor_property("InventoryWidgetClass", wbp_inv.generated_class())
    if s_open:
        comp.set_editor_property("SoundInventoryOpen", s_open)
    if s_close:
        comp.set_editor_property("SoundInventoryClose", s_close)
    if s_pickup:
        comp.set_editor_property("SoundItemPickup", s_pickup)
    if s_drop:
        comp.set_editor_property("SoundItemDrop", s_drop)
    if s_rotate:
        comp.set_editor_property("SoundItemRotate", s_rotate)
        
    unreal.EditorAssetLibrary.save_loaded_asset(bp_char)
    print("Successfully configured and saved TarkovInventory component properties!")

# Verify
print("Verified properties:")
print("  GridColumns:", comp.get_editor_property("GridColumns"))
print("  GridRows:", comp.get_editor_property("GridRows"))
print("  CellPixelSize:", comp.get_editor_property("CellPixelSize"))
print("  BackpackPixelWidth:", comp.get_editor_property("BackpackPixelWidth"))
print("  BackpackPixelHeight:", comp.get_editor_property("BackpackPixelHeight"))
print("  InventoryWidgetClass:", comp.get_editor_property("InventoryWidgetClass"))
print("  SoundInventoryOpen:", comp.get_editor_property("SoundInventoryOpen"))
