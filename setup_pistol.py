import unreal

print("=== STARTING UNREAL PISTOL ASSET SETUP ===")

# 1. Inspect Animation Assets
reload_seq_path = "/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload"
fire_montage_path = "/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage"
reload_montage_path = "/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload_Montage"

reload_seq = unreal.load_asset(reload_seq_path)
print("Reload Sequence:", reload_seq)

fire_montage = unreal.load_asset(fire_montage_path)
print("Fire Montage:", fire_montage)

reload_montage = unreal.load_asset(reload_montage_path)
if not reload_montage and reload_seq:
    print("Creating MM_Pistol_Reload_Montage...")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.AnimMontageFactory()
    skeleton = reload_seq.get_editor_property("skeleton")
    factory.set_editor_property("target_skeleton", skeleton)
    
    reload_montage = asset_tools.create_asset("MM_Pistol_Reload_Montage", "/Game/Characters/Mannequins/Anims/Pistol", unreal.AnimMontage, factory)
    if reload_montage:
        try:
            unreal.AnimationLibrary.add_sequence_to_anim_montage(reload_montage, reload_seq)
        except Exception as e:
            print("Note on add_sequence_to_anim_montage:", e)
        reload_montage.set_editor_property("slot_node_name", "DefaultGroup.UpperBody")
        unreal.EditorAssetLibrary.save_loaded_asset(reload_montage)
        print("Created and saved Reload Montage!")
else:
    print("Reload Montage exists:", reload_montage)

# 2. Import Audio
audio_dir = "C:/Users/admin/OneDrive/Documents/Unreal Projects/GameJam_Shift_2026/Content/Weapons/Pistol/Audio"
wav_files = ["S_Pistol_Fire.wav", "S_Pistol_Reload.wav", "S_Pistol_DryFire.wav"]
imported_sounds = {}

for wav_file in wav_files:
    wav_path = f"{audio_dir}/{wav_file}"
    asset_name = wav_file.replace(".wav", "")
    dest_path = f"/Game/Weapons/Pistol/Audio/{asset_name}"
    
    sound = unreal.load_asset(dest_path)
    if not sound:
        print(f"Importing {wav_file}...")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", wav_path)
        task.set_editor_property("destination_path", "/Game/Weapons/Pistol/Audio")
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        asset_tools.import_asset_tasks([task])
        sound = unreal.load_asset(dest_path)
        print(f"Imported {asset_name}:", sound)
    else:
        print(f"Sound {asset_name} already loaded:", sound)
    imported_sounds[asset_name] = sound

# 3. Create IA_Reload
actions_dir = "/Game/Variant_Shooter/Input/Actions"
ia_reload_path = f"{actions_dir}/IA_Reload"
ia_reload = unreal.load_asset(ia_reload_path)

if not ia_reload:
    print("Creating IA_Reload...")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    ia_reload = asset_tools.create_asset("IA_Reload", actions_dir, unreal.InputAction, None)
    if ia_reload:
        unreal.EditorAssetLibrary.save_loaded_asset(ia_reload)
        print("Created IA_Reload:", ia_reload)
else:
    print("IA_Reload exists:", ia_reload)

# Map into IMC_Weapons
imc_path = "/Game/Variant_Shooter/Input/IMC_Weapons"
imc = unreal.load_asset(imc_path)
if imc and ia_reload:
    mappings = imc.get_editor_property("mappings")
    existing_keys = [str(m.get_editor_property("key").get_editor_property("key_name")) for m in mappings if m.get_editor_property("action") == ia_reload]
    print("Existing Reload keys in IMC_Weapons:", existing_keys)
    
    dirty = False
    if "R" not in existing_keys:
        key_r = unreal.Key(key_name="R")
        m_r = unreal.EnhancedActionKeyMapping(action=ia_reload, key=key_r)
        mappings.append(m_r)
        dirty = True
        print("Added R mapping")
        
    if "Gamepad_FaceButton_Left" not in existing_keys:
        key_gp = unreal.Key(key_name="Gamepad_FaceButton_Left")
        m_gp = unreal.EnhancedActionKeyMapping(action=ia_reload, key=key_gp)
        mappings.append(m_gp)
        dirty = True
        print("Added Gamepad Face Button Left mapping")
        
    if dirty:
        imc.set_editor_property("mappings", mappings)
        unreal.EditorAssetLibrary.save_loaded_asset(imc)
        print("Saved updated IMC_Weapons")

# 4. Configure BP_ShooterWeapon_Pistol
bp_pistol_path = "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol"
bp_pistol = unreal.load_asset(bp_pistol_path)
if bp_pistol:
    cdo = unreal.get_default_object(bp_pistol.GeneratedClass)
    if cdo:
        if fire_montage:
            cdo.set_editor_property("FiringMontage", fire_montage)
        if reload_montage:
            cdo.set_editor_property("ReloadMontage", reload_montage)
        if imported_sounds.get("S_Pistol_Fire"):
            cdo.set_editor_property("FireSound", imported_sounds["S_Pistol_Fire"])
        if imported_sounds.get("S_Pistol_Reload"):
            cdo.set_editor_property("ReloadSound", imported_sounds["S_Pistol_Reload"])
        if imported_sounds.get("S_Pistol_DryFire"):
            cdo.set_editor_property("DryFireSound", imported_sounds["S_Pistol_DryFire"])
            
        cdo.set_editor_property("ReloadDuration", 1.5)
        cdo.set_editor_property("MagazineSize", 10)
        cdo.set_editor_property("RefireRate", 0.25)
        cdo.set_editor_property("bFullAuto", False)
        unreal.EditorAssetLibrary.save_loaded_asset(bp_pistol)
        print("Configured and saved BP_ShooterWeapon_Pistol!")

# 5. Configure BP_ShooterCharacter
bp_char_path = "/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"
bp_char = unreal.load_asset(bp_char_path)
if bp_char and ia_reload:
    cdo_char = unreal.get_default_object(bp_char.GeneratedClass)
    if cdo_char:
        cdo_char.set_editor_property("ReloadAction", ia_reload)
        unreal.EditorAssetLibrary.save_loaded_asset(bp_char)
        print("Configured and saved BP_ShooterCharacter ReloadAction!")

print("=== SETUP SCRIPT COMPLETED SUCCESSFULLY ===")
