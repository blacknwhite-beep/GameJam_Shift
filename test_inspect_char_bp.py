import unreal

bp_char = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")

# Let's see what components are in BP_ShooterCharacter
gen = bp_char.generated_class()
cdo = unreal.get_default_object(gen)

# Does BP_ShooterCharacter give a weapon on BeginPlay?
# In C++, let's check ShooterCharacter.cpp BeginPlay!
