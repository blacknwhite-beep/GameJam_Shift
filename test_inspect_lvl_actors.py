import unreal

# Load Lvl_Shooter and inspect actors
unreal.EditorLevelLibrary.load_level("/Game/Variant_Shooter/Lvl_Shooter")
world = unreal.EditorLevelLibrary.get_editor_world()
print("Loaded world:", world.get_name())

actors = unreal.EditorLevelLibrary.get_all_level_actors()
print(f"Total actors in {world.get_name()}:", len(actors))
for a in actors:
    name = a.get_name()
    cname = a.get_class().get_name()
    if any(k in name.lower() or k in cname.lower() for k in ["weapon", "pickup", "spawn", "player", "pistol", "rifle"]):
        print(f"  Actor: {name} (Class: {cname}) Location: {a.get_actor_location()}")
