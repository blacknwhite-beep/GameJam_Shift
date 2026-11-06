import unreal

assets = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False)
gamemodes = [a for a in assets if "gamemode" in a.lower()]
print("GameModes in project:")
for gm in gamemodes:
    gm_asset = unreal.load_asset(gm)
    print(" ", gm)
    if gm_asset and hasattr(gm_asset, "generated_class"):
        cdo = unreal.get_default_object(gm_asset.generated_class())
        if cdo:
            try:
                print("    Default Pawn Class:", cdo.get_editor_property("default_pawn_class"))
            except Exception as e:
                print("    Err getting default_pawn_class:", e)

maps = [a for a in assets if unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name == "World"]
print("\nMaps in project:")
for m in maps:
    print(" ", m)
