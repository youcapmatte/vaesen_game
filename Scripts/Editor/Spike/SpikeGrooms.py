"""Phase 1 groundwork: grooms that swap on every base face, and each face's age skins.

    SpikeGrooms.py [--rebuild] [face ...] - for every face (default: all below): bind every groom of the preset table's hairstyle,
                                eyebrow and facial hair rows of the face's sex to the face's head mesh (transferred
                                from the head the plugin grooms were authored on), and write the face preset's groom
                                bindings, base age and young / old skin normals. Existing bindings are kept.

Bindings go to /Game/Spike/Appearance/GroomBindings/<face>/<groom>_Binding. Logs "[GROOMS] ..." lines.
"""
import sys

import unreal

EAL = unreal.EditorAssetLibrary
TABLE = "/Game/Spike/Appearance/DT_AppearancePresets_Spike"
BINDINGS = "/Game/Spike/Appearance/GroomBindings"
# The plugin grooms were authored on different heads (female, male, other); each groom's own plugin binding names its
# head, and the transfer must start from it, or the groom ends up offset (it did: women's hair and some brows sat low).
PLUGIN_BINDINGS = "/MetaHumanCharacter/Optional/Grooms/Bindings"
FALLBACK_HEAD = "/MetaHumanCharacter/Optional/Grooms/GroomMesh/SKM_Groom_Head_Legacy02"
# Each face's young and old skin: its own skin, or a copy of the face built with another of MetaHuman's skin textures
# (MetaHumanBaseFaces.py skin ...: texture 151 is the oldest), so the skin keeps the face's own tone and features.
# Baked head textures (MetaHuman heads share one UV layout); a skin build's two are kept in AgeSkins (SpikeCleanup.py
# moves them there and removes the rest of the build).
BUILT = "/Game/Spike/MetaHumans/Built/{}/Face/Baked/T_Head_LOD1_{}_VT"
AGE_SKIN = "/Game/Spike/Appearance/AgeSkins/T_{}_{}"
OWN = {"SpikeFace_B": "/Game/Spike/MetaHumans/SpikeFace_B/Face/Baked/T_Head_LOD1_{}_VT"}
SKINS = {  # face: (young skin build, old skin build); None: the face's own
    "SpikeFace_A": (None, "SpikeFace_A_Old"),
    "SpikeFace_B": (None, "SpikeFace_B_Old"),
    "SpikeFace_M_A": (None, "SpikeFace_M_A_Old"),
    "SpikeFace_M_B": ("SpikeFace_M_B_Young", None),
}


def skin_texture(face, build, channel):
    if build:
        return AGE_SKIN.format(build, channel)
    return OWN[face].format(channel) if face in OWN else BUILT.format(face, channel)

# Face, sex, head mesh, how old its own skin looks (0-1).
FACES = [
    ("SpikeFace_A", "female", "/Game/Spike/MetaHumans/Built/SpikeFace_A/Face/SKM_SpikeFace_A_FaceMesh", 0.25),
    ("SpikeFace_B", "female", "/Game/Spike/MetaHumans/SpikeFace_B/Face/SKM_SpikeFace_B_FaceMesh", 0.3),
    ("SpikeFace_M_A", "male", "/Game/Spike/MetaHumans/Built/SpikeFace_M_A/Face/SKM_SpikeFace_M_A_FaceMesh", 0.3),
    ("SpikeFace_M_B", "male", "/Game/Spike/MetaHumans/Built/SpikeFace_M_B/Face/SKM_SpikeFace_M_B_FaceMesh", 0.75),
]


def log(message):
    unreal.log(f"[GROOMS] {message}")
    print(f"[GROOMS] {message}")


def groom_rows(sex):
    """Every groom asset path of the table's hairstyle, eyebrow and facial hair rows for this sex."""
    table = unreal.load_asset(TABLE)
    names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    kinds = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "Kind")
    sexes = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "Sex")
    assets = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "Asset")
    paths = []
    for name, kind, row_sex, asset in zip(names, kinds, sexes, assets):
        if any(k in kind for k in ("HairStyle", "Eyebrows", "FacialHair")) and row_sex.lower() == sex and asset not in ("", "None"):
            paths.append(asset.strip('"').split("'")[-2] if "'" in asset else asset.strip('"'))
    return sorted(set(paths))


def authored_head(groom_path):
    """The head a plugin groom was authored on: its plugin binding's source mesh."""
    category = groom_path.split("/GroomAssets/")[1].split("/")[0]
    name = groom_path.split(".")[-1]
    for path in (f"{PLUGIN_BINDINGS}/{category}/{name}_Binding", f"{PLUGIN_BINDINGS}/{category}/{name}"):
        if EAL.does_asset_exist(path):
            source = unreal.load_asset(path).get_editor_property("source_skeletal_mesh")
            if source:
                return source
    log(f"{name}: no plugin binding, transferring from {FALLBACK_HEAD}")
    return unreal.load_asset(FALLBACK_HEAD)


def bind(face, mesh, groom_path, rebuild):
    groom = unreal.load_asset(groom_path)
    if groom is None:
        log(f"{face}: groom not found: {groom_path}")
        return None, None
    path = f"{BINDINGS}/{face}/{groom.get_name()}_Binding"
    if EAL.does_asset_exist(path) and not rebuild:
        return groom, unreal.load_asset(path)
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    source = authored_head(groom_path)
    binding = unreal.GroomLibrary.create_new_groom_binding_asset_with_path(path, groom, mesh, 100, source, 0)
    if binding is None:
        log(f"{face}: binding {groom.get_name()} FAILED")
        return groom, None
    EAL.save_loaded_asset(binding)
    log(f"{face}: bound {groom.get_name()} (from {source.get_name()})")
    return groom, binding


def main(args):
    rebuild = "--rebuild" in args
    only = [a for a in args if a != "--rebuild"]
    for face, sex, mesh_path, base_age in FACES:
        if only and face not in only:
            continue
        mesh = unreal.load_asset(mesh_path)
        preset = unreal.load_asset(f"/Game/Spike/Appearance/FP_{face}")
        if mesh is None or preset is None:
            log(f"{face}: head mesh or face preset missing, skipped")
            continue
        entries = []
        for groom_path in groom_rows(sex):
            groom, binding = bind(face, mesh, groom_path, rebuild)
            if groom and binding:
                entry = unreal.VaesenGroomBinding()
                entry.set_editor_property("groom", groom)
                entry.set_editor_property("binding", binding)
                entries.append(entry)
        preset.set_editor_property("groom_bindings", entries)
        preset.set_editor_property("base_age", base_age)
        young, old = SKINS[face]
        for side, build in (("young", young), ("old", old)):
            preset.set_editor_property(f"age_{side}_normal", unreal.load_asset(skin_texture(face, build, "N")))
            preset.set_editor_property(f"age_{side}_color", unreal.load_asset(skin_texture(face, build, "BC")))
        EAL.save_loaded_asset(preset)
        log(f"{face}: {len(entries)} grooms bound; base age {base_age}")


main(sys.argv[1:])
