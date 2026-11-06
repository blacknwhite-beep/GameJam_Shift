import unreal

world = unreal.EditorLevelLibrary.get_editor_world()
print("Current Editor World:", world.get_name(), world.get_path_name())

# Check World Settings GameMode
world_settings = world.get_world_settings()
print("World Settings:", world_settings)
gm_override = world_settings.get_editor_property("default_game_mode")
print("World Settings GameMode Override:", gm_override)

# Check Maps & Modes Default GameMode
engine_settings = unreal.GeneralProjectSettings()
# Or via Config / Asset
maps_settings = unreal.load_asset("/Engine/Transient") # let's check config files or GameMode classes

# List all GameModes in project
assets = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True, include_folder=False)
gamemodes = [a for a in assets if "gamemode" in a.lower()]
print("GameModes in project:")
for gm in gamemodes:
    print(" ", gm)
    gm_asset = unreal.load_asset(gm)
    if gm_asset and hasattr(gm_asset, "generated_class"):
        cdo = unreal.get_default_object(gm_asset.generated_class())
        if cdo and hasattr(cdo, "default_pawn_class"):
            print("    Default Pawn Class:", cdo.get_editor_property("default_pawn_class"))
