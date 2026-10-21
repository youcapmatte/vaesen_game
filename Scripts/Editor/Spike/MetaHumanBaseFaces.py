"""Phase 0 spike (P0-2): create and build two base-face MetaHumans by script.

Run inside the editor (Python), or as a commandlet:
    UnrealEditor-Cmd.exe VaesenGame.uproject -run=pythonscript -script="<this file> <stage>"

Stages (optionally followed by face names, e.g. "rig SpikeFace_M_A"; default: every face below):
    author  - create the characters, sculpt, skin and groom them (all local, no Epic login). Faces with a template
              start as a copy of that MetaHuman preset (its own body, skin and grooms).
    rig      - auto-rig the faces, then download their textures (Epic cloud service: needs an Epic login in the editor).
    textures - only download the high-resolution textures (for faces already rigged in the editor).
    build    - build them with the Optimized pipeline (needs the rig and the textures).
    all     - author, rig, build.
    nofacialhair <face> ... - remove the beard and moustache (a preset's come baked into the face's skin textures
              when built with them; the creator puts facial hair on as grooms). Build again afterwards.
    skin <face> <texture index> <suffix> - a copy of a built face with another of MetaHuman's skin textures (e.g. an
              old one), its textures downloaded and built to <face>_<suffix>: the age layer's young or old skin. Then
              SpikeCleanup.py --delete keeps its two head textures (in /Game/Spike/Appearance/AgeSkins) and removes the rest.

Every step logs "[P0-2] <step>: OK" or "[P0-2] <step>: FAILED <reason>", so the log reads as a pass/fail list.
"""
import random
import sys

import unreal

FOLDER = "/Game/Spike/MetaHumans"
BUILD_FOLDER = "/Game/Spike/MetaHumans/Built"
GROOM = "/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_S_UpdoBuns.WI_Hair_S_UpdoBuns"

# Name, random seed for the face sculpt (None = leave the face as it is), skin tone (u, v; None = keep), template
# (a MetaHuman preset to start from; None = the default character with the updo groom).
FACES = [
    ("SpikeFace_A", None, (0.25, 0.55), None),
    ("SpikeFace_B", 7, (0.55, 0.45), None),
    ("SpikeFace_M_A", None, None, "/MetaHumanCharacter/Optional/Presets/Victor"),
    ("SpikeFace_M_B", None, None, "/MetaHumanCharacter/Optional/Presets/Lorenzo"),
]

subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(step, ok, reason=""):
    if ok:
        unreal.log(f"[P0-2] {step}: OK")
    else:
        unreal.log_error(f"[P0-2] {step}: FAILED {reason}")


class Editing:
    """Opens a character for editing and always closes it again."""

    def __init__(self, character):
        self.character = character

    def __enter__(self):
        if not subsystem.try_add_object_to_edit(self.character):
            raise RuntimeError("unable to edit the asset (already open?)")
        return self.character

    def __exit__(self, *exc):
        if subsystem.is_object_added_for_editing(self.character):
            subsystem.remove_object_to_edit(self.character)
        return False


def create(name, template=None):
    """A fresh character, or a copy of a preset; an earlier spike run's asset of that name is replaced."""
    path = f"{FOLDER}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    if template:
        return unreal.EditorAssetLibrary.duplicate_asset(template, path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name=name,
        package_path=FOLDER,
        asset_class=unreal.MetaHumanCharacter,
        factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew),
    )


def sculpt(character, seed):
    """Reshapes the face through its landmarks: a random, mild change of proportions around the face's centre.

    Not through the face model coefficients: that array also holds the model's layout (values up to ~155), and
    changing it blindly corrupts the state. Returns how far the landmarks moved on average (cm).
    """
    rng = random.Random(seed)
    landmarks = list(subsystem.get_face_landmarks(character=character))
    if not landmarks:
        raise RuntimeError("no face landmarks")
    centre = unreal.Vector(
        sum(v.x for v in landmarks) / len(landmarks),
        sum(v.y for v in landmarks) / len(landmarks),
        sum(v.z for v in landmarks) / len(landmarks),
    )
    scale = unreal.Vector(rng.uniform(0.94, 1.06), rng.uniform(0.94, 1.06), rng.uniform(0.95, 1.05))
    deltas = [
        unreal.Vector((v.x - centre.x) * (scale.x - 1), (v.y - centre.y) * (scale.y - 1), (v.z - centre.z) * (scale.z - 1))
        for v in landmarks
    ]
    subsystem.translate_face_landmarks(character=character, landmark_indices=list(range(len(deltas))), deltas=deltas)
    subsystem.commit_face_state(character=character)

    moved = list(subsystem.get_face_landmarks(character=character))
    return sum((a - b).length() for a, b in zip(moved, landmarks)) / len(landmarks), scale


def skin(character, uv):
    properties = unreal.MetaHumanCharacterSkinProperties()
    properties.u, properties.v = uv
    settings = unreal.MetaHumanCharacterSkinSettings()
    settings.skin = properties
    settings.enable_texture_overrides = False
    subsystem.commit_skin_settings(character, settings)


def groom(character):
    item = unreal.load_asset(GROOM)
    if item is None:
        raise RuntimeError(f"groom not found: {GROOM}")
    key = character.internal_collection.try_add_item_from_wardrobe_item("Hair", item)
    selection = unreal.MetaHumanPipelineSlotSelection(slot_name="Hair", selected_item=key)
    character.internal_collection.default_instance.try_add_slot_selection(selection)


def author(name, seed, uv, template):
    character = create(name, template)
    log(f"{name} create" + (f" from {template}" if template else ""), character is not None)
    if character is None:
        return
    if not template:
        groom(character)
    with Editing(character):
        if seed is not None:
            moved, scale = sculpt(character, seed)
            log(f"{name} sculpt (scale {scale.x:.3f}/{scale.y:.3f}/{scale.z:.3f}, landmarks moved {moved:.3f} cm on average)",
                moved > 0.01, "the landmarks didn't move")
        if uv is not None:
            skin(character, uv)
            log(f"{name} skin (u={uv[0]}, v={uv[1]})", True)
        subsystem.assemble_for_preview(character=character)
        log(f"{name} groom + preview assembly", True)
    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)


def rig(name):
    character = unreal.load_asset(f"{FOLDER}/{name}.{name}")
    with Editing(character):
        params = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        params.blocking = True
        params.report_progress = False
        params.rig_type = unreal.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES
        subsystem.request_auto_rigging(character, params)
    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)
    textures(name)


def textures(name):
    character = unreal.load_asset(f"{FOLDER}/{name}.{name}")
    with Editing(character):
        # Building also needs the high-resolution texture sources (with the animated wrinkle maps); same cloud service.
        request = unreal.MetaHumanCharacterTextureRequestParams()
        request.blocking = True
        request.report_progress = False
        subsystem.request_texture_sources(character, request)
        has_textures = character.get_editor_property("has_high_resolution_textures")
        log(f"{name} high-res textures", has_textures, "none afterwards (not logged in to Epic?)")

        # Rigged and textured: buildable. Otherwise the subsystem logs why not.
        log(f"{name} rigged + textures -> buildable", subsystem.can_build_meta_human(character, True),
            "see the 'Failed to build MetaHuman Character' line above")
    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)


def build(name):
    character = unreal.load_asset(f"{FOLDER}/{name}.{name}")
    with Editing(character):
        if not subsystem.can_build_meta_human(character, True):
            log(f"{name} build", False, "not buildable (no face rig?)")
            return
        params = unreal.MetaHumanCharacterEditorBuildParameters()
        params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        # High keeps four face LODs: LOD0 for the creator, the rest for gameplay (spec section 6.2).
        params.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        params.absolute_build_path = BUILD_FOLDER
        params.common_folder_path = f"{BUILD_FOLDER}/Common"
        subsystem.build_meta_human(character=character, params=params)
    saved = unreal.EditorAssetLibrary.save_directory(BUILD_FOLDER, only_if_is_dirty=False, recursive=True)
    built = unreal.EditorAssetLibrary.list_assets(f"{BUILD_FOLDER}/{name}", recursive=True)
    log(f"{name} build (Optimized, High): {len(built)} assets", saved and len(built) > 0, "nothing built or saved")


def no_facial_hair(name):
    character = unreal.load_asset(f"{FOLDER}/{name}")
    for slot in ("Beard", "Mustache"):
        character.internal_collection.default_instance.set_single_slot_selection(slot, unreal.MetaHumanPaletteItemKey())
    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)
    log(f"{name} beard and moustache removed", True)


def skin_variant(name, index, suffix):
    variant = f"{name}_{suffix}"
    path = f"{FOLDER}/{variant}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    character = unreal.EditorAssetLibrary.duplicate_asset(f"{FOLDER}/{name}", path)
    with Editing(character):
        settings = character.get_editor_property("skin_settings")
        skin_props = settings.get_editor_property("skin")
        skin_props.set_editor_property("face_texture_index", index)
        settings.set_editor_property("skin", skin_props)
        subsystem.commit_skin_settings(character, settings)
        log(f"{variant} face texture {index}", True)
    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)
    textures(variant)
    build(variant)


def run(step, *args):
    try:
        step(*args)
    except Exception as error:  # Report and carry on with the next face.
        log(f"{args[0]} {step.__name__}", False, repr(error))


def main(stage, only):
    for name, seed, uv, template in FACES:
        if only and name not in only:
            continue
        if stage in ("author", "all"):
            run(author, name, seed, uv, template)
        if stage in ("rig", "all"):
            run(rig, name)  # Also downloads the textures.
        if stage == "textures":
            run(textures, name)
        if stage in ("build", "all"):
            run(build, name)


if len(sys.argv) > 1 and sys.argv[1] == "nofacialhair":
    for face_name in sys.argv[2:]:
        run(no_facial_hair, face_name)
elif len(sys.argv) > 1 and sys.argv[1] == "skin":
    run(skin_variant, sys.argv[2], int(sys.argv[3]), sys.argv[4])
else:
    main(sys.argv[1] if len(sys.argv) > 1 else "author", sys.argv[2:])
