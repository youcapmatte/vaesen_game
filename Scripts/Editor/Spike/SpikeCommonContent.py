"""The Vaesen changes to MetaHuman common content, applied to every copy of it in the project.

MetaHuman builds copy common content into the build's common folder, and faces built to different folders reference
different copies; a build can also restore the stock assets. Run this after every MetaHuman build. Both changes are
skipped where already present:
- M_skin_unified_baked: VaesenToneScale / VaesenToneOffset at the material output (runtime skin tone), then the age
  layer (the normal blended toward a young and an old face's baked normal; the face presets pick the skins);
- ABP_Face_PostProcess: the Vaesen Face Joint Offsets node after the face rig (slider joint offsets, age wrinkles).
Saving the material recompiles the skin shaders.
"""
import unreal

EAL = unreal.EditorAssetLibrary
# The age layer's default skins (the face presets set their own): a young face and an old one.
YOUNG_NORMAL = "/Game/Spike/MetaHumans/Built/SpikeFace_A/Face/Baked/T_Head_LOD1_N_VT"
OLD_NORMAL = "/Game/Spike/MetaHumans/Built/SpikeFace_M_B/Face/Baked/T_Head_LOD1_N_VT"
YOUNG_COLOR = "/Game/Spike/MetaHumans/Built/SpikeFace_A/Face/Baked/T_Head_LOD1_BC_VT"
OLD_COLOR = "/Game/Spike/MetaHumans/Built/SpikeFace_M_B/Face/Baked/T_Head_LOD1_BC_VT"
COMMON_FOLDERS = ["/Game/MetaHumans/Common", "/Game/Spike/MetaHumans/Built/Common"]

for common in COMMON_FOLDERS:
    material_path = f"{common}/Lookdev_UHM/Skin/Materials/M_skin_unified_baked"
    abp_path = f"{common}/Face/ABP_Face_PostProcess"
    if EAL.does_asset_exist(material_path):
        material = unreal.load_asset(material_path)
        print(f"[COMMON] {material_path}: {unreal.VaesenSpikeSliderLibrary.add_skin_tone_parameters(material, ['Basecolor Baked VT', 'Basecolor Baked'])}")
        print(f"[COMMON] {material_path}: {unreal.VaesenSpikeSliderLibrary.add_skin_tone_floor(material)}")
        print(f"[COMMON] {material_path}: {unreal.VaesenSpikeSliderLibrary.add_skin_age_layer(material, unreal.load_asset(YOUNG_NORMAL), unreal.load_asset(OLD_NORMAL))}")
        print(f"[COMMON] {material_path}: {unreal.VaesenSpikeSliderLibrary.add_skin_age_color(material, unreal.load_asset(YOUNG_COLOR), unreal.load_asset(OLD_COLOR))}")
        EAL.save_loaded_asset(material, only_if_is_dirty=True)
    if EAL.does_asset_exist(abp_path):
        abp = unreal.load_asset(abp_path)
        print(f"[COMMON] {abp_path}: {unreal.VaesenSpikeSliderLibrary.insert_face_joint_offsets_node(abp)}")
        EAL.save_loaded_asset(abp, only_if_is_dirty=True)
