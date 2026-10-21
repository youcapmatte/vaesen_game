"""Phase 0 spike (P0-3): sculpt sliders for a base face (default SpikeFace_A), made from auto-rigged variants.

Run in the editor (Scripts/Editor/RunInEditor.py), one step per call, optionally for another face (--face SpikeFace_B):
    base                      - export the face's head DNA.
    variant <slider> <pos|neg> - copy the face, sculpt the slider's region one way, auto-rig it (Epic cloud, a few
                                minutes, the editor is busy meanwhile) and export its head DNA.
    apply                     - for every exported variant: add a morph target to the face's built head mesh and
                                write the joint offsets JSON (UVaesenSpikeSliderLibrary).
    preset                    - put the joint offsets into the face's UVaesenFacePresetData (FP_<face>), keeping its
                                other values (base skin tone).

Sliders are defined by face landmarks in UE space (cm): x sideways (symmetric), y forward, z up.
Logs "[P0-3] ..." lines.
"""
import glob
import json
import os
import sys

import unreal

ARGS = sys.argv[1:]
FACE = "SpikeFace_A"
if ARGS[:1] == ["--face"]:
    FACE, ARGS = ARGS[1], ARGS[2:]
# The built head mesh of each base face (face B was built into its own folder).
HEAD_MESHES = {
    "SpikeFace_A": "/Game/Spike/MetaHumans/Built/SpikeFace_A/Face/SKM_SpikeFace_A_FaceMesh",
    "SpikeFace_B": "/Game/Spike/MetaHumans/SpikeFace_B/Face/SKM_SpikeFace_B_FaceMesh",
    "SpikeFace_M_A": "/Game/Spike/MetaHumans/Built/SpikeFace_M_A/Face/SKM_SpikeFace_M_A_FaceMesh",
    "SpikeFace_M_B": "/Game/Spike/MetaHumans/Built/SpikeFace_M_B/Face/SKM_SpikeFace_M_B_FaceMesh",
}
# The face the slider regions below were drawn on. Other faces move the same landmarks (by index: the face model's
# landmarks are the same features on every face), by the reference face's moves scaled to their size. (Mapping their
# landmark positions onto the reference and selecting there picked the wrong landmarks on a face with another
# landmark layout: cheekbone height moved the ears on Victor.)
REFERENCE_FACE = "/Game/Spike/MetaHumans/SpikeFace_A"
# Morph and joint-offset weight at slider value 1, per slider (UVaesenFacePresetData::SliderStrength): sculpts of small
# regions are damped by the face model and barely read at 1.
SLIDER_STRENGTH = {"cheekboneHeight": 2.5, "noseWidth": 1.6}
BASE = f"/Game/Spike/MetaHumans/{FACE}"
VARIANTS_FOLDER = "/Game/Spike/MetaHumans/Sliders"
HEAD_MESH = HEAD_MESHES[FACE]
PRESET = f"/Game/Spike/Appearance/FP_{FACE}"
DNA_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Spike", "DNA")
# Face A's outputs predate other faces and sit at the top of DNA_DIR; other faces' go in a folder of their own.
OUT_DIR = DNA_DIR if FACE == "SpikeFace_A" else os.path.join(DNA_DIR, f"{FACE}_out")
OFFSETS_FILE = os.path.join(OUT_DIR, "slider_joint_offsets.json")

# Each slider: which landmarks it moves, and the move at full strength (pos; neg is the opposite).
SLIDERS = {
    # The whole jaw below the mouth, back to the chin: the jaw's sides and the chin's sides spread outward in
    # proportion to their distance from the middle (the chin's centre stays), so the chin widens with the jaw.
    "jawWidth": dict(select=lambda p: abs(p.x) > 1.0 and p.z < 156.5 and (p.y < 9.5 or p.z < 151.5), move=lambda p: (p.x * 0.25, 0, 0)),
    # The nose wings: outward.
    "noseWidth": dict(select=lambda p: 1.0 < abs(p.x) < 3.6 and p.y > 10.5 and 153.6 < p.z < 159.0, move=lambda p: (0.6 if p.x > 0 else -0.6, 0, 0)),
    # Age (P0-6, positive direction only): jowls and mouth corners down, cheeks hollower, brow and nose tip lower.
    "age": dict(
        select=lambda p: (abs(p.x) > 4.0 and p.z < 154.5)
        or (4.0 < abs(p.x) < 7.2 and 154.5 <= p.z < 158.0 and 3.0 < p.y < 9.5)
        or (1.4 < abs(p.x) < 3.4 and 150.5 < p.z < 153.5 and p.y > 10.0)
        or (p.z > 163.0 and p.y > 8.0)
        or (abs(p.x) < 1.0 and 155.0 < p.z < 158.5 and p.y > 12.0),
        move=lambda p: (0.0, 0.0, -0.9) if p.z < 154.5 and abs(p.x) > 4.0
        else (-0.45 if p.x > 0 else 0.45, 0.0, 0.0) if 154.5 <= p.z < 158.0 and abs(p.x) > 4.0
        else (0.0, 0.0, -0.4)),
    # The cheekbones: up.
    "cheekboneHeight": dict(select=lambda p: abs(p.x) > 5.4 and 157.5 < p.z < 162.5 and 1.5 < p.y < 10.0, move=lambda p: (0, 0, 1.0)),
}

subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(message):
    unreal.log(f"[P0-3] {message}")
    print(f"[P0-3] {message}")


class Editing:
    def __init__(self, character):
        self.character = character

    def __enter__(self):
        if not subsystem.try_add_object_to_edit(self.character):
            raise RuntimeError(f"{self.character.get_name()} is open for editing; close its tab")
        return self.character

    def __exit__(self, *exc):
        if subsystem.is_object_added_for_editing(self.character):
            subsystem.remove_object_to_edit(self.character)
        return False


def export_head_dna(character, name):
    folder = os.path.join(DNA_DIR, name)
    os.makedirs(folder, exist_ok=True)
    for old in glob.glob(os.path.join(folder, "*.dna")):
        os.remove(old)
    params = unreal.MetaHumanDNAExportParams()
    params.external_path = folder.replace("\\", "/")
    params.dna_head = True
    params.dna_body = False
    unreal.MetaHumanCharacterExportBlueprintLibrary.export_dna(character, params)
    files = glob.glob(os.path.join(folder, "*.dna"))
    if not files:
        raise RuntimeError(f"no .dna written to {folder}")
    log(f"{name}: head DNA exported ({os.path.basename(files[0])})")
    return files[0]


def base():
    character = unreal.load_asset(BASE)
    with Editing(character):
        export_head_dna(character, FACE)


def frame(landmarks):
    """The landmarks' centre and size (mean distance from the centre)."""
    n = len(landmarks)
    centre = unreal.Vector(sum(p.x for p in landmarks) / n, sum(p.y for p in landmarks) / n, sum(p.z for p in landmarks) / n)
    return centre, sum((p - centre).length() for p in landmarks) / n


def reference_landmarks():
    character = unreal.load_asset(REFERENCE_FACE)
    with Editing(character):
        return list(subsystem.get_face_landmarks(character=character))


def variant(slider, direction):
    spec = SLIDERS[slider]
    sign = 1.0 if direction == "pos" else -1.0
    name = f"{FACE}_{slider}_{direction}"
    path = f"{VARIANTS_FOLDER}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    character = unreal.EditorAssetLibrary.duplicate_asset(BASE, path)
    if character is None:
        raise RuntimeError(f"couldn't copy {BASE}")

    with Editing(character):
        subsystem.remove_face_rig(character)
        landmarks = list(subsystem.get_face_landmarks(character=character))
        # The regions are drawn on the reference face: select there, and move the same landmarks here.
        reference = landmarks if BASE == REFERENCE_FACE else reference_landmarks()
        scale = frame(landmarks)[1] / frame(reference)[1]
        indices, deltas = [], []
        for i, p in enumerate(reference):
            if i < len(landmarks) and spec["select"](p):
                dx, dy, dz = spec["move"](p)
                indices.append(i)
                deltas.append(unreal.Vector(dx * sign * scale, dy * sign * scale, dz * sign * scale))
        if not indices:
            raise RuntimeError(f"{slider}: no landmarks selected")
        subsystem.translate_face_landmarks(character=character, landmark_indices=indices, deltas=deltas)
        subsystem.commit_face_state(character=character)
        log(f"{name}: sculpted {len(indices)} landmarks {indices}")

        params = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        params.blocking = True
        params.report_progress = False
        params.rig_type = unreal.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES
        subsystem.request_auto_rigging(character, params)
        log(f"{name}: auto-rig requested (blocking) and returned")
        export_head_dna(character, name)
    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)


def apply():
    mesh = unreal.load_asset(HEAD_MESH)
    if mesh is None:
        raise RuntimeError(f"head mesh not found: {HEAD_MESH}")
    base_dna = glob.glob(os.path.join(DNA_DIR, FACE, "*.dna"))
    os.makedirs(OUT_DIR, exist_ok=True)
    if not base_dna:
        raise RuntimeError("run 'base' first")
    offsets = {}
    for slider in SLIDERS:
        for direction in (("pos",) if slider == "age" else ("pos", "neg")):
            name = f"{FACE}_{slider}_{direction}"
            files = glob.glob(os.path.join(DNA_DIR, name, "*.dna"))
            if not files:
                log(f"{name}: no DNA yet, skipped")
                continue
            # Two morphs per slider: the full shape (P0-3: shape only, no joint offsets), and the shape minus what the
            # joint offsets cover (P0-4: used together with the joint offsets).
            morph = f"slider_{slider}_{direction}"
            for suffix, compensate in (("", False), ("_j", True)):
                json_path = os.path.join(OUT_DIR, f"{morph}{suffix}.json")
                report = unreal.VaesenSpikeSliderLibrary.add_slider_morph_from_dna(mesh, base_dna[0], files[0], morph + suffix, json_path, compensate)
                for line in report.splitlines():
                    log(f"{morph}{suffix}: {line}")
                if compensate and os.path.exists(json_path):
                    with open(json_path, encoding="utf-8") as f:
                        offsets[morph] = json.load(f)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    with open(OFFSETS_FILE, "w", encoding="utf-8") as f:
        json.dump(offsets, f, indent=1)
    log(f"{FACE}: applied {len(offsets)} sliders; joint offsets in {OFFSETS_FILE}")


def preset():
    with open(OFFSETS_FILE, encoding="utf-8") as f:
        offsets = json.load(f)
    asset = unreal.load_asset(PRESET)
    if asset is None:
        raise RuntimeError(f"{PRESET} not found (SpikeSkinToneRuntime.py presets creates it)")
    positive, negative = {}, {}
    for morph, joints in offsets.items():
        _, slider, direction = morph.split("_")
        moves = [unreal.VaesenFaceJointOffset(joint=joint, offset=unreal.Vector(*xyz)) for joint, xyz in joints.items()]
        (positive if direction == "pos" else negative)[slider] = unreal.VaesenSliderJointOffsets(offsets=moves)
    asset.set_editor_property("positive_offsets", positive)
    asset.set_editor_property("negative_offsets", negative)
    asset.set_editor_property("slider_strength", SLIDER_STRENGTH)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log(f"{FACE}: {PRESET} has {len(positive)} positive and {len(negative)} negative sliders: {sorted(positive)}")


command = ARGS[0] if ARGS else ""
if command == "base":
    base()
elif command == "variant":
    variant(ARGS[1], ARGS[2])
elif command == "apply":
    apply()
elif command == "preset":
    preset()
else:
    raise RuntimeError(__doc__)
