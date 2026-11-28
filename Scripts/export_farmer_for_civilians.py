import os
import unreal

OUT_DIR = r"C:\UnrealProjects\GameJam_Shift_2026\Scripts\CivilianExport"
os.makedirs(OUT_DIR, exist_ok=True)

# 1. Export T_Animalstextures as TGA first
tex = unreal.load_object(None, "/Game/3D_LOW_POLY_FarmerPack/Texture/T_Animalstextures.T_Animalstextures")
if tex:
    tex_path = os.path.join(OUT_DIR, "T_Animalstextures.tga")
    task = unreal.AssetExportTask()
    task.object = tex
    task.filename = tex_path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log(f"[ExportFarmer] Texture export ok={ok} -> {tex_path}")
else:
    unreal.log_error("[ExportFarmer] Could not load T_Animalstextures")

# 2. Export SKM_Farmer_male as FBX
skm = unreal.load_object(None, "/Game/3D_LOW_POLY_FarmerPack/Characters/Mesh/SKM_Farmer_male.SKM_Farmer_male")
if skm:
    mats = skm.get_editor_property("materials")
    unreal.log(f"[ExportFarmer] Material slots ({len(mats)}): {[str(m.material_slot_name) + ':' + str(m.material_interface.get_path_name() if m.material_interface else None) for m in mats]}")
    fbx_path = os.path.join(OUT_DIR, "SKM_Farmer_male.fbx")
    task = unreal.AssetExportTask()
    task.object = skm
    task.filename = fbx_path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    opts = unreal.FbxExportOption()
    opts.ascii = False
    opts.collision = False
    opts.level_of_detail = False
    opts.vertex_color = True
    opts.export_morph_targets = False
    task.options = opts
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log(f"[ExportFarmer] FBX export ok={ok} -> {fbx_path}")
else:
    unreal.log_error("[ExportFarmer] Could not load SKM_Farmer_male")
