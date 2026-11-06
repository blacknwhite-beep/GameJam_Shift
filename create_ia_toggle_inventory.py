#!/usr/bin/env python3
# ==============================================================================
# create_ia_toggle_inventory.py
# -----------------------------------------------------------------------------
# Creates the Toggle Inventory Enhanced Input action and binds it to Tab + I
# inside IMC_Weapons.  Idempotent: re-running only adds missing bindings.
#
# Compatible with Unreal Engine 5.7 API.
# ==============================================================================

import unreal

# ----------------------------------------------------------------------------- #
# Paths
# ----------------------------------------------------------------------------- #
INPUT_DIR    = "/Game/Variant_Shooter/Input"
ACTIONS_DIR  = INPUT_DIR + "/Actions"
IA_INV       = ACTIONS_DIR + "/IA_ToggleInventory"
IMC_WEAPONS  = INPUT_DIR + "/IMC_Weapons"

def make_key(key_name):
    k = unreal.Key()
    k.set_editor_property("key_name", key_name)
    return k

KEY_TAB = make_key("Tab")
KEY_I   = make_key("I")


# ----------------------------------------------------------------------------- #
# Helpers
# ----------------------------------------------------------------------------- #
def _save(asset_or_path):
    if isinstance(asset_or_path, str):
        unreal.EditorAssetLibrary.save_asset(asset_or_path)
    else:
        unreal.EditorAssetLibrary.save_loaded_asset(asset_or_path)


# ----------------------------------------------------------------------------- #
# 1. Create / load IA_ToggleInventory
# ----------------------------------------------------------------------------- #
def ensure_input_action():
    """Return the IA_ToggleInventory UInputAction, creating it if needed."""
    if unreal.EditorAssetLibrary.does_asset_exist(IA_INV):
        action = unreal.load_asset(IA_INV)
        if action:
            print("[IA_ToggleInventory] Loaded existing action: %s" % IA_INV)
            return action

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.InputAction_Factory()
    action = asset_tools.create_asset("IA_ToggleInventory", ACTIONS_DIR, unreal.InputAction, factory)

    if not action:
        raise RuntimeError("Failed to create UInputAction at %s" % IA_INV)

    print("[IA_ToggleInventory] Created new action: %s" % IA_INV)
    return action


# ----------------------------------------------------------------------------- #
# 2. Add the Tab + I mappings to IMC_Weapons
# ----------------------------------------------------------------------------- #
def add_toggle_inventory_mapping(context, action):
    """Append Tab + I bindings for IA_ToggleInventory if not already present."""
    mappings = context.get_editor_property("mappings")
    existing_keys = set()
    for m in mappings:
        act = m.get_editor_property("action")
        if act and act.get_name() == action.get_name():
            key = m.get_editor_property("key")
            if key:
                existing_keys.add(key.get_editor_property("key_name"))

    if "Tab" in existing_keys and "I" in existing_keys:
        print("[IMC_Weapons] ToggleInventory mapping already present, skipping.")
        return

    for key_name in ("Tab", "I"):
        if key_name in existing_keys:
            continue
        mapping = context.map_key(action, make_key(key_name))
        print("[IMC_Weapons] Added mapping: %s -> %s" % (key_name, action.get_name()))

    _save(context)
    print("[IMC_Weapons] Saved.")


# ----------------------------------------------------------------------------- #
# 3. Main
# ----------------------------------------------------------------------------- #
def main():
    action = ensure_input_action()
    _save(action)

    context = unreal.load_asset(IMC_WEAPONS)
    if not context:
        raise RuntimeError("Could not load %s" % IMC_WEAPONS)

    add_toggle_inventory_mapping(context, action)
    print("\n[DONE] IA_ToggleInventory is ready.")


if __name__ == "__main__":
    main()
