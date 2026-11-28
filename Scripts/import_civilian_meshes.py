import os
import unreal

SRC_DIR = r"C:\UnrealProjects\GameJam_Shift_2026\Scripts\CivilianExport"
DEST_PATH = "/Game/Dev_Blueprint/Characters/Civilians"
SKEL_PATH = "/Game/3D_LOW_POLY_FarmerPack/Characters/Mesh/SK_Farmer_male_Skeleton.SK_Farmer_male_Skeleton"

skeleton = unreal.load_object(None, SKEL_PATH)
if not skeleton:
    raise RuntimeError(f"Could not load skeleton: {SKEL_PATH}")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

def import_skm(fbx_filename, asset_name):
    src = os.path.join(SRC_DIR, fbx_filename)
    if not os.path.isfile(src):
        raise RuntimeError(f"Missing FBX: {src}")

    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = True
    opts.import_animations = False
    opts.import_materials = True
    opts.import_textures = False
    opts.create_physics_asset = False
    opts.skeleton = skeleton
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    opts.skeletal_mesh_import_data.set_editor_property("import_content_type", unreal.FBXImportContentType.FBXICT_ALL)
    opts.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", True)
    opts.skeletal_mesh_import_data.set_editor_property("update_skeleton_reference_pose", False)

    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = DEST_PATH
    task.destination_name = asset_name
    task.factory = unreal.FbxFactory()
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = opts

    asset_tools.import_asset_tasks([task])

    skm_path = f"{DEST_PATH}/{asset_name}.{asset_name}"
    skm = unreal.load_object(None, skm_path)
    if skm:
        phys = unreal.load_object(None, "/Game/3D_LOW_POLY_FarmerPack/Characters/Mesh/PA_Farmer_male_PhysicsAsset.PA_Farmer_male_PhysicsAsset")
        if phys:
            skm.set_editor_property("physics_asset", phys)
        unreal.EditorAssetLibrary.save_asset(f"{DEST_PATH}/{asset_name}", only_if_is_dirty=False)
        unreal.log(f"[ImportCivilians] Saved {skm_path} (skeleton={skm.skeleton.get_name() if skm.skeleton else None})")
    else:
        unreal.log_error(f"[ImportCivilians] Failed to load {skm_path}")

import_skm("SKM_Civilian_RedShirt_Jeans.fbx", "SKM_Civilian_RedShirt_Jeans")
import_skm("SKM_Civilian_BlueShirt_Shorts.fbx", "SKM_Civilian_BlueShirt_Shorts")

# Save any generated materials in DEST_PATH
for asset_path in unreal.EditorAssetLibrary.list_assets(DEST_PATH, recursive=True):
    unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)

unreal.log("[ImportCivilians] DONE.")
