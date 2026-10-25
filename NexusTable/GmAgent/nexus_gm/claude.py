"""The Gamemaster's replies from Claude, through the Anthropic SDK.

Claude Sonnet 5.5 by default: running a case at a table (keeping its secrets, its clues' requirements and its pace
straight over a long case file) needs more than the fastest model; NEXUS_GM_MODEL=claude-haiku-5-5 trades that for
speed. Thinking off and low effort for the first word, streaming, and the long stable part of the prompt (rules, the
table, where the session stands) cached.
"""

from __future__ import annotations

import logging
import os
from collections.abc import AsyncIterator
from dataclasses import dataclass

import anthropic

logger = logging.getLogger("nexus-gm.claude")

MODEL = os.getenv("NEXUS_GM_MODEL", "claude-sonnet-5-5")


def thinking_off(model: str) -> dict | None:
    """Thinking off, as each model takes it: Sonnet 5.5 refuses "disabled" and turns it off with "between_tools";
    Opus 5.5 cannot turn it off (None: leave the field out); Haiku and the rest take "disabled" at low effort."""
    if "sonnet-5-5" in model:
        return {"type": "between_tools"}
    if "opus-5-5" in model:
        return None
    return {"type": "disabled"}
MAX_TOKENS = 2048

# Said in the fiction when Claude declines a turn, so the table isn't left in silence.
REFUSAL_LINE = "The candle gutters, and the moment passes. Let us take that another way."


@dataclass(frozen=True)
class Turn:
    role: str  # "user" or "assistant"
    text: str


def to_messages(turns: list[Turn]) -> list[dict]:
    """
    Turns as Messages API messages: alternating roles, starting with the user, no empty text.
    Consecutive turns of one role are joined (several players can speak before the Gamemaster answers).
    """
    messages: list[dict] = []
    for turn in turns:
        text = turn.text.strip()
        if not text or turn.role not in ("user", "assistant"):
            continue
        if messages and messages[-1]["role"] == turn.role:
            messages[-1]["content"] += "\n\n" + text
        else:
            messages.append({"role": turn.role, "content": text})

    if not messages or messages[0]["role"] != "user":
        messages.insert(0, {"role": "user", "content": "[Table] The session begins."})
    if messages[-1]["role"] != "user":
        # The Messages API answers a user turn; this happens when the agent is asked to speak on its own.
        messages.append({"role": "user", "content": "[Table] Continue."})
    return messages


class GamemasterModel:
    def __init__(self, rules: str, table_brief: str, session_block: str = "") -> None:
        self._client = anthropic.AsyncAnthropic()
        # Rules, then the table, then where the session stands: stable while the Gamemaster sits, so cached.
        self._system = [
            {"type": "text", "text": rules},
            {"type": "text", "text": table_brief, "cache_control": {"type": "ephemeral"}},
        ]
        if session_block:
            self._system.append({"type": "text", "text": session_block, "cache_control": {"type": "ephemeral"}})

    def update_table(self, table_brief: str) -> None:
        self._system[1] = {"type": "text", "text": table_brief, "cache_control": {"type": "ephemeral"}}

    async def stream_reply(self, turns: list[Turn]) -> AsyncIterator[str]:
        """The reply as it is generated. Speaker tags (<npc ...>) are left in for the voice switch."""
        extra = {"thinking": thinking} if (thinking := thinking_off(MODEL)) is not None else {}
        async with self._client.messages.stream(
            model=MODEL,
            max_tokens=MAX_TOKENS,
            system=self._system,
            messages=to_messages(turns),
            # Dialogue wants the fastest first word: no thinking, low effort.
            output_config={"effort": "low"},
            # Also cache the conversation so far, so each turn only pays for what is new.
            cache_control={"type": "ephemeral"},
            **extra,
        ) as stream:
            async for text in stream.text_stream:
                yield text
            final = await stream.get_final_message()

        usage = final.usage
        logger.info(
            "claude reply: stop=%s in=%s cached=%s out=%s",
            final.stop_reason,
            usage.input_tokens,
            usage.cache_read_input_tokens,
            usage.output_tokens,
        )
        if final.stop_reason == "refusal":
            # Keep the table moving.
            yield " " + REFUSAL_LINE
