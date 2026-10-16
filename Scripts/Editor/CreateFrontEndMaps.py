"""Creates the front-end maps and assigns their game modes. Safe to re-run: existing maps only get their game mode re-applied.

Run via Scripts/CreateFrontEndMaps.bat (editor commandlet), or in the editor: File > Execute Python Script.
"""
import unreal

MAPS = {
    "/Game/Vaesen/Maps/L_Login": "/Script/VaesenGame.VaesenLoginGameMode",
    "/Game/Vaesen/Maps/L_CharacterSelect": "/Script/VaesenGame.VaesenCharacterSelectGameMode",
}

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

for map_path, game_mode_path in MAPS.items():
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        level_editor.load_level(map_path)
        unreal.log(f"Updating existing map {map_path}")
    else:
        if not level_editor.new_level(map_path):
            raise RuntimeError(f"Could not create {map_path}")
        unreal.log(f"Created map {map_path}")

    game_mode = unreal.load_class(None, game_mode_path)
    if game_mode is None:
        raise RuntimeError(f"Game mode {game_mode_path} not found; build the editor first")

    world_settings = editor.get_editor_world().get_world_settings()
    world_settings.set_editor_property("default_game_mode", game_mode)

    if not level_editor.save_current_level():
        raise RuntimeError(f"Could not save {map_path}")
    unreal.log(f"{map_path}: GameMode override = {game_mode_path}")
