import unreal

pc = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterPlayerController")
print("BP_ShooterPlayerController:", pc)
cdo = unreal.get_default_object(pc.generated_class())
for p in dir(cdo):
    if "hud" in p.lower() or "widget" in p.lower() or "ui" in p.lower():
        print("  PC prop:", p)
