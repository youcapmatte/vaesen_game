"""Phase 0 spike (P0-10, and P0-4 in a packaged build): 24 MetaHumans for performance measurement.

A 6 x 4 grid of SpikeFace_A and SpikeFace_B (alternating), 1.2 m apart, facing a camera about 4 m from the first row
(roughly a third-person view of a busy village square). Every face plays the face range-of-motion animation through
an AVaesenSpikeFaceDriver at normal LOD behaviour; two A faces also get the jaw-width slider with its joint offsets
(the P0-4 check, logged as "[P0-4]" lines). Plain game mode (no login), a spectator pawn at the camera position.

Creates or replaces /Game/Spike/Maps/L_SpikeBenchmark. Measure with:
    VaesenGame.exe /Game/Spike/Maps/L_SpikeBenchmark -ExecCmds="Vaesen.Spike.Crowd 0 none 30 -1 1"
"""
import os
import sys

import unreal

# "empty": the same scene without the characters (/Game/Spike/Maps/L_SpikeBenchmarkEmpty), the baseline to subtract.
EMPTY = len(sys.argv) > 1 and sys.argv[1] == "empty"
LEVEL = "/Game/Spike/Maps/L_SpikeBenchmarkEmpty" if EMPTY else "/Game/Spike/Maps/L_SpikeBenchmark"
CLASSES = [
    "/Game/Spike/MetaHumans/Built/SpikeFace_A/BP_SpikeFace_A.BP_SpikeFace_A_C",
    "/Game/Spike/MetaHumans/SpikeFace_B/BP_SpikeFace_B.BP_SpikeFace_B_C",
]
ROM = "/MetaHumanCharacter/Optional/Animation/TemplateAnimations/Technical_Loops/FaceROM/mhc_mh001_fmn_rom_face"
DNA_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Spike", "DNA")
COLUMNS, ROWS, SPACING = 6, 4, 120.0

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(cls, location, rotation=unreal.Rotator(), label=None):
    actor = actors.spawn_actor_from_class(cls, location, rotation)
    if actor and label:
        actor.set_actor_label(label)
    return actor


# Reuse the level if it exists (an open level can't be deleted), emptied; otherwise create it.
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    levels.load_level(LEVEL)
    for actor in actors.get_all_level_actors():
        actors.destroy_actor(actor)
else:
    levels.new_level(LEVEL)

sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-35, yaw=140), "Sun")
sun.light_component.set_intensity(6.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 300), label="SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), label="Fog")
floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), label="Floor")
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
floor.set_actor_scale3d(unreal.Vector(40, 40, 1))

classes = [unreal.load_class(None, c) for c in CLASSES]
entries = []
for row in range(0 if EMPTY else ROWS):
    for column in range(COLUMNS):
        index = row * COLUMNS + column
        location = unreal.Vector(row * SPACING, (column - (COLUMNS - 1) / 2) * SPACING, 0)
        actor = spawn(classes[index % 2], location, unreal.Rotator(roll=0, pitch=0, yaw=90), f"Crowd_{index:02d}")
        entry = unreal.VaesenSpikeFaceSlider()
        entry.target = actor
        entry.weight = 1.0
        # Two A faces in the front row carry the jaw slider and its joint offsets (P0-4 in a cooked build).
        if index in (0, 2):
            direction = "pos" if index == 0 else "neg"
            entry.morph = f"slider_jawWidth_{direction}_j"
            entry.joint_offsets_file = os.path.join(DNA_DIR, f"slider_jawWidth_{direction}_j.json")
        entries.append(entry)

driver = None if EMPTY else spawn(unreal.VaesenSpikeFaceDriver, unreal.Vector(0, 0, 0), label="FaceDriver")
if driver:
    driver.set_editor_property("faces", entries)
    driver.set_editor_property("face_animation", unreal.load_asset(ROM))
    driver.set_editor_property("force_full_detail", False)

# The viewpoint: about 4 m in front of the first row at eye height, looking at the grid.
spawn(unreal.PlayerStart, unreal.Vector(-400, 0, 170), unreal.Rotator(roll=0, pitch=-8, yaw=0), "PlayerStart")
spawn(unreal.CameraActor, unreal.Vector(-400, 0, 170), unreal.Rotator(roll=0, pitch=-8, yaw=0), "BenchmarkCamera")

# Plain game mode: no login, a spectator pawn at the player start.
world_settings = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
world_settings.set_editor_property("default_game_mode", unreal.GameModeBase)

levels.save_current_level()
print(f"[P0-10] level saved: {LEVEL} with {len(entries)} MetaHumans")
