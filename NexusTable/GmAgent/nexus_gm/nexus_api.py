"""The Nexus Table API, as the Gamemaster agent uses it (authorized by X-Agent-Key)."""

from __future__ import annotations

import os
from typing import Any

import aiohttp

API_URL = os.getenv("NEXUS_API_URL", "http://localhost:5240").rstrip("/")
AGENT_KEY = os.getenv("NEXUS_AGENT_KEY", "")


class NexusApiError(RuntimeError):
    pass


async def _get(path: str) -> Any:
    timeout = aiohttp.ClientTimeout(total=10)
    async with aiohttp.ClientSession(timeout=timeout) as http:
        async with http.get(f"{API_URL}{path}", headers={"X-Agent-Key": AGENT_KEY}) as response:
            body = await response.json(content_type=None)
            if response.status != 200 or not body.get("success"):
                raise NexusApiError(f"GET {path} failed ({response.status}): {body.get('errorMessage')}")
            return body["result"]


async def get_table() -> dict[str, Any]:
    """The Society and every player's full sheet, dark secrets included."""
    return await _get("/api/agent/table")


async def get_roll(roll_id: str) -> dict[str, Any]:
    """A roll as the server threw it."""
    return await _get(f"/api/agent/rolls/{roll_id}")


def describe_roll(roll: dict[str, Any]) -> str:
    """A [Dice] line for the Gamemaster, e.g. "[Dice] Elin Gregorius rolled learning: 7 dice, 2 successes, pushed"."""
    what = roll.get("skill") or f"{roll.get('attribute')} (attribute test)"
    successes = roll.get("successes", 0)
    outcome = f"{successes} success{'es' if successes != 1 else ''}" if successes else "no successes, a failure"
    pushed = f", pushed and took {roll['pushCondition']}" if roll.get("pushed") and roll.get("pushCondition") else ", pushed" if roll.get("pushed") else ""
    reason = f" ({roll['reason']})" if roll.get("reason") else ""
    return f"[Dice] {roll.get('characterName')} rolled {what}{reason}: {roll.get('poolSize')} dice, {outcome}{pushed}."
