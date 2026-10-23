from nexus_gm.prompt import NO_MYSTERY, RUNNING_A_CASE, build_table_brief, opening_instruction, speech_keyterms

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
