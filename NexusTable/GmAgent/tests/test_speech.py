from nexus_gm.claude import Turn, to_messages
from nexus_gm.nexus_api import describe_roll
from nexus_gm.speech import NARRATOR, Piece, SpeakerParser, strip_tags


def parse(*chunks: str) -> list[Piece]:
    parser = SpeakerParser()
    pieces = [p for chunk in chunks for p in parser.feed(chunk)]
    return pieces + parser.finish()


def joined(pieces: list[Piece]) -> list[tuple[str | None, str]]:
    """Adjacent pieces of one speaker merged, as the voices hear them."""
    out: list[tuple[str | None, str]] = []
    for p in pieces:
        if out and out[-1][0] == p.speaker:
            out[-1] = (p.speaker, out[-1][1] + p.text)
        else:
            out.append((p.speaker, p.text))
    return out


def test_narration_and_npc_lines_get_their_speakers():
    pieces = parse('The door creaks. <npc name="Father Lind">Who sent you?</npc> He waits.')
    assert joined(pieces) == [(NARRATOR, "The door creaks. "), ("Father Lind", "Who sent you?"), (NARRATOR, " He waits.")]


def test_tags_split_across_chunks():
    pieces = parse("Rain. <np", 'c name="Ma', 'rta">Come in', "side.</n", "pc>")
    assert joined(pieces) == [(NARRATOR, "Rain. "), ("Marta", "Come inside.")]


def test_text_is_released_before_the_tag_completes():
    parser = SpeakerParser()
    assert parser.feed("The candle ") == [Piece(NARRATOR, "The candle ")]
    assert parser.feed("flickers <np") == [Piece(NARRATOR, "flickers ")]


def test_a_stray_angle_bracket_is_spoken():
    assert joined(parse("Three < four, she says.")) == [(NARRATOR, "Three < four, she says.")]


def test_strip_tags_names_the_speaker():
    assert strip_tags('Silence. <npc name="Marta">Leave.</npc>') == "Silence. Marta: Leave."


def test_messages_alternate_and_start_with_the_user():
    messages = to_messages([
        Turn("assistant", "Welcome."),
        Turn("user", "[Elin] Hello."),
        Turn("user", "[Albert] Me too."),
        Turn("assistant", "Good."),
    ])
    assert [m["role"] for m in messages] == ["user", "assistant", "user", "assistant", "user"]
    assert messages[2]["content"] == "[Elin] Hello.\n\n[Albert] Me too."


def test_describe_roll():
    roll = {"characterName": "Elin Gregorius", "skill": "learning", "poolSize": 7, "successes": 1, "pushed": True, "pushCondition": "angry"}
    assert describe_roll(roll) == "[Dice] Elin Gregorius rolled learning: 7 dice, 1 six, a success, pushed and took angry."
