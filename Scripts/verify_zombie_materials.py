"""Read-only check of the zombie material chain. Logs results with the [ZOMBIE_VERIFY] tag."""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning(f"[ZOMBIE_VERIFY] {msg}")


def tex_name(mi, param):
    t = mel.get_material_instance_texture_parameter_value(mi, param)
    return t.get_name() if t else "NONE"


def main():
    master = unreal.load_asset("/Game/Dev_Blueprint/Characters/Zombie/Materials/M_ZombieMaster")
    log(f"M_ZombieMaster exists={bool(master)} used_with_skeletal_mesh={master.get_editor_property('used_with_skeletal_mesh') if master else None}")

    for mi_name, tset in (("MI_Zombie_Slow_Skin", "Set1"), ("MI_Zombie_Fast_Skin", "Set2")):
        mi = unreal.load_asset(f"/Game/Dev_Blueprint/Characters/Zombie/Materials/{mi_name}")
        if not mi:
            log(f"{mi_name} MISSING")
            continue
        parent = mi.get_editor_property("parent")
        log(f"{mi_name} parent={parent.get_path_name() if parent else None}")
        for p in ("Albedo", "Normal", "Metallic", "Gloss", "AO", "Emission"):
            log(f"  {mi_name}.{p} = {tex_name(mi, p)}")

    for bp_name in ("BP_Zombie_Slow", "BP_Zombie_Fast"):
        bp = unreal.load_asset(f"/Game/Dev_Blueprint/Blueprints/Enemies/{bp_name}")
        if not bp:
            log(f"{bp_name} MISSING")
            continue
        cdo = unreal.get_default_object(bp.generated_class())
        mesh = cdo.get_editor_property("Mesh")
        mats = mesh.get_materials() if mesh else []
        names = [m.get_name() if m else "NONE" for m in mats]
        skin = cdo.get_editor_property("ZombieSkinMaterial")
        log(f"{bp_name} skin={skin.get_name() if skin else None} mesh={mesh.skeletal_mesh.get_name() if mesh and mesh.skeletal_mesh else None} slot_materials={names}")


main()
