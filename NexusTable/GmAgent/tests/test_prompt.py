from nexus_gm.prompt import GM_RULES, NO_MYSTERY, RUNNING_A_CASE, build_session_block, build_table_brief, opening_instruction, speech_keyterms

TABLE = {
    "society": {"name": "The Society", "headquarters": "A castle outside Upsala"},
    "players": [{"displayName": "Ingrid", "character": {"name": "Elin Gregorius", "archetype": "academic", "age": 34}}],
}

CASE = {
    "mysteryId": "mystery-1",
    "title": "The Drowned Bells",
    "hookText": "Come at once: the bells of Forsby ring under the lake.",
    "caseFile": "# The Drowned Bells\n\nThe miller drowned them.",
    "castNames": ["Anders Hallgren", "Elin Gregorius"],
}


def test_without_a_case_the_gamemaster_improvises_and_welcomes_the_table():
    brief = build_table_brief(TABLE)

    assert NO_MYSTERY in brief
    assert RUNNING_A_CASE not in brief
    assert "Welcome the Society" in opening_instruction(TABLE)


def test_with_a_case_the_gamemaster_gets_the_hook_and_the_case_file_and_opens_with_the_hook():
    table = {**TABLE, "mystery": CASE}
    brief = build_table_brief(table)

    assert RUNNING_A_CASE in brief
    assert NO_MYSTERY not in brief
    assert "the bells of Forsby ring under the lake" in brief
    assert "The miller drowned them." in brief
    assert "Present the hook" in opening_instruction(table)


def test_speech_keyterms_are_the_characters_and_the_cast_once_each():
    assert speech_keyterms(TABLE) == ["Elin Gregorius"]
    assert speech_keyterms({**TABLE, "mystery": CASE}) == ["Elin Gregorius", "Anders Hallgren"]


SESSION_TWO = {
    "id": "playsession-1",
    "number": 2,
    "state": {
        "summary": "The Society heard of the bells from the ferryman.",
        "scene": "The ferry landing, the first evening",
        "factsFound": [{"factId": "f1", "how": "the ferryman told Elin", "session": 1}],
        "peopleMet": [{"characterId": "npc-ferryman", "name": "Olof", "note": "wary, helpful"}],
        "openThreads": ["the miller's locked shed"],
    },
}


def test_a_later_session_opens_with_a_recap_from_where_the_mystery_stands():
    table = {**TABLE, "mystery": CASE, "session": SESSION_TWO, "recentLog": ["19:00 *Session 2 of \"The Drowned Bells\" begins.*"]}
    block = build_session_block(table)

    assert "Session 2" in opening_instruction(table) and "recap" in opening_instruction(table)
    assert "The Society heard of the bells from the ferryman." in block
    assert "f1 (the ferryman told Elin)" in block
    assert "the miller's locked shed" in block


def test_rejoining_mid_session_picks_up_rather_than_starting_over():
    table = {**TABLE, "mystery": CASE, "session": SESSION_TWO, "recentLog": ["19:05 **The Gamemaster:** The water is black."]}

    assert "rejoins the table in the middle of the session" in opening_instruction(table)


def test_the_first_session_opens_with_the_hook_and_the_rules_are_for_a_table():
    table = {**TABLE, "mystery": CASE, "session": {"id": "playsession-1", "number": 1, "state": {}}, "recentLog": []}

    assert "Present the hook" in opening_instruction(table)
    assert "nothing is shown" in RUNNING_A_CASE
    assert "trust>=N" in RUNNING_A_CASE
    assert "<test who=" in GM_RULES
