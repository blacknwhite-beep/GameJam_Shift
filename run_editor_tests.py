import unreal

print("=== RUNNING IN-EDITOR VERIFICATION TESTS ===")

# Test 1: Load Assets
print("\n[TEST 1: ASSET LOADING]")
bp_pistol = unreal.load_asset("/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol")
bp_char = unreal.load_asset("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
ia_reload = unreal.load_asset("/Game/Variant_Shooter/Input/Actions/IA_Reload")
imc_weapons = unreal.load_asset("/Game/Variant_Shooter/Input/IMC_Weapons")
fire_m = unreal.load_asset("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage")
reload_m = unreal.load_asset("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload_Montage")
s_fire = unreal.load_asset("/Game/Weapons/Pistol/Audio/S_Pistol_Fire")
s_reload = unreal.load_asset("/Game/Weapons/Pistol/Audio/S_Pistol_Reload")
s_dryfire = unreal.load_asset("/Game/Weapons/Pistol/Audio/S_Pistol_DryFire")

assets = {
    "BP_ShooterWeapon_Pistol": bp_pistol,
    "BP_ShooterCharacter": bp_char,
    "IA_Reload": ia_reload,
    "IMC_Weapons": imc_weapons,
    "MM_Pistol_Fire_Montage": fire_m,
    "MM_Pistol_Reload_Montage": reload_m,
    "S_Pistol_Fire": s_fire,
    "S_Pistol_Reload": s_reload,
    "S_Pistol_DryFire": s_dryfire,
}

all_assets_ok = True
for name, asset in assets.items():
    ok = asset is not None
    if not ok:
        all_assets_ok = False
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}: {asset}")

# Test 2: Verify Character CDO Properties
print("\n[TEST 2: CHARACTER CDO PROPERTIES]")
cdo_char = unreal.get_default_object(bp_char.generated_class())
char_reload_act = cdo_char.get_editor_property("reload_action")
print(f"  [{'PASS' if char_reload_act == ia_reload else 'FAIL'}] reload_action: {char_reload_act}")

# Test 3: Verify Weapon CDO Properties
print("\n[TEST 3: PISTOL WEAPON CDO PROPERTIES]")
cdo_pistol = unreal.get_default_object(bp_pistol.generated_class())
p_fire_m = cdo_pistol.get_editor_property("firing_montage")
p_reload_m = cdo_pistol.get_editor_property("reload_montage")
p_fire_s = cdo_pistol.get_editor_property("fire_sound")
p_reload_s = cdo_pistol.get_editor_property("reload_sound")
p_dry_s = cdo_pistol.get_editor_property("dry_fire_sound")
p_reload_dur = cdo_pistol.get_editor_property("reload_duration")
p_mag_size = cdo_pistol.get_editor_property("magazine_size")
p_refire = cdo_pistol.get_editor_property("refire_rate")

print(f"  [{'PASS' if p_fire_m == fire_m else 'FAIL'}] firing_montage: {p_fire_m}")
print(f"  [{'PASS' if p_reload_m == reload_m else 'FAIL'}] reload_montage: {p_reload_m}")
print(f"  [{'PASS' if p_fire_s == s_fire else 'FAIL'}] fire_sound: {p_fire_s}")
print(f"  [{'PASS' if p_reload_s == s_reload else 'FAIL'}] reload_sound: {p_reload_s}")
print(f"  [{'PASS' if p_dry_s == s_dryfire else 'FAIL'}] dry_fire_sound: {p_dry_s}")
print(f"  [{'PASS' if p_reload_dur == 1.5 else 'FAIL'}] reload_duration: {p_reload_dur}")
print(f"  [{'PASS' if p_mag_size == 10 else 'FAIL'}] magazine_size: {p_mag_size}")
print(f"  [{'PASS' if p_refire == 0.25 else 'FAIL'}] refire_rate: {p_refire}")

# Test 4: Verify Input Mappings in IMC_Weapons
print("\n[TEST 4: INPUT MAPPING CONTEXT]")
mappings = imc_weapons.get_editor_property("mappings")
reload_keys = [str(m.get_editor_property("key").get_editor_property("key_name")) for m in mappings if m.get_editor_property("action") == ia_reload]
has_r = "R" in reload_keys
has_gp = "Gamepad_FaceButton_Left" in reload_keys
print(f"  [{'PASS' if has_r else 'FAIL'}] IMC_Weapons contains 'R' mapping for IA_Reload: {reload_keys}")
print(f"  [{'PASS' if has_gp else 'FAIL'}] IMC_Weapons contains 'Gamepad_FaceButton_Left' mapping for IA_Reload: {reload_keys}")

# Test 5: Verify Montages & Animations
print("\n[TEST 5: MONTAGES & ANIMATIONS]")
print(f"  Fire Montage duration: {fire_m.sequence_length:.2f}s, slots: {unreal.AnimationLibrary.get_montage_slot_names(fire_m)}")
print(f"  Reload Montage duration: {reload_m.sequence_length:.2f}s, slots: {unreal.AnimationLibrary.get_montage_slot_names(reload_m)}")

print("\n=== ALL IN-EDITOR VERIFICATION TESTS COMPLETED ===")
