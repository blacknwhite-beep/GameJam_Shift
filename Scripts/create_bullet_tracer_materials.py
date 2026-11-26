"""Create the small smoky bullet-tracer materials used by ABulletTracer.

Run in UE 5.7 with:
    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=Scripts/create_bullet_tracer_materials.py

The script is safe to re-run. It rebuilds only the two tracer materials and
copies the engine's soft smoke texture into the project so the effect is cooked
with the game instead of depending on editor/tutorial content at runtime.
"""

import traceback
import unreal

LOG_PATH = r"C:\UnrealProjects\GameJam_Shift_2026\Saved\bullet_tracer_materials.log"


def log(message):
    text = "[BulletTracer] " + message
    unreal.log(text)
    print(text, flush=True)
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as handle:
            handle.write(text + "\n")
    except Exception:
        pass

CONTENT_ROOT = "/Game/Dev_Blueprint/VFX"
MATERIAL_ROOT = CONTENT_ROOT + "/Materials"
TEXTURE_ROOT = CONTENT_ROOT + "/Textures"

SOURCE_SMOKE_TEXTURE = "/Engine/Tutorial/SubEditors/TutorialAssets/T_soft_smoke"
SMOKE_TEXTURE_PATH = TEXTURE_ROOT + "/T_BulletSmoke"
SMOKE_MATERIAL_PATH = MATERIAL_ROOT + "/M_BulletSmoke"
CORE_MATERIAL_PATH = MATERIAL_ROOT + "/M_BulletTracerCore"

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDITOR_ASSETS = unreal.EditorAssetLibrary
MATERIAL_EDITOR = unreal.MaterialEditingLibrary


def ensure_directory(path):
    if not EDITOR_ASSETS.does_directory_exist(path):
        EDITOR_ASSETS.make_directory(path)


def ensure_smoke_texture():
    ensure_directory(TEXTURE_ROOT)
    if EDITOR_ASSETS.does_asset_exist(SMOKE_TEXTURE_PATH):
        return unreal.load_asset(SMOKE_TEXTURE_PATH)

    source = unreal.load_asset(SOURCE_SMOKE_TEXTURE)
    if not source:
        raise RuntimeError("Could not load engine smoke texture: " + SOURCE_SMOKE_TEXTURE)

    copied = EDITOR_ASSETS.duplicate_asset(SOURCE_SMOKE_TEXTURE, SMOKE_TEXTURE_PATH)
    if not copied:
        raise RuntimeError("Could not duplicate smoke texture to: " + SMOKE_TEXTURE_PATH)

    EDITOR_ASSETS.save_loaded_asset(copied, only_if_is_dirty=False)
    log("Copied smoke texture to " + SMOKE_TEXTURE_PATH)
    return copied


def try_set(obj, property_name, value):
    """Set optional editor properties without making the script version-fragile."""
    try:
        obj.set_editor_property(property_name, value)
    except Exception as exc:
        unreal.log_warning("[BulletTracer] Could not set {}.{}: {}".format(obj.get_name(), property_name, exc))


def configure_material(material, blend_mode, tint, default_opacity, use_static_mesh):
    try_set(material, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    try_set(material, "blend_mode", blend_mode)
    try_set(material, "two_sided", True)
    try_set(material, "used_with_static_lighting", False)
    try_set(material, "used_with_particle_sprites", True)
    try_set(material, "used_with_niagara_sprites", True)
    if use_static_mesh:
        try_set(material, "used_with_static_mesh", True)

    tint_node = MATERIAL_EDITOR.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -700, -120
    )
    tint_node.set_editor_property("parameter_name", "Tint")
    tint_node.set_editor_property("default_value", tint)

    opacity_node = MATERIAL_EDITOR.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -700, 120
    )
    opacity_node.set_editor_property("parameter_name", "Opacity")
    opacity_node.set_editor_property("default_value", default_opacity)

    return tint_node, opacity_node


def rebuild_material(path, name, tint, opacity, smoke_texture=None):
    ensure_directory(MATERIAL_ROOT)

    # Rebuild our own assets so re-running the script cannot accumulate duplicate nodes.
    if EDITOR_ASSETS.does_asset_exist(path):
        EDITOR_ASSETS.delete_asset(path)

    material = ASSET_TOOLS.create_asset(name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Failed to create material: " + path)
    log("Created material object " + material.get_path_name())

    tint_node, opacity_node = configure_material(
        material,
        unreal.BlendMode.BLEND_TRANSLUCENT if smoke_texture else unreal.BlendMode.BLEND_ADDITIVE,
        tint,
        opacity,
        use_static_mesh=not bool(smoke_texture),
    )

    if smoke_texture:
        texture_node = MATERIAL_EDITOR.create_material_expression(
            material, unreal.MaterialExpressionTextureSampleParameter2D, -950, 0
        )
        texture_node.set_editor_property("parameter_name", "SmokeTexture")
        texture_node.set_editor_property("texture", smoke_texture)

        tinted_rgb = MATERIAL_EDITOR.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -430, -120
        )
        MATERIAL_EDITOR.connect_material_expressions(texture_node, "RGB", tinted_rgb, "A")
        MATERIAL_EDITOR.connect_material_expressions(tint_node, "", tinted_rgb, "B")

        masked_opacity = MATERIAL_EDITOR.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -430, 120
        )
        MATERIAL_EDITOR.connect_material_expressions(texture_node, "A", masked_opacity, "A")
        MATERIAL_EDITOR.connect_material_expressions(opacity_node, "", masked_opacity, "B")

        if not MATERIAL_EDITOR.connect_material_property(
            tinted_rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        ):
            raise RuntimeError("Failed to connect smoke emissive color")
        if not MATERIAL_EDITOR.connect_material_property(
            masked_opacity, "", unreal.MaterialProperty.MP_OPACITY
        ):
            raise RuntimeError("Failed to connect smoke opacity")
    else:
        if not MATERIAL_EDITOR.connect_material_property(
            tint_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        ):
            raise RuntimeError("Failed to connect tracer emissive color")
        if not MATERIAL_EDITOR.connect_material_property(
            opacity_node, "", unreal.MaterialProperty.MP_OPACITY
        ):
            raise RuntimeError("Failed to connect tracer opacity")

    MATERIAL_EDITOR.recompile_material(material)
    EDITOR_ASSETS.save_loaded_asset(material, only_if_is_dirty=False)
    log("Saved material " + path)
    return material


def main():
    log("Starting bullet tracer material generation")
    smoke_texture = ensure_smoke_texture()

    rebuild_material(
        SMOKE_MATERIAL_PATH,
        "M_BulletSmoke",
        unreal.LinearColor(0.72, 0.76, 0.82, 1.0),
        0.12,
        smoke_texture=smoke_texture,
    )
    rebuild_material(
        CORE_MATERIAL_PATH,
        "M_BulletTracerCore",
        unreal.LinearColor(1.0, 0.86, 0.55, 1.0),
        0.62,
    )

    log("Created smoke texture and tracer materials under " + CONTENT_ROOT)


try:
    main()
except Exception:
    error = traceback.format_exc()
    log("FAILED:\n" + error)
    raise
