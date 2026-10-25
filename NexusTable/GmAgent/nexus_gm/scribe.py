"""The scribe: keeps where the mystery stands, in the background, so the next session can resume from it.

After each exchange it reads what has happened since its last look, with the case file and the play state as it was,
and writes the new play state: the story so far, the scene, the facts found and how, whom they met, which beats have
happened, what is still open. Its own call on a stronger model than the dialogue's, off the critical path: the table
never waits for it.
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
from collections.abc import Awaitable, Callable
from typing import Any

import anthropic

logger = logging.getLogger("nexus-gm.scribe")

SCRIBE_MODEL = os.getenv("NEXUS_SCRIBE_MODEL", "claude-sonnet-5-5")
MAX_TOKENS = 6000

INSTRUCTION = """\
You keep the Gamemaster's notes for a Vaesen mystery played by voice over several sessions. Below are the case file \
(the hidden truth, its facts with their ids, the channels that reveal them, the cast, the beats and the levers), the \
play state as it stood, and what has happened at the table since. Write the new play state.
- Keep the case file's ids: facts f1, channels c7, events e3, beats b2, deeds r1-d1, people by their id.
- A fact is found only when the investigators perceived a channel's reveal, or were told it in play. Not because \
the Gamemaster knows it, and not because they guessed it. Say how, in a few words, and in which session.
- summary: the story so far as the investigators lived it, in the past tense, naming people and places, with what \
they suspect as their suspicion. 120 to 300 words. It is what a recap at the next session is made from.
- scene: where they are and when, in the world ("the Forsby inn, the second evening").
- peopleMet: each person they have dealt with, and how it went and stands.
- beatsFired: escalation events and beats that have happened in play. deedsDone: lever deeds the investigators did.
- openThreads: leads not followed, promises made, things left hanging, short.
- Notes from the table ([Note from ...]) are true: take them in.
- Keep everything of the old state that is still true. Never drop a found fact.
"""

FOUND_FACT = {
    "type": "object",
    "properties": {"factId": {"type": "string"}, "how": {"type": "string"}, "session": {"type": "integer"}},
    "required": ["factId", "how", "session"],
    "additionalProperties": False,
}
MET_PERSON = {
    "type": "object",
    "properties": {"characterId": {"type": "string"}, "name": {"type": "string"}, "note": {"type": "string"}},
    "required": ["characterId", "name", "note"],
    "additionalProperties": False,
}
STRINGS = {"type": "array", "items": {"type": "string"}}
STATE_SCHEMA = {
    "type": "object",
    "properties": {
        "summary": {"type": "string"},
        "scene": {"type": "string"},
        "factsFound": {"type": "array", "items": FOUND_FACT},
        "channelsUsed": STRINGS,
        "peopleMet": {"type": "array", "items": MET_PERSON},
        "beatsFired": STRINGS,
        "deedsDone": STRINGS,
        "openThreads": STRINGS,
    },
    "required": ["summary", "scene", "factsFound", "channelsUsed", "peopleMet", "beatsFired", "deedsDone", "openThreads"],
    "additionalProperties": False,
}
STATE_KEYS = STATE_SCHEMA["required"]


def merge_found(old: dict[str, Any], new: dict[str, Any]) -> dict[str, Any]:
    """The new state, with every fact the old one had found kept: a found fact is never lost to a careless rewrite."""
    found = {f["factId"]: f for f in old.get("factsFound") or []}
    for fact in new.get("factsFound") or []:
        found.setdefault(fact["factId"], fact)
    return {**new, "factsFound": list(found.values())}


class Scribe:
    """Updates the play state after the table moves on; one update at a time, the latest lines always included."""

    def __init__(self, case_file: str, session: dict[str, Any], save: Callable[[str, dict[str, Any]], Awaitable[None]]) -> None:
        self._client = anthropic.AsyncAnthropic()
        self._system = [
            {"type": "text", "text": INSTRUCTION},
            {"type": "text", "text": f"The case file:\n{case_file}", "cache_control": {"type": "ephemeral"}},
        ]
        self._session_id: str = session["id"]
        self._number: int = session.get("number", 1)
        self._state: dict[str, Any] = {k: (session.get("state") or {}).get(k, [] if k not in ("summary", "scene") else "") for k in STATE_KEYS}
        self._lines: list[str] = []
        self._save = save
        self._running: asyncio.Task | None = None
        self._again = False

    def add(self, line: str) -> None:
        self._lines.append(line)

    def poke(self) -> None:
        """Update soon: now if idle, else once the running update is done."""
        if self._running and not self._running.done():
            self._again = True
            return
        self._running = asyncio.create_task(self._loop())

    async def flush(self) -> None:
        """Wait until everything logged so far is in the state (the session ending, the Gamemaster leaving)."""
        if self._running and not self._running.done():
            self._again = True
            await self._running
        elif self._lines:
            await self._loop()

    async def _loop(self) -> None:
        while True:
            self._again = False
            await self._update()
            if not self._again:
                return

    async def _update(self) -> None:
        if not self._lines:
            return
        lines, self._lines = self._lines, []
        try:
            response = await self._client.messages.create(
                model=SCRIBE_MODEL,
                max_tokens=MAX_TOKENS,
                system=self._system,
                messages=[{
                    "role": "user",
                    "content": (
                        f"Session {self._number}.\n\n# The play state as it stood\n{json.dumps(self._state, ensure_ascii=False)}\n\n"
                        "# What has happened since\n" + "\n".join(lines)
                    ),
                }],
                output_config={"effort": "low", "format": {"type": "json_schema", "schema": STATE_SCHEMA}},
            )
            if response.stop_reason in ("refusal", "max_tokens"):
                raise ValueError(f"the scribe stopped: {response.stop_reason}")
            text = "".join(block.text for block in response.content if block.type == "text")
            state = merge_found(self._state, json.loads(text))
        except (anthropic.APIError, ValueError) as error:
            # The lines go back in, so the next update still sees them.
            logger.warning("the scribe could not update the play state: %s", error)
            self._lines = lines + self._lines
            return

        self._state = state
        try:
            await self._save(self._session_id, state)
        except Exception as error:  # noqa: BLE001 - the table goes on; the next update saves again
            logger.warning("the play state was not saved: %s", error)
