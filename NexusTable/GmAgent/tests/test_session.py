from nexus_gm.claude import thinking_off
from nexus_gm.nexus_api import describe_roll
from nexus_gm.scribe import STATE_KEYS, STATE_SCHEMA, merge_found
from nexus_gm.speech import SpeakerParser, find_tests, strip_tags

REPLY = (
    'The ledger is old and the ink has run. Elin, make a Learning test, minus one; you need two sixes. '
    '<test who="Elin Gregorius" skill="Learning" difficulty="-1" sixes="2" why="the water-stained ledger"/>'
)


def test_a_test_tag_is_never_spoken_but_is_found_in_the_reply():
    parser = SpeakerParser()
    spoken = "".join(p.text for chunk in [REPLY[:90], REPLY[90:130], REPLY[130:]] for p in parser.feed(chunk))
    spoken += "".join(p.text for p in parser.finish())

    assert "<test" not in spoken and "sixes=" not in spoken
    assert spoken.startswith("The ledger is old")
    assert "<test" not in strip_tags(REPLY)
    assert find_tests(REPLY) == [{"who": "Elin Gregorius", "skill": "Learning", "difficulty": "-1", "sixes": "2", "why": "the water-stained ledger"}]


def test_a_roll_tells_the_gamemaster_the_sixes_needed():
    roll = {"characterName": "Elin Gregorius", "skill": "learning", "poolSize": 7, "successes": 1, "successesNeeded": 2}

    assert describe_roll(roll) == "[Dice] Elin Gregorius rolled learning: 7 dice, 1 of 2 sixes needed, a failure."
    assert describe_roll({**roll, "successesNeeded": 1}) == "[Dice] Elin Gregorius rolled learning: 7 dice, 1 six, a success."


def test_the_scribe_never_loses_a_found_fact_and_answers_every_field():
    old = {"factsFound": [{"factId": "f1", "how": "told", "session": 1}]}
    new = {"summary": "", "factsFound": [{"factId": "f2", "how": "found", "session": 2}]}

    assert [f["factId"] for f in merge_found(old, new)["factsFound"]] == ["f1", "f2"]
    assert STATE_SCHEMA["required"] == STATE_KEYS and set(STATE_KEYS) == set(STATE_SCHEMA["properties"])


def test_thinking_is_turned_off_the_way_each_model_takes_it():
    assert thinking_off("claude-sonnet-5-5") == {"type": "between_tools"}
    assert thinking_off("claude-haiku-5-5") == {"type": "disabled"}
    assert thinking_off("claude-opus-5-5") is None


def test_the_gamemaster_is_taught_to_move_the_story_as_at_a_table():
    from nexus_gm.prompt import MOVE_ON, NUDGE, RUNNING_A_CASE

    for rule in ("Frame scenes", "Share the spotlight", "Never let a missed clue stop the story", "When they stall, the world moves"):
        assert rule in RUNNING_A_CASE
    assert "another of its channels" in MOVE_ON
    assert NUDGE.startswith("[Table]")


def test_the_scribes_notes_reach_the_gamemaster_as_a_line_it_never_reads_out():
    from nexus_gm.prompt import RUNNING_A_CASE, scribe_line

    line = scribe_line({"scene": "the mill, night", "factsFound": [{"factId": "f1"}, {"factId": "f4"}], "beatsFired": ["e2"], "openThreads": ["the locked shed"]})

    assert line == "[Scribe] Now: the mill, night. Facts found: f1, f4. Beats and events so far: e2. Deeds done: none. Open threads: the locked shed."
    assert "[Scribe] are your own notes" in RUNNING_A_CASE
