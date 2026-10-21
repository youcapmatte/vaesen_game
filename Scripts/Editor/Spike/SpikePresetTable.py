"""Phase 0 spike: a DT_AppearancePresets with the spike content, and its export to the API.

Creates /Game/Spike/Appearance/DT_AppearancePresets_Spike (rows of FVaesenAppearancePresetRow; the row name is the
stored ID) and writes UVaesenAppearanceLibrary.to_api_options_json(table) to Saved/Spike/appearance-presets.json, in
the shape the API reads from VaesenAPI/Api/Vaesen.Api/appearance-presets.json as its allowed lists.
"""
import json
import os

import unreal

TABLE = "/Game/Spike/Appearance/DT_AppearancePresets_Spike"
# The game uses this table (Project Settings > Vaesen > Characters > Appearance), so the API allows exactly what it offers.
API_FILE = os.path.normpath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "..", "VaesenAPI", "Api", "Vaesen.Api", "appearance-presets.json"))


def groom(folder, name):
    return f"/MetaHumanCharacter/Optional/Grooms/GroomAssets/{folder}/{name}/{name}.{name}"


def row(name, kind, sex="", **values):
    entry = {"Name": name, "Kind": kind, "Sex": sex, "bPlayerSelectable": True, "RandomWeight": 1.0}
    entry.update(values)
    return entry


ROWS = [
    row("face_f_spike_a", "Face", "female",
        MetaHumanClass="/Game/Spike/MetaHumans/Built/SpikeFace_A/BP_SpikeFace_A.BP_SpikeFace_A_C",
        FacePreset="/Game/Spike/Appearance/FP_SpikeFace_A.FP_SpikeFace_A"),
    row("face_f_spike_b", "Face", "female",
        MetaHumanClass="/Game/Spike/MetaHumans/SpikeFace_B/BP_SpikeFace_B.BP_SpikeFace_B_C",
        FacePreset="/Game/Spike/Appearance/FP_SpikeFace_B.FP_SpikeFace_B"),
    # Male faces from the MetaHuman presets Victor and Lorenzo (MetaHumanBaseFaces.py).
    row("face_m_spike_a", "Face", "male",
        MetaHumanClass="/Game/Spike/MetaHumans/Built/SpikeFace_M_A/BP_SpikeFace_M_A.BP_SpikeFace_M_A_C",
        FacePreset="/Game/Spike/Appearance/FP_SpikeFace_M_A.FP_SpikeFace_M_A"),
    row("face_m_spike_b", "Face", "male",
        MetaHumanClass="/Game/Spike/MetaHumans/Built/SpikeFace_M_B/BP_SpikeFace_M_B.BP_SpikeFace_M_B_C",
        FacePreset="/Game/Spike/Appearance/FP_SpikeFace_M_B.FP_SpikeFace_M_B"),
    row("body_f_spike", "Body", "female"),
    row("body_m_spike", "Body", "male"),
    # Grooms from the MetaHuman plugin, bound to every face of the row's sex by SpikeGrooms.py. A hairstyle row
    # without an asset is bald; a facial hair row's groom goes on the moustache or the beard component by its name.
    *[row(f"hair_f_{name.lower()}", "HairStyle", "female", Asset=groom("Hair", f"Hair_{name}"))
      for name in ("S_UpdoBuns", "S_Updo", "S_UpdoBraids", "S_LowPonytail", "S_PulledBack", "L_Straight")],
    row("hair_f_bald", "HairStyle", "female", RandomWeight=0.0),
    *[row(f"hair_m_{name.lower()}", "HairStyle", "male", Asset=groom("Hair", f"Hair_{name}"))
      for name in ("S_SlickBack", "S_PulledBack", "S_Casual", "S_BrushCut", "S_SideSweptFringe", "M_Layered", "S_RecedeMessy", "S_BaldingStubble")],
    row("hair_m_bald", "HairStyle", "male", RandomWeight=0.2),
    *[row(f"brow_{sex[0]}_{name.lower()}", "Eyebrows", sex, Asset=groom("Eyebrows", f"Eyebrows_{name}"))
      for sex in ("female", "male") for name in ("M_Natural", "M_Full", "M_Dense", "M_Thin", "S_FlatThin")],
    *[row(f"facial_m_{name.lower()}", "FacialHair", "male", Asset=groom(folder, name))
      for folder, name in (("Mustaches", "Mustache_L_Handlebar"), ("Mustaches", "Mustache_M_Gunslinger"), ("Mustaches", "Mustache_S_Full"),
                           ("Mustaches", "Mustache_M_Curly"), ("Mustaches", "Mustache_S_Horseshoe"), ("Beards", "Beard_M_MuttonChops"),
                           ("Beards", "Beard_S_Full"), ("Beards", "Beard_L_Full"), ("Beards", "Beard_S_Stubble"), ("Beards", "Goatee_M_Pointed"))],
    # Iris textures recoloured from the baked brown iris (Saved/Spike/Eyes); the built eyes have no colour parameters.
    row("eye_blue", "EyeColor", Asset="/Game/Spike/Appearance/Eyes/T_Iris_blue.T_Iris_blue"),
    row("eye_grey_blue", "EyeColor", Asset="/Game/Spike/Appearance/Eyes/T_Iris_grey_blue.T_Iris_grey_blue"),
    row("eye_grey", "EyeColor", Asset="/Game/Spike/Appearance/Eyes/T_Iris_grey.T_Iris_grey"),
    row("eye_green", "EyeColor", Asset="/Game/Spike/Appearance/Eyes/T_Iris_green.T_Iris_green"),
    row("eye_hazel", "EyeColor", Asset="/Game/Spike/Appearance/Eyes/T_Iris_hazel.T_Iris_hazel"),
    row("eye_brown", "EyeColor", Asset="/Game/Spike/Appearance/Eyes/T_Iris_brown.T_Iris_brown"),
    row("eye_dark_brown", "EyeColor", Asset="/Game/Spike/Appearance/Eyes/T_Iris_dark_brown.T_Iris_dark_brown"),
    row("skin_fair", "SkinTone", SkinTone={"X": 0.25, "Y": 0.55}),
    row("skin_medium", "SkinTone", SkinTone={"X": 0.55, "Y": 0.45}),
    row("skin_deep", "SkinTone", SkinTone={"X": 0.85, "Y": 0.40}),
    # Tuned by eye (Saved/Spike/HairColour), with the material's white strands off (its default is 0.3): the material
    # caps redness at 1 and tints in proportion to melanin, so blonds need some redness and reds melanin near 0.45.
    row("haircolor_black", "HairColor", HairMelanin=1.0, HairRedness=0.05),
    row("haircolor_dark_brown", "HairColor", HairMelanin=0.88, HairRedness=0.2),
    row("haircolor_brown", "HairColor", HairMelanin=0.75, HairRedness=0.3),
    row("haircolor_light_brown", "HairColor", HairMelanin=0.58, HairRedness=0.35),
    row("haircolor_blond", "HairColor", HairMelanin=0.4, HairRedness=0.05),
    row("haircolor_light_blond", "HairColor", HairMelanin=0.3, HairRedness=0.5),
    row("haircolor_ash_brown", "HairColor", HairMelanin=0.55, HairRedness=0.02),
    row("haircolor_red", "HairColor", HairMelanin=0.45, HairRedness=1.0),
    row("haircolor_auburn", "HairColor", HairMelanin=0.65, HairRedness=0.85),
    row("haircolor_greying", "HairColor", HairMelanin=0.7, HairRedness=0.15, HairGrey=0.5, RandomWeight=0.3),
    row("haircolor_grey", "HairColor", HairMelanin=0.6, HairRedness=0.15, HairGrey=1.0, RandomWeight=0.3),
    row("haircolor_white", "HairColor", HairMelanin=0.3, HairRedness=0.1, HairGrey=1.0, RandomWeight=0.1),    row("jawWidth", "Slider"),
    row("noseWidth", "Slider"),
    row("cheekboneHeight", "Slider"),
]

EAL = unreal.EditorAssetLibrary
# Refilled in place: deleting and recreating it would clear the references of every loaded level and actor using it.
if EAL.does_asset_exist(TABLE):
    table = unreal.load_asset(TABLE)
else:
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", unreal.VaesenAppearancePresetRow.static_struct())
    table = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DT_AppearancePresets_Spike", "/Game/Spike/Appearance", unreal.DataTable, factory)
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(ROWS))
EAL.save_loaded_asset(table)
names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
print(f"[PRESETS] table filled: {ok}, {len(names)} rows")

exported = unreal.VaesenAppearanceLibrary.to_api_options_json(table)
with open(API_FILE, "w", encoding="utf-8") as f:
    json.dump(json.loads(exported), f, indent=2)
print(f"[PRESETS] exported to {API_FILE}: {exported}")
