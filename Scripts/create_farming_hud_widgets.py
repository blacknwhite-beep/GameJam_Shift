import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory("/Game/Dev_Blueprint/UI")

widgets_to_create = [
    ("WBP_FarmingHUD", unreal.FarmingHUDWidget),
    ("WBP_CropPrompt", unreal.CropPromptWidget),
]

for asset_name, parent_cls in widgets_to_create:
    asset_path = f"/Game/Dev_Blueprint/UI/{asset_name}"
    existing = unreal.load_asset(asset_path)
    if existing:
        print(f"[FarmingHUD] {asset_path} already exists.")
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent_cls)
        created = asset_tools.create_asset(asset_name, "/Game/Dev_Blueprint/UI", unreal.WidgetBlueprint, factory)
        if created:
            unreal.EditorAssetLibrary.save_loaded_asset(created)
            print(f"[FarmingHUD] Created and saved {asset_path} (Parent: {parent_cls.get_name()})")
        else:
            print(f"[FarmingHUD] ERROR: Failed to create {asset_path}")
