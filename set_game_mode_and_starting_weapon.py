#!/usr/bin/env python3
# ==============================================================================
# set_game_mode_and_starting_weapon.py  (fw-kilo)
# -----------------------------------------------------------------------------
# Configures the two GameMode / spawn items required by the inventory spec:
#   1. Make BP_ShooterGameMode the active GameMode for Lvl_FirstPerson.
#   2. Set BP_ShooterCharacter CDO InitialWeaponClass = BP_ShooterWeapon_Pistol
#
# Compatible with Unreal Engine 5.7 API.
# ==============================================================================

import unreal
import os

GAME_MODE_CLASS      = "/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode.BP_ShooterGameMode_C"
CHARACTER_BP         = "/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"
WEAPON_CLASS         = "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol.BP_ShooterWeapon_Pistol_C"

# ----------------------------------------------------------------------------- #
# 1. GameMode on the open world's WorldSettings
# ----------------------------------------------------------------------------- #
def set_world_game_mode():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_editor_world() if subsystem else None
    if not world:
        unreal.log_warning("No world loaded; open Lvl_FirstPerson first.")
        return False

    ws = world.get_world_settings()
    if not ws:
        unreal.log_warning("WorldSettings not found.")
        return False

    gm_cls = unreal.load_asset(GAME_MODE_CLASS)
    if not gm_cls:
        unreal.log_warning("Could not load GameMode class %s" % GAME_MODE_CLASS)
        return False

    ws.set_editor_property("default_game_mode", gm_cls)
    unreal.EditorLevelLibrary.save_current_level()
    print("[GameMode] WorldSettings.default_game_mode -> %s" % GAME_MODE_CLASS)
    return True


# ----------------------------------------------------------------------------- #
# 2. BP_ShooterCharacter CDO InitialWeaponClass
# ----------------------------------------------------------------------------- #
def set_initial_weapon_class():
    char_bp = unreal.load_asset(CHARACTER_BP)
    if not char_bp:
        unreal.log_warning("Could not load %s" % CHARACTER_BP)
        return False

    cdo = unreal.get_default_object(char_bp.generated_class())
    if not cdo:
        unreal.log_warning("No CDO for %s" % CHARACTER_BP)
        return False

    weapon_cls = unreal.load_asset(WEAPON_CLASS)
    if not weapon_cls:
        unreal.log_warning("Could not load %s" % WEAPON_CLASS)
        return False

    cdo.set_editor_property("initial_weapon_class", weapon_cls)
    unreal.EditorAssetLibrary.save_loaded_asset(char_bp)
    print("[CDO] BP_ShooterCharacter.initial_weapon_class -> %s" % WEAPON_CLASS)
    return True


def main():
    ok_wm = set_world_game_mode()
    ok_iw = set_initial_weapon_class()
    print("\n[DONE] GameMode: %s | InitialWeapon: %s" % (
        "OK" if ok_wm else "SKIPPED",
        "OK" if ok_iw else "FAILED"))


if __name__ == "__main__":
    main()
