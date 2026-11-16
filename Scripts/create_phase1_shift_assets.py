import os
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory("/Game/Dev_Blueprint/VFX")
unreal.EditorAssetLibrary.make_directory("/Game/Dev_Blueprint/Audio")


def create_camera_shake():
    asset_path = "/Game/Dev_Blueprint/VFX/CS_DimensionShift"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        print(f"[Phase1] {asset_path} already exists.")
        return

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.DimensionShiftCameraShake)
    bp = asset_tools.create_asset("CS_DimensionShift", "/Game/Dev_Blueprint/VFX", unreal.Blueprint, factory)
    if bp:
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        print(f"[Phase1] Created {asset_path} (Parent: DimensionShiftCameraShake)")
    else:
        print(f"[Phase1] ERROR creating {asset_path}")


def create_audio_stinger():
    project_dir = unreal.Paths.project_dir()
    wav_path = os.path.abspath(os.path.join(project_dir, "Scripts", "SFX_DimensionShift.wav"))

    if not os.path.exists(wav_path):
        print(f"[Phase1] ERROR: Missing {wav_path}")
        return

    if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Dev_Blueprint/Audio/SFX_DimensionShift"):
        task = unreal.AssetImportTask()
        task.filename = wav_path
        task.destination_path = "/Game/Dev_Blueprint/Audio"
        task.destination_name = "SFX_DimensionShift"
        task.automated = True
        task.save = True
        task.replace_existing = True
        asset_tools.import_asset_tasks([task])
        print("[Phase1] Imported /Game/Dev_Blueprint/Audio/SFX_DimensionShift")

    sound_wave = unreal.load_asset("/Game/Dev_Blueprint/Audio/SFX_DimensionShift")
    cue_path = "/Game/Dev_Blueprint/Audio/Cue_DimensionShift"
    if unreal.EditorAssetLibrary.does_asset_exist(cue_path):
        print(f"[Phase1] {cue_path} already exists.")
        return

    cue_factory = unreal.SoundCueFactoryNew()
    cue = asset_tools.create_asset("Cue_DimensionShift", "/Game/Dev_Blueprint/Audio", unreal.SoundCue, cue_factory)
    if cue and sound_wave:
        wave_player = unreal.SoundNodeWavePlayer(cue)
        wave_player.set_editor_property("sound_wave", sound_wave)
        modulator = unreal.SoundNodeModulator(cue)
        modulator.set_editor_property("pitch_min", 0.94)
        modulator.set_editor_property("pitch_max", 1.06)
        modulator.set_editor_property("volume_min", 0.9)
        modulator.set_editor_property("volume_max", 1.0)
        modulator.set_editor_property("child_nodes", [wave_player])
        cue.set_editor_property("first_node", modulator)
        unreal.EditorAssetLibrary.save_loaded_asset(cue)
        print(f"[Phase1] Created {cue_path}")
    else:
        print(f"[Phase1] ERROR creating {cue_path}")


def create_post_process_material():
    mat_path = "/Game/Dev_Blueprint/VFX/M_PP_DimensionShift"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        print(f"[Phase1] {mat_path} already exists.")
        return

    factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset("M_PP_DimensionShift", "/Game/Dev_Blueprint/VFX", unreal.Material, factory)
    if not mat:
        print(f"[Phase1] ERROR creating {mat_path}")
        return

    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mel = unreal.MaterialEditingLibrary

    # Scalar Parameter: GlitchIntensity (0.0 -> 1.0)
    glitch_param = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 0)
    glitch_param.set_editor_property("parameter_name", "GlitchIntensity")
    glitch_param.set_editor_property("default_value", 0.0)

    # UV offset = GlitchIntensity * 0.015 along X
    offset_scale = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -720, -100)
    offset_const = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -900, -160)
    offset_const.set_editor_property("r", 0.015)
    mel.connect_material_expressions(glitch_param, "", offset_scale, "A")
    mel.connect_material_expressions(offset_const, "", offset_scale, "B")

    # Append (Offset, 0.0) -> float2
    zero_const = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -720, -220)
    zero_const.set_editor_property("r", 0.0)
    offset_vec = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -540, -140)
    mel.connect_material_expressions(offset_scale, "", offset_vec, "A")
    mel.connect_material_expressions(zero_const, "", offset_vec, "B")

    # ScreenPosition UV
    tex_coord = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -720, 140)

    # UV + Offset (Red channel) and UV - Offset (Blue channel)
    uv_plus = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -360, -80)
    mel.connect_material_expressions(tex_coord, "", uv_plus, "A")
    mel.connect_material_expressions(offset_vec, "", uv_plus, "B")

    uv_minus = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -360, 260)
    mel.connect_material_expressions(tex_coord, "", uv_minus, "A")
    mel.connect_material_expressions(offset_vec, "", uv_minus, "B")

    # 3 SceneTexture:PostProcessInput0 nodes (R, G, B)
    st_r = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -180, -120)
    st_r.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    mel.connect_material_expressions(uv_plus, "", st_r, "UVs")

    st_g = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -180, 80)
    st_g.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)

    st_b = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -180, 280)
    st_b.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    mel.connect_material_expressions(uv_minus, "", st_b, "UVs")

    # Mask R, G, B individually and recombine
    mask_r = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 20, -120)
    mask_r.set_editor_property("r", True)
    mask_r.set_editor_property("g", False)
    mask_r.set_editor_property("b", False)
    mel.connect_material_expressions(st_r, "Color", mask_r, "")

    mask_g = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 20, 80)
    mask_g.set_editor_property("r", False)
    mask_g.set_editor_property("g", True)
    mask_g.set_editor_property("b", False)
    mel.connect_material_expressions(st_g, "Color", mask_g, "")

    mask_b = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 20, 280)
    mask_b.set_editor_property("r", False)
    mask_b.set_editor_property("g", False)
    mask_b.set_editor_property("b", True)
    mel.connect_material_expressions(st_b, "Color", mask_b, "")

    append_rg = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, 200, -20)
    mel.connect_material_expressions(mask_r, "", append_rg, "A")
    mel.connect_material_expressions(mask_g, "", append_rg, "B")

    ca_rgb = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, 380, 80)
    mel.connect_material_expressions(append_rg, "", ca_rgb, "A")
    mel.connect_material_expressions(mask_b, "", ca_rgb, "B")

    # Subtle cyan/emerald shift color pulse scaled by GlitchIntensity * 0.28
    pulse_color = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, 180, 420)
    pulse_color.set_editor_property("constant", unreal.LinearColor(0.15, 0.95, 0.85, 1.0))

    pulse_strength = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 180, 560)
    pulse_factor = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 0, 580)
    pulse_factor.set_editor_property("r", 0.28)
    mel.connect_material_expressions(glitch_param, "", pulse_strength, "A")
    mel.connect_material_expressions(pulse_factor, "", pulse_strength, "B")

    pulse_tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 380, 440)
    mel.connect_material_expressions(pulse_color, "", pulse_tint, "A")
    mel.connect_material_expressions(pulse_strength, "", pulse_tint, "B")

    final_add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, 560, 160)
    mel.connect_material_expressions(ca_rgb, "", final_add, "A")
    mel.connect_material_expressions(pulse_tint, "", final_add, "B")

    mel.connect_material_property(final_add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    print(f"[Phase1] Created {mat_path}")


if __name__ == "__main__":
    create_camera_shake()
    create_audio_stinger()
    create_post_process_material()
