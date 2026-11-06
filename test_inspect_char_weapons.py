import unreal

char_bp = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
cdo = unreal.get_default_object(char_bp.generated_class())

print("BP_ShooterCharacter CDO properties:")
for prop in ["current_weapon", "weapons", "starting_weapons", "weapon_class", "initial_weapons", "default_weapon"]:
    try:
        val = cdo.get_editor_property(prop)
        print(f"  {prop}: {val}")
    except Exception as e:
        # print(f"  {prop} err: {e}")
        pass

# Check C++ header of ShooterCharacter.h for weapon handling
