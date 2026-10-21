"""Phase 0 spike (P0-5 at runtime): skin tone through the material, set per character.

    SpikeSkinToneRuntime.py material - add VaesenToneScale/VaesenToneOffset to M_skin_unified_baked (recompiles shaders)
    SpikeSkinToneRuntime.py table    - sample MetaHuman's tone model (bias, gain) on a grid: /Game/Spike/Appearance/ST_SkinTones
    SpikeSkinToneRuntime.py presets  - base tones on FP_SpikeFace_A and a new FP_SpikeFace_B, linked from the spike preset table
    SpikeSkinToneRuntime.py level    - /Game/Spike/Maps/L_SpikeSkinToneRuntime: face A at fair, medium and deep tones, face B at fair
"""
import json
import sys

import unreal

MATERIAL = "/Game/MetaHumans/Common/Lookdev_UHM/Skin/Materials/M_skin_unified_baked"
TABLE = "/Game/Spike/Appearance/ST_SkinTones"
PRESETS = "/Game/Spike/Appearance/DT_AppearancePresets_Spike"
RESOLUTION = 9
EAL = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def vec(v):
    return unreal.Vector3f(v.x, v.y, v.z)


def bias_gain(character):
    skin = character.get_editor_property("skin_settings").get_editor_property("skin")
    return skin.get_editor_property("body_bias"), skin.get_editor_property("body_gain")


def material():
    print(unreal.VaesenSpikeSliderLibrary.add_skin_tone_parameters(unreal.load_asset(MATERIAL), ["Basecolor Baked VT", "Basecolor Baked"]))


def table():
    probe_path = "/Game/Spike/Appearance/Temp_ToneProbe"
    if EAL.does_asset_exist(probe_path):
        EAL.delete_asset(probe_path)
    probe = EAL.duplicate_asset("/Game/Spike/MetaHumans/SpikeFace_A", probe_path)
    biases, gains, valid = [], [], []
    if not sub.try_add_object_to_edit(probe):
        raise RuntimeError("can't edit the probe character")
    try:
        for j in range(RESOLUTION):
            for i in range(RESOLUTION):
                u, v = i / (RESOLUTION - 1), j / (RESOLUTION - 1)
                settings = probe.get_editor_property("skin_settings")
                skin = settings.get_editor_property("skin")
                skin.set_editor_property("u", u)
                skin.set_editor_property("v", v)
                settings.set_editor_property("skin", skin)
                sub.commit_skin_settings(probe, settings)
                b, g = bias_gain(probe)
                # An invalid tone gives equal channels; real skin is red > green > blue.
                ok = b.x > b.y + 5 and b.y > b.z
                biases.append(vec(b))
                gains.append(vec(g))
                valid.append(ok)
    finally:
        sub.remove_object_to_edit(probe)
    EAL.delete_asset(probe_path)

    if EAL.does_asset_exist(TABLE):
        EAL.delete_asset(TABLE)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset("ST_SkinTones", "/Game/Spike/Appearance", unreal.VaesenSkinToneTable, unreal.DataAssetFactory())
    asset.set_editor_property("resolution", RESOLUTION)
    asset.set_editor_property("bias", biases)
    asset.set_editor_property("gain", gains)
    asset.set_editor_property("valid", valid)
    EAL.save_loaded_asset(asset)
    print(f"[TONE] table: {RESOLUTION}x{RESOLUTION}, {sum(valid)} valid; bias at (0,0.5) {biases[(RESOLUTION // 2) * RESOLUTION]}, at (1,0.5) {biases[(RESOLUTION // 2) * RESOLUTION + RESOLUTION - 1]}")


def presets():
    def base_tone(preset_path, character_path, name):
        preset = unreal.load_asset(preset_path)
        if preset is None:
            preset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, "/Game/Spike/Appearance", unreal.VaesenFacePresetData, unreal.DataAssetFactory())
        b, g = bias_gain(unreal.load_asset(character_path))
        preset.set_editor_property("base_skin_bias", vec(b))
        preset.set_editor_property("base_skin_gain", vec(g))
        EAL.save_loaded_asset(preset)
        print(f"[TONE] {name}: base bias {b}, gain {g}")
        return preset

    # Face, its ID in the preset table.
    for face, face_id in (("SpikeFace_A", "face_f_spike_a"), ("SpikeFace_B", "face_f_spike_b"),
                          ("SpikeFace_M_A", "face_m_spike_a"), ("SpikeFace_M_B", "face_m_spike_b")):
        if not EAL.does_asset_exist(f"/Game/Spike/MetaHumans/{face}"):
            continue
        preset = base_tone(f"/Game/Spike/Appearance/FP_{face}", f"/Game/Spike/MetaHumans/{face}", f"FP_{face}")
        preset.set_editor_property("face_preset_id", face_id)
        EAL.save_loaded_asset(preset)
    table_asset = unreal.load_asset(PRESETS)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table_asset)) if hasattr(unreal.DataTableFunctionLibrary, "export_data_table_to_json_string") else None
    if rows is None:
        rows = json.loads(table_asset.export_to_json_string())
    for row in rows:
        if row["Name"] == "face_f_spike_b":
            row["FacePreset"] = "/Game/Spike/Appearance/FP_SpikeFace_B.FP_SpikeFace_B"
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table_asset, json.dumps(rows))
    EAL.save_loaded_asset(table_asset)
    print("[TONE] face_f_spike_b now has its face preset")


def level():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    path = "/Game/Spike/Maps/L_SpikeSkinToneRuntime"
    if EAL.does_asset_exist(path):
        levels.load_level(path)
        for actor in actors.get_all_level_actors():
            actors.destroy_actor(actor)
    else:
        levels.new_level(path)

    def spawn(cls, location, rotation=unreal.Rotator(), label=None):
        actor = actors.spawn_actor_from_class(cls, location, rotation)
        if actor and label:
            actor.set_actor_label(label)
        return actor

    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-25, yaw=20), "Sun")
    sun.light_component.set_intensity(6.0)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 300), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), label="Floor")
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
    floor.set_actor_scale3d(unreal.Vector(20, 20, 1))

    presets_table = unreal.load_asset(PRESETS)
    tones = unreal.load_asset(TABLE)
    row = [("A_fair", "face_f_spike_a", (0.1, 0.5)), ("A_as_built", "face_f_spike_a", (0.25, 0.55)), ("A_medium", "face_f_spike_a", (0.55, 0.45)),
           ("A_deep", "face_f_spike_a", (0.85, 0.4)), ("B_fair", "face_f_spike_b", (0.15, 0.5))]
    for index, (label, face, uv) in enumerate(row):
        y = (index - (len(row) - 1) / 2) * 45.0
        look = unreal.VaesenAppearance()
        look.set_editor_property("appearance_version", 1)
        look.set_editor_property("sex", "female")
        look.set_editor_property("face_preset_id", face)
        # FVector2f has no fields in Python; set it from text.
        tone = unreal.Vector2f()
        tone.import_text(f"(X={uv[0]},Y={uv[1]})")
        look.set_editor_property("skin_tone", tone)
        dresser = spawn(unreal.VaesenSpikeAppearanceActor, unreal.Vector(0, y, 0), unreal.Rotator(roll=0, pitch=0, yaw=90), label)
        dresser.set_editor_property("test_appearance", look)
        dresser.set_editor_property("apply_delay", 0.5)
        dresser.appearance.set_editor_property("preset_table", presets_table)
        dresser.appearance.set_editor_property("skin_tone_table", tones)
    capture = spawn(unreal.SceneCapture2D, unreal.Vector(-130, 0, 163), unreal.Rotator(roll=0, pitch=-2, yaw=0), "Capture_CloseCamera")
    capture.capture_component2d.set_editor_property("capture_every_frame", False)
    capture.capture_component2d.set_editor_property("capture_on_movement", False)
    levels.save_current_level()
    print(f"[TONE] level saved: {path}")


globals()[sys.argv[1]]()
