from nexus_gm.clock import SessionClock, Timeline, note_line, stamp, timed
from nexus_gm.prompt import GM_RULES


class FakeMonotonic:
    def __init__(self, start: float) -> None:
        self.value = start

    def __call__(self) -> float:
        return self.value


def test_the_clock_runs_on_from_the_api_reading_whatever_the_agent_machine_clock_says():
    # The agent's own clock reads anything at all; only how far it moves counts.
    mono = FakeMonotonic(98765.0)
    clock = SessionClock(2515.0, monotonic=mono)
    pressed = mono.value + 3.0
    mono.value += 10.0

    assert clock.now() == 2525.0
    assert clock.at(pressed) == 2518.0
    assert stamp(clock.now()) == "0:42:05"
    assert stamp(3 * 3600 + 7) == "3:00:07"
    assert timed(2525, "[Elin Gregorius, played by Ingrid] Who is there?") == "[0:42:05] [Elin Gregorius, played by Ingrid] Who is there?"


def test_a_note_is_placed_where_it_happened_with_what_was_being_said_then():
    timeline = Timeline()
    timeline.add(2400, "Elin Gregorius", "I go down to the ferry.")
    timeline.add(2410, "The Gamemaster", "The ferryman watches you come, his lantern low.", from_gm=True)
    timeline.add(2600, "The Gamemaster", "Back at the inn, the fire has burned down.", from_gm=True)

    line = note_line("Mattias (the host)", "Elin took the ferryman's key while he slept.", 2515, 2590, timeline)

    assert line.startswith("[Note from Mattias (the host), about 0:41:55, sent at 0:43:10] Elin took the ferryman's key")
    # What was going on at 0:41:55, not what the Gamemaster said later.
    assert "The ferryman watches you come" in line and "Back at the inn" not in line
    assert 'Elin Gregorius: "I go down to the ferry."' in line


def test_a_note_sent_at_once_says_only_its_time():
    assert note_line("Ingrid", "We rest for the night.", 100, 103) == "[Note from Ingrid, about 0:01:40] We rest for the night."


def test_the_gamemaster_is_told_the_clock_and_never_to_say_it():
    assert "session clock" in GM_RULES and "Never say these times aloud" in GM_RULES
