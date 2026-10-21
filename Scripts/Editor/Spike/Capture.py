"""Renders the open level from a camera actor into a PNG with a scene capture (works while the editor is in the
background, unlike a viewport screenshot).

    Capture.py <output folder> <file name without .png> [camera label] [width] [height]
"""
import sys

import unreal

folder, name = sys.argv[1], sys.argv[2]
label = sys.argv[3] if len(sys.argv) > 3 else "FaceCamera"
width = int(sys.argv[4]) if len(sys.argv) > 4 else 1600
height = int(sys.argv[5]) if len(sys.argv) > 5 else 900

# While playing or simulating, capture the game world (with what only runs there: animation, sliders); else the editor's.
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world() or editor.get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
camera = next((a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor) if a.get_actor_label() == label), None)
if camera is None:
    raise RuntimeError(f"no actor labelled {label}")

target = unreal.RenderingLibrary.create_render_target2d(world, width, height, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
# A level can hold a scene capture labelled "Capture_<camera>" (needed to capture while simulating, since actors can't
# be spawned into that world from here); otherwise a temporary one is spawned in the editor world.
placed = next((a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D) if a.get_actor_label() == f"Capture_{label}"), None)
if placed is None and world != editor.get_editor_world():
    raise RuntimeError(f"no Capture_{label} scene capture in the level to capture the simulated world with")
capture = placed or actors.spawn_actor_from_class(unreal.SceneCapture2D, camera.get_actor_location(), camera.get_actor_rotation())
try:
    component = capture.capture_component2d
    component.set_editor_property("texture_target", target)
    component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    component.set_editor_property("fov_angle", camera.camera_component.get_editor_property("field_of_view"))
    # A few captures, so temporal effects (anti-aliasing, hair) settle.
    for _ in range(8):
        component.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, target, folder, f"{name}.png")
    print(f"captured {label} -> {folder}/{name}.png")
finally:
    if placed is None:
        actors.destroy_actor(capture)
