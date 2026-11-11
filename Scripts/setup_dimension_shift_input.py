#!/usr/bin/env python3
# ==============================================================================
# setup_dimension_shift_input.py
# -----------------------------------------------------------------------------
# Creates the IA_DimensionShift Enhanced Input action, binds it to LeftShift
# in IMC_Weapons and IMC_Default, and sets DimensionShiftAction on BP_ShooterCharacter.
# ==============================================================================

import unreal

INPUT_DIR = "/Game/Variant_Shooter/Input"
ACTIONS_DIR = INPUT_DIR + "/Actions"
IA_SHIFT_PATH = ACTIONS_DIR + "/IA_DimensionShift"
IMC_WEAPONS_PATH = INPUT_DIR + "/IMC_Weapons"
IMC_DEFAULT_PATH = "/Game/Input/IMC_Default"
BP_CHAR_PATH = "/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"

def make_key(key_name):
    k = unreal.Key()
    k.set_editor_property("key_name", key_name)
    return k

def _save(asset_or_path):
    if isinstance(asset_or_path, str):
        unreal.EditorAssetLibrary.save_asset(asset_or_path)
    else:
        unreal.EditorAssetLibrary.save_loaded_asset(asset_or_path)

def ensure_shift_action():
    if unreal.EditorAssetLibrary.does_asset_exist(IA_SHIFT_PATH):
        action = unreal.load_asset(IA_SHIFT_PATH)
        if action:
            print(f"[IA_DimensionShift] Loaded existing action: {IA_SHIFT_PATH}")
            return action

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.InputAction_Factory()
    action = asset_tools.create_asset("IA_DimensionShift", ACTIONS_DIR, unreal.InputAction, factory)
    if not action:
        raise RuntimeError("Failed to create IA_DimensionShift")
    print(f"[IA_DimensionShift] Created new action: {IA_SHIFT_PATH}")
    return action

def add_shift_mapping(context_path, action):
    context = unreal.load_asset(context_path)
    if not context:
        print(f"[Warning] Could not load context {context_path}")
        return

    mappings = context.get_editor_property("mappings")
    existing_keys = set()
    for m in mappings:
        act = m.get_editor_property("action")
        if act and act.get_name() == action.get_name():
            key = m.get_editor_property("key")
            if key:
                existing_keys.add(key.get_editor_property("key_name"))

    if "LeftShift" in existing_keys:
        print(f"[{context_path}] LeftShift already mapped to IA_DimensionShift.")
        return

    mapping = context.map_key(action, make_key("LeftShift"))
    print(f"[{context_path}] Added mapping: LeftShift -> {action.get_name()}")
    _save(context)

def assign_to_character(action):
    bp_char = unreal.load_asset(BP_CHAR_PATH)
    if not bp_char:
        print(f"[Warning] Could not load {BP_CHAR_PATH}")
        return

    gen_cls = bp_char.generated_class() if callable(bp_char.generated_class) else bp_char.generated_class
    cdo = unreal.get_default_object(gen_cls)
    if cdo:
        try:
            cdo.set_editor_property("dimension_shift_action", action)
            unreal.EditorAssetLibrary.save_loaded_asset(bp_char)
            print(f"[BP_ShooterCharacter] Successfully assigned DimensionShiftAction = {action.get_name()}")
        except Exception as e:
            print(f"[BP_ShooterCharacter] Could not set dimension_shift_action via CDO: {e}")

def main():
    action = ensure_shift_action()
    _save(action)
    add_shift_mapping(IMC_WEAPONS_PATH, action)
    add_shift_mapping(IMC_DEFAULT_PATH, action)
    assign_to_character(action)
    print("\n[SUCCESS] Dimension Shift Enhanced Input is completely configured!")

if __name__ == "__main__":
    main()
