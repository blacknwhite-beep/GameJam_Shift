import unreal

for g in ["M1911", "M1911Tactical", "MP5", "MP7", "M4A1", "M16", "SCAR"]:
    sm = unreal.load_asset(f"/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_{g}")
    if sm:
        bb = sm.get_bounds()
        # Sample bounding boxes at +Y end and -Y end to see which end is taller in Z (barrel vs grip/stock)
        unreal.log_warning(f"[GUN_ORIENT] {g}: origin=({bb.origin.x:.1f}, {bb.origin.y:.1f}, {bb.origin.z:.1f}) extent=({bb.box_extent.x:.1f}, {bb.box_extent.y:.1f}, {bb.box_extent.z:.1f})")

for w_path in [
    "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol",
    "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Rifle",
    "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_GrenadeLauncher",
]:
    bp = unreal.load_asset(w_path)
    if bp:
        cdo = unreal.get_default_object(bp.generated_class())
        for prop in ["ProjectileClass", "MagazineSize", "ReloadDuration", "FiringMontage", "ReloadMontage", "FirstPersonAnimInstanceClass", "ThirdPersonAnimInstanceClass", "FireSound", "ReloadSound", "DryFireSound", "AimVariance", "FiringRecoil", "MuzzleSocketName", "MuzzleOffset", "bFullAuto", "RefireRate"]:
            try:
                val = cdo.get_editor_property(prop)
                unreal.log_warning(f"[WEP_PROP] {bp.get_name()}.{prop} = {val}")
            except Exception as e:
                pass
