"""Removes what the character creator spike left behind but the game doesn't use, keeping what it does.

    SpikeCleanup.py [--delete]   - without --delete, only reports

1. The age skins: the two textures the face presets use from each skin variant build (SpikeFace_*_Old / _Young) move
   to /Game/Spike/Appearance/AgeSkins (their referencers follow); the rest of those builds and their characters go
   (MetaHumanBaseFaces.py skin ... rebuilds them).
2. Test-only content goes: the slider variant characters (SpikeSliders.py variant ... rebuilds them; the DNA they made
   stays in Saved/Spike/DNA), the heavy body test, the body split test, the offline skin tone test, the retargeted test
   animations, and the Phase 0 test maps (L_SpikeCreator stays).
An asset is only deleted when nothing that stays references it; anything else is reported and kept.
"""
import sys

import unreal

EAL = unreal.EditorAssetLibrary
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
DELETE = "--delete" in sys.argv
ROOT = "/Game/Spike"
AGE_SKINS = f"{ROOT}/Appearance/AgeSkins"
# Skin variant builds: any SpikeFace_*_Old / _Young folder in Built.
VARIANTS = [d.split("/")[-1] for d in sorted({"/".join(a.split("/")[:6]) for a in EAL.list_assets(f"{ROOT}/MetaHumans/Built", recursive=True)})
            if d.split("/")[-1].endswith(("_Old", "_Young"))] or ["SpikeFace_A_Old", "SpikeFace_B_Old", "SpikeFace_M_A_Old", "SpikeFace_M_B_Young"]
KEEP_MAPS = {"L_SpikeCreator"}
KEEP_IN_RETARGET = {"RTG_Mannequin_to_MetaHuman"}


def package(path):
    return path.split(".")[0]


def assets_in(folder):
    return [package(a) for a in EAL.list_assets(folder, recursive=True, include_folder=False)] if EAL.does_directory_exist(folder) else []


# 1. Move the age skins out of the variant builds.
for variant in VARIANTS:
    for channel in ("BC", "N"):
        source = f"{ROOT}/MetaHumans/Built/{variant}/Face/Baked/T_Head_LOD1_{channel}_VT"
        target = f"{AGE_SKINS}/T_{variant}_{channel}"
        if EAL.does_asset_exist(target):
            continue
        if not EAL.does_asset_exist(source):
            print(f"[CLEANUP] missing {source}")
            continue
        print(f"[CLEANUP] move {source} -> {target}")
        if DELETE:
            EAL.rename_asset(source, target)

# 2. What goes.
doomed = set()
doomed.update(assets_in(f"{ROOT}/MetaHumans/Sliders"))
doomed.update(assets_in(f"{ROOT}/MetaHumans/Built/SpikeFace_A_Heavy"))
doomed.add(f"{ROOT}/MetaHumans/SpikeFace_A_Heavy")
for variant in VARIANTS:
    doomed.update(assets_in(f"{ROOT}/MetaHumans/Built/{variant}"))
    doomed.add(f"{ROOT}/MetaHumans/{variant}")
doomed.update(assets_in(f"{ROOT}/BodyParts"))
doomed.update(assets_in(f"{ROOT}/SkinTone"))
doomed.update(a for a in assets_in(f"{ROOT}/Retarget") if a.split("/")[-1] not in KEEP_IN_RETARGET)
doomed.update(a for a in assets_in(f"{ROOT}/Maps") if a.split("/")[-1] not in KEEP_MAPS)
doomed.add(f"{ROOT}/Appearance/Temp_ToneProbe")
doomed = {a for a in doomed if EAL.does_asset_exist(a) and not a.startswith(AGE_SKINS)}

# Keep anything something that stays still uses (transitively: a kept asset keeps what it references).
kept_back = set()
changed = True
while changed:
    changed = False
    for asset in sorted(doomed - kept_back):
        options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
        users = [str(r) for r in (REGISTRY.get_referencers(asset, options) or [])]
        outside = [u for u in users if u not in doomed or u in kept_back]
        if outside:
            kept_back.add(asset)
            changed = True
            print(f"[CLEANUP] keep {asset}: used by {outside[:3]}")

gone = sorted(doomed - kept_back)
print(f"[CLEANUP] {len(gone)} assets to delete, {len(kept_back)} kept because they're used")
if DELETE:
    for asset in gone:
        if EAL.does_asset_exist(asset):
            EAL.delete_asset(asset)
    for folder in [f"{ROOT}/MetaHumans/Sliders", f"{ROOT}/MetaHumans/Built/SpikeFace_A_Heavy", f"{ROOT}/BodyParts", f"{ROOT}/SkinTone"] + [f"{ROOT}/MetaHumans/Built/{v}" for v in VARIANTS]:
        if EAL.does_directory_exist(folder) and not EAL.list_assets(folder, recursive=True):
            EAL.delete_directory(folder)
    # Leave no redirectors behind from the moves.
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    redirectors = [unreal.load_asset(a) for a in EAL.list_assets(ROOT, recursive=True) if REGISTRY.get_asset_by_object_path(a).asset_class_path.asset_name == "ObjectRedirector"]
    if redirectors:
        tools.fixup_referencers([r for r in redirectors if r])
    print(f"[CLEANUP] deleted {len(gone)} assets, fixed {len(redirectors)} redirectors")
