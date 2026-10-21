"""Sets up /Game/Vaesen/Maps/L_CharacterSelect's 3D scene: the character creator scene (it previews the selected
character and hosts the creator) in a simple dark studio. Replaces what an earlier run placed (actors tagged
VaesenSelectScene); anything else in the level stays.

    RunInEditor.py Scripts/Editor/SetupCharacterSelectLevel.py
"""
import unreal

LEVEL = "/Game/Vaesen/Maps/L_CharacterSelect"
TAG = "VaesenSelectScene"
PRESETS = "/Game/Spike/Appearance/DT_AppearancePresets_Spike"
SKIN_TONES = "/Game/Spike/Appearance/ST_SkinTones"

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level(LEVEL)
for actor in actors.get_all_level_actors():
    if TAG in [str(t) for t in actor.tags]:
        actors.destroy_actor(actor)


def spawn(cls, location, rotation=unreal.Rotator(), label=None):
    actor = actors.spawn_actor_from_class(cls, location, rotation)
    actor.set_actor_label(label)
    actor.tags = [TAG]
    return actor


# A dark studio: a floor, a warm key light from the front left, a cool rim light from behind, a dim sky light.
floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), label="Studio_Floor")
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
floor.static_mesh_component.set_material(0, unreal.load_asset("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"))
floor.set_actor_scale3d(unreal.Vector(30, 30, 1))
key = spawn(unreal.RectLight, unreal.Vector(-250, 180, 260), unreal.Rotator(roll=0, pitch=-30, yaw=-35), "Studio_KeyLight")
key.rect_light_component.set_editor_property("intensity", 40.0)
key.rect_light_component.set_editor_property("light_color", unreal.Color(255, 228, 196, 255))
key.rect_light_component.set_editor_property("source_width", 120.0)
key.rect_light_component.set_editor_property("source_height", 120.0)
key.rect_light_component.set_editor_property("attenuation_radius", 1500.0)
fill = spawn(unreal.RectLight, unreal.Vector(-250, -220, 180), unreal.Rotator(roll=0, pitch=-15, yaw=40), "Studio_FillLight")
fill.rect_light_component.set_editor_property("intensity", 8.0)
fill.rect_light_component.set_editor_property("attenuation_radius", 1500.0)
rim = spawn(unreal.RectLight, unreal.Vector(220, -120, 240), unreal.Rotator(roll=0, pitch=-25, yaw=150), "Studio_RimLight")
rim.rect_light_component.set_editor_property("intensity", 30.0)
rim.rect_light_component.set_editor_property("light_color", unreal.Color(190, 210, 255, 255))
rim.rect_light_component.set_editor_property("attenuation_radius", 1500.0)
sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 400), label="Studio_SkyLight")
sky.light_component.set_editor_property("intensity", 0.3)
sky.light_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property("cubemap", unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"))
fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), label="Studio_Fog")
fog.component.set_editor_property("fog_density", 0.06)
fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.01, 0.01, 0.012, 1.0))
post = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), label="Studio_PostProcess")
post.set_editor_property("unbound", True)
settings = post.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_method", True)
settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", 10.0)
post.set_editor_property("settings", settings)

scene = spawn(unreal.VaesenCharacterCreatorScene, unreal.Vector(0, 0, 0), label="CharacterScene")
scene.set_editor_property("presets", unreal.load_asset(PRESETS))
scene.set_editor_property("skin_tone_table", unreal.load_asset(SKIN_TONES))

levels.save_current_level()
print(f"[SELECT] {LEVEL}: character scene and studio placed")
