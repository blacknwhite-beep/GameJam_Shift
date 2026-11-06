import unreal

# Let's inspect Lvl_Shooter actors
editor_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# Let's check BP_ShooterCharacter graph
bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
print("BP_ShooterCharacter:", bp)

# Let's see what DefaultStartingWeapon or similar properties exist in BP_ShooterCharacter
cdo = unreal.get_default_object(bp.generated_class())
for p in dir(cdo):
    if "weapon" in p.lower():
        print("CDO property:", p)

# Let's check Project Settings Default GameMode and Maps
default_maps = unreal.load_asset("/Game/Variant_Shooter/Lvl_Shooter")
print("Lvl_Shooter asset:", default_maps)
