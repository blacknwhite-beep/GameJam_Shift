import unreal

# Load Lvl_Shooter
lvl_shooter = unreal.load_asset("/Game/Variant_Shooter/Lvl_Shooter")
print("Lvl_Shooter:", lvl_shooter)

# Let's inspect BP_ShooterGameMode
gm_shooter = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode")
print("BP_ShooterGameMode:", gm_shooter)
cdo_gm = unreal.get_default_object(gm_shooter.generated_class())
print("Default Pawn:", cdo_gm.get_editor_property("default_pawn_class"))
print("Player Controller:", cdo_gm.get_editor_property("player_controller_class"))

# Let's check BP_ShooterCharacter in detail
bp_char = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
print("BP_ShooterCharacter:", bp_char)
