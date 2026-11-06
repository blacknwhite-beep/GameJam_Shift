import unreal

factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

for name in ["WBP_BackpackGrid", "WBP_EquippedSlot", "WBP_InventoryItem"]:
    path = f"/Game/UI/{name}"
    w = unreal.load_asset(path)
    if not w:
        w = asset_tools.create_asset(name, "/Game/UI", unreal.WidgetBlueprint, factory)
        if w:
            unreal.EditorAssetLibrary.save_loaded_asset(w)
            print(f"Created and saved {name}!")
    else:
        print(f"{name} already exists.")
