"""The session clock: one time for the whole table, counted from the start of the session.

The API keeps it (seconds since the session started, by its own clock) and hands the Gamemaster a reading when it
joins. The agent runs it on from there with a monotonic clock, so its times agree with every browser's whatever the
agent machine's clock says. Every line in the Gamemaster's conversation carries its time on this clock, and a note
from the table says when what it is about happened, with what was going on at that moment.
"""

from __future__ import annotations

import time
from collections.abc import Callable
from dataclasses import dataclass

# What a note's line repeats of what was being said at its moment: enough to place it, not the whole reply.
QUOTE_CHARS = 160


def stamp(seconds: float) -> str:
    """0:41:55: hours, minutes and seconds since the session started, as the API writes it."""
    s = max(0, int(seconds))
    return f"{s // 3600}:{s % 3600 // 60:02d}:{s % 60:02d}"


class SessionClock:
    """The session clock, run on from the API's reading with a monotonic clock."""

    def __init__(self, reading: float, monotonic: Callable[[], float] = time.monotonic) -> None:
        self._monotonic = monotonic
        self._base = reading - monotonic()

    def now(self) -> float:
        return self._base + self._monotonic()

    def at(self, monotonic_time: float) -> float:
        """The session time at an earlier moment of the monotonic clock (a talk key pressed)."""
        return self._base + monotonic_time


@dataclass(frozen=True)
class Said:
    at: float
    speaker: str
    text: str
    from_gm: bool


def _quote(text: str) -> str:
    text = " ".join(text.split())
    return text if len(text) <= QUOTE_CHARS else text[: QUOTE_CHARS - 1].rstrip() + "…"


class Timeline:
    """What was said to and by the Gamemaster, and when on the session clock: so a note can be placed in the story."""

    def __init__(self) -> None:
        self._said: list[Said] = []

    def add(self, at: float, speaker: str, text: str, from_gm: bool = False) -> None:
        self._said.append(Said(at, speaker, text, from_gm))
        self._said.sort(key=lambda s: s.at)

    def around(self, at: float) -> str:
        """What was going on at a moment: the Gamemaster's last words before it and the last thing said to it."""
        before = [s for s in self._said if s.at <= at]
        gm = next((s for s in reversed(before) if s.from_gm), None)
        player = next((s for s in reversed(before) if not s.from_gm), None)
        parts = []
        if gm:
            parts.append(f'you had said at {stamp(gm.at)}: "{_quote(gm.text)}"')
        if player:
            parts.append(f'the last thing said to you, at {stamp(player.at)}, was {player.speaker}: "{_quote(player.text)}"')
        return "; ".join(parts)


def timed(at: float, line: str) -> str:
    """A line of the conversation with its time on the session clock in front."""
    return f"[{stamp(at)}] {line}"


def note_line(name: str, text: str, about: float, now: float, timeline: Timeline | None = None) -> str:
    """
    A note from the table for the Gamemaster: when what it is about happened on the session clock, when it was sent if
    that was a while later, and what was going on at that moment, so the Gamemaster knows where in the story it belongs.
    """
    when = f"about {stamp(about)}"
    if now - about >= 10:
        when += f", sent at {stamp(now)}"
    context = timeline.around(about) if timeline else ""
    placed = f" (At {stamp(about)} {context}.)" if context else ""
    return f"[Note from {name}, {when}] {text}{placed}"
