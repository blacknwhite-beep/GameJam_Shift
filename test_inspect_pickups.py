import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
for a in actors:
    if "pickup" in a.get_name().lower():
        weapon_class = a.get_editor_property("weapon_class") if hasattr(a, "weapon_class") else None
        try:
            wc = a.get_editor_property("WeaponClass")
        except:
            wc = getattr(a, "weapon_class", None)
        print(f"Pickup {a.get_name()}: weapon_class={wc}")
