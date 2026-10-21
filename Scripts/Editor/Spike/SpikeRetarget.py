"""Phase 0 spike (P0-9): mannequin animations on the MetaHuman body through an IK Retargeter.

Creates /Game/Spike/Retarget/RTG_Mannequin_to_MetaHuman (source IK_Mannequin, target the MetaHuman plugin's
IK_MH_IKRig, chains mapped automatically), retargets a few mannequin animations onto SpikeFace_A's body skeleton, and
builds /Game/Spike/Maps/L_SpikeAnim with SpikeFace_A once per animation. Press Simulate to see them move.
"""
import unreal

OUT = "/Game/Spike/Retarget"
RTG = f"{OUT}/RTG_Mannequin_to_MetaHuman"
LEVEL = "/Game/Spike/Maps/L_SpikeAnim"
SOURCE_RIG = "/Game/Characters/Mannequins/Rigs/IK_Mannequin"
TARGET_RIG = "/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig"
SOURCE_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny"
TARGET_MESH = "/Game/Spike/MetaHumans/Built/SpikeFace_A/Body/SKM_SpikeFace_A_BodyMesh"
CLASS_A = "/Game/Spike/MetaHumans/Built/SpikeFace_A/BP_SpikeFace_A.BP_SpikeFace_A_C"
ANIMATIONS = [
    "/Game/Characters/Mannequins/Animations/Manny/MM_Idle",
    "/Game/Characters/Mannequins/Animations/Manny/MM_Walk_InPlace",
    "/Game/Characters/Mannequins/Animations/Manny/MM_Run_Fwd",
]

EAL = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 1. The retargeter.
if EAL.does_asset_exist(RTG):
    EAL.delete_asset(RTG)
retargeter = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "RTG_Mannequin_to_MetaHuman", OUT, unreal.IKRetargeter, unreal.IKRetargetFactory())
controller = unreal.IKRetargeterController.get_controller(retargeter)
controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, unreal.load_asset(SOURCE_RIG))
controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, unreal.load_asset(TARGET_RIG))
controller.add_default_ops()
controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, unreal.load_asset(SOURCE_RIG))
controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, unreal.load_asset(TARGET_RIG))
controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
EAL.save_loaded_asset(retargeter)
print(f"[P0-9] retargeter: {RTG} ({controller.get_num_retarget_ops()} ops)")

# 2. Retarget the animations onto A's body.
animations = [unreal.load_asset(path) for path in ANIMATIONS]
for path in ANIMATIONS:
    old = f"{OUT}/{path.split('/')[-1]}_MH"
    if EAL.does_asset_exist(old):
        EAL.delete_asset(old)
inputs = unreal.IKRetargetBatchOperationInputs()
inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(path) for path in ANIMATIONS]
inputs.ik_retarget_asset = retargeter
inputs.source_mesh = unreal.load_asset(SOURCE_MESH)
inputs.target_mesh = unreal.load_asset(TARGET_MESH)
inputs.suffix = "_MH"
inputs.use_source_path = False
inputs.target_path = OUT
inputs.overwrite_existing_files = True
inputs.include_referenced_assets = False
results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
retargeted = []
for asset_data in results:
    asset = asset_data.get_asset()
    if not isinstance(asset, unreal.AnimSequence):
        continue
    EAL.save_loaded_asset(asset)
    retargeted.append(asset)
    print(f"[P0-9] retargeted: {asset.get_path_name()} on {asset.get_editor_property('skeleton').get_name()}")

# 3. The test level: A once per animation, each driven by its own face driver (body animation only).
if EAL.does_asset_exist(LEVEL):
    levels.load_level(LEVEL)
    for actor in actors.get_all_level_actors():
        actors.destroy_actor(actor)
else:
    levels.new_level(LEVEL)


def spawn(cls, location, rotation=unreal.Rotator(), label=None):
    actor = actors.spawn_actor_from_class(cls, location, rotation)
    if actor and label:
        actor.set_actor_label(label)
    return actor


sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-35, yaw=140), "Sun")
sun.light_component.set_intensity(6.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 300), label="SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), label="Floor")
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
floor.set_actor_scale3d(unreal.Vector(40, 40, 1))

face_class = unreal.load_class(None, CLASS_A)
for index, animation in enumerate(retargeted):
    y = (index - (len(retargeted) - 1) / 2) * 120.0
    label = animation.get_name()
    actor = spawn(face_class, unreal.Vector(0, y, 0), unreal.Rotator(roll=0, pitch=0, yaw=90), label)
    entry = unreal.VaesenSpikeFaceSlider()
    entry.target = actor
    driver = spawn(unreal.VaesenSpikeFaceDriver, unreal.Vector(0, y, 0), label=f"Driver_{label}")
    driver.set_editor_property("faces", [entry])
    driver.set_editor_property("body_animation", animation)
    driver.set_editor_property("force_full_detail", False)
    text = spawn(unreal.TextRenderActor, unreal.Vector(-5, y, 195), unreal.Rotator(roll=0, pitch=0, yaw=180), f"Label_{label}")
    text.text_render.set_editor_property("text", label)
    text.text_render.set_editor_property("world_size", 6.0)
    text.text_render.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)

spawn(unreal.CameraActor, unreal.Vector(-450, 0, 150), unreal.Rotator(roll=0, pitch=-5, yaw=0), "AnimCamera")
spawn(unreal.PlayerStart, unreal.Vector(-450, 0, 100), label="PlayerStart")
levels.save_current_level()
print(f"[P0-9] level saved: {LEVEL} with {len(retargeted)} animations")
