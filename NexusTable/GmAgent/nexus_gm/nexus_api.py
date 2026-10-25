"""The Nexus Table API, as the Gamemaster agent uses it (authorized by X-Agent-Key)."""

from __future__ import annotations

import os
from typing import Any

import aiohttp

API_URL = os.getenv("NEXUS_API_URL", "http://localhost:5240").rstrip("/")
AGENT_KEY = os.getenv("NEXUS_AGENT_KEY", "")


class NexusApiError(RuntimeError):
    pass


async def _call(method: str, path: str, payload: Any = None) -> Any:
    timeout = aiohttp.ClientTimeout(total=10)
    async with aiohttp.ClientSession(timeout=timeout) as http:
        async with http.request(method, f"{API_URL}{path}", headers={"X-Agent-Key": AGENT_KEY}, json=payload) as response:
            body = await response.json(content_type=None)
            if response.status != 200 or not body.get("success"):
                raise NexusApiError(f"{method} {path} failed ({response.status}): {body.get('errorMessage')}")
            return body["result"]


async def _get(path: str) -> Any:
    return await _call("GET", path)


async def get_table() -> dict[str, Any]:
    """The Society and every player's full sheet, dark secrets included."""
    return await _get("/api/agent/table")


async def get_roll(roll_id: str) -> dict[str, Any]:
    """A roll as the server threw it."""
    return await _get(f"/api/agent/rolls/{roll_id}")


async def log_events(events: list[dict[str, Any]]) -> None:
    """Lines for the open session's log: what was said to and by the Gamemaster, notes it was dictated."""
    await _call("POST", "/api/agent/events", {"events": events})


async def get_event(event_id: str) -> dict[str, Any]:
    """A logged event (a note typed at the table), as the server has it."""
    return await _get(f"/api/agent/events/{event_id}")


async def put_state(session_id: str, state: dict[str, Any]) -> None:
    """The scribe's play state for the session."""
    await _call("PUT", f"/api/agent/sessions/{session_id}/state", state)


async def request_test(test: dict[str, str]) -> dict[str, Any]:
    """Puts a test the Gamemaster asked for on the table, from a <test .../> tag's attributes."""
    def number(key: str, default: int) -> int:
        try:
            return int(str(test.get(key, default)).replace("+", "").strip())
        except ValueError:
            return default

    return await _call("POST", "/api/agent/tests", {
        "character": test.get("who", ""),
        "skill": test.get("skill", ""),
        "difficulty": number("difficulty", 0),
        "successesNeeded": number("sixes", 1),
        "reason": test.get("why") or None,
    })


def describe_roll(roll: dict[str, Any]) -> str:
    """A [Dice] line for the Gamemaster: "[Dice] Elin Gregorius rolled learning: 7 dice, 1 of 2 sixes needed, a failure"."""
    what = roll.get("skill") or f"{roll.get('attribute')} (attribute test)"
    successes = roll.get("successes", 0)
    needed = max(1, roll.get("successesNeeded") or 1)
    sixes = f"{successes} six{'es' if successes != 1 else ''}" if needed == 1 else f"{successes} of {needed} sixes needed"
    outcome = f"{sixes}, a success" if successes >= needed else f"{sixes}, a failure"
    pushed = f", pushed and took {roll['pushCondition']}" if roll.get("pushed") and roll.get("pushCondition") else ", pushed" if roll.get("pushed") else ""
    reason = f" ({roll['reason']})" if roll.get("reason") else ""
    return f"[Dice] {roll.get('characterName')} rolled {what}{reason}: {roll.get('poolSize')} dice, {outcome}{pushed}."
