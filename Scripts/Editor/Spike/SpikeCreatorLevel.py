"""Phase 1 groundwork check: the character creator scene (AVaesenCharacterCreatorScene) in a test level.

    SpikeCreatorLevel.py level - /Game/Spike/Maps/L_SpikeCreator: the scene actor with the spike preset and tone tables,
                                 fixed lights, and a scene capture for checks
    SpikeCreatorLevel.py drive <folder> - while simulating: open the creator, capture the body framing, go to the Face
                                 step and capture the face framing, then Randomise and capture again (PNG files)
"""
import math
import sys

import unreal

LEVEL = "/Game/Spike/Maps/L_SpikeCreator"


def level():
    EAL = unreal.EditorAssetLibrary
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
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

    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-25, yaw=20), "KeyLight")
    sun.light_component.set_intensity(6.0)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 300), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), label="Floor")
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
    floor.set_actor_scale3d(unreal.Vector(20, 20, 1))

    scene = spawn(unreal.VaesenCharacterCreatorScene, unreal.Vector(0, 0, 0), label="CreatorScene")
    scene.set_editor_property("presets", unreal.load_asset("/Game/Spike/Appearance/DT_AppearancePresets_Spike"))
    scene.set_editor_property("skin_tone_table", unreal.load_asset("/Game/Spike/Appearance/ST_SkinTones"))
    # Press Play to look at it: the creator's game mode (no player character or HUD) opens it, with test keys.
    # Simulate has no player; drive it with "drive".
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings().set_editor_property(
        "default_game_mode", unreal.VaesenCharacterCreatorGameMode)
    capture = spawn(unreal.SceneCapture2D, unreal.Vector(-330, 0, 110), label="Capture_Creator")
    capture.capture_component2d.set_editor_property("capture_every_frame", False)
    capture.capture_component2d.set_editor_property("capture_on_movement", False)
    levels.save_current_level()
    print(f"[CREATOR] level saved: {LEVEL}")


def capture_from_camera(world, scene, folder, name):
    cap = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D) if a.get_actor_label() == "Capture_Creator")
    camera = scene.camera
    cap.set_actor_location_and_rotation(camera.get_world_location(), camera.get_world_rotation(), False, False)
    comp = cap.capture_component2d
    target = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    comp.set_editor_property("texture_target", target)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    # The camera's horizontal field of view is set for the game viewport's shape; derive it for this 16:9 capture.
    half = math.radians(scene.get_editor_property("vertical_field_of_view") / 2)
    comp.set_editor_property("fov_angle", math.degrees(2 * math.atan(math.tan(half) * 16 / 9)))
    # Grooms update when the editor viewport draws, which a background editor doesn't do on its own; ask for a redraw.
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    for _ in range(4):
        levels.editor_invalidate_viewports()
        comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, target, folder, f"{name}.png")


def drive(folder, stage):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    scene = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.VaesenCharacterCreatorScene))
    creator = scene.get_creator()
    if stage == "open":
        scene.open(None, "female")
        creator = scene.get_creator()
        creator.set_class("Occultist")
        print(f"[CREATOR] opened: step {creator.get_step()}, framing {creator.get_framing()}, face {creator.get_appearance().get_editor_property('face_preset_id')}")
    elif stage == "body":
        capture_from_camera(world, scene, folder, "creator_body")
        print("[CREATOR] body framing captured")
    elif stage == "toface":
        creator.next()
        creator.next()
        print(f"[CREATOR] step {creator.get_step()}, framing {creator.get_framing()}")
    elif stage == "face":
        capture_from_camera(world, scene, folder, "creator_face")
        print("[CREATOR] face framing captured")
    elif stage == "randomise":
        creator.randomise(3)
        a = creator.get_appearance()
        print(f"[CREATOR] randomised: face {a.get_editor_property('face_preset_id')}, age {a.get_editor_property('age'):.2f}, hair melanin {a.get_editor_property('hair_melanin'):.2f}")
    elif stage == "random":
        scene.add_turntable_yaw(25.0)
        capture_from_camera(world, scene, folder, "creator_random")
        print("[CREATOR] randomised face captured (turned 25 degrees)")


if sys.argv[1] == "level":
    level()
else:
    drive(sys.argv[2], sys.argv[3])
