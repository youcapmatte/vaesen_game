"""Who is speaking in the Gamemaster's reply.

Claude marks NPC speech as <npc name="Father Lind">...</npc>; everything else is the narrator. The reply streams
in small chunks, so tags can be split across chunks: SpeakerParser buffers just enough to tell a tag from text and
emits (speaker, text) pieces as early as it can, which keeps the first audio fast.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

NARRATOR = None

_OPEN = re.compile(r'<npc\s+name\s*=\s*"([^"]{1,80})"\s*>', re.IGNORECASE)
_CLOSE = re.compile(r"</npc\s*>", re.IGNORECASE)
# What an unfinished <npc ...> or </npc> tag can look like while it streams in.
_PARTIAL = re.compile(r"<(/?(n(p(c(\s[^>]*)?)?)?)?)?", re.IGNORECASE)
# The longest tag we accept; a "<" with this much text after it and no ">" is plain text.
_MAX_TAG = 100


@dataclass(frozen=True)
class Piece:
    """Text spoken by one speaker: None for the narrator, else the NPC's name."""

    speaker: str | None
    text: str


class SpeakerParser:
    """Feed it chunks of the streamed reply; it returns the pieces that are certain so far."""

    def __init__(self) -> None:
        self._buffer = ""
        self._speaker: str | None = NARRATOR

    @property
    def speaker(self) -> str | None:
        return self._speaker

    def feed(self, chunk: str) -> list[Piece]:
        self._buffer += chunk
        pieces: list[Piece] = []
        while self._buffer:
            lt = self._buffer.find("<")
            if lt == -1:
                pieces.append(Piece(self._speaker, self._buffer))
                self._buffer = ""
                break
            if lt > 0:
                pieces.append(Piece(self._speaker, self._buffer[:lt]))
                self._buffer = self._buffer[lt:]

            gt = self._buffer.find(">")
            if gt == -1:
                if len(self._buffer) > _MAX_TAG or not _PARTIAL.fullmatch(self._buffer):
                    # Not a tag after all: say the "<" and carry on.
                    pieces.append(Piece(self._speaker, self._buffer[0]))
                    self._buffer = self._buffer[1:]
                    continue
                break  # wait for the rest of the tag

            tag = self._buffer[: gt + 1]
            if (opened := _OPEN.fullmatch(tag)) is not None:
                self._speaker = opened.group(1).strip()
            elif _CLOSE.fullmatch(tag):
                self._speaker = NARRATOR
            else:
                pieces.append(Piece(self._speaker, tag))
            self._buffer = self._buffer[gt + 1 :]
        return [p for p in pieces if p.text]

    def finish(self) -> list[Piece]:
        """The rest of the buffer, at the end of the reply; an unfinished tag is dropped."""
        rest, self._buffer = self._buffer, ""
        if _PARTIAL.fullmatch(rest):
            return []
        return [Piece(self._speaker, rest)] if rest else []


def strip_tags(text: str) -> str:
    """The reply as plain text, with NPC lines prefixed by the speaker's name, for the log and the chat history."""
    parser = SpeakerParser()
    out: list[str] = []
    last: str | None = NARRATOR
    for piece in [*parser.feed(text), *parser.finish()]:
        if piece.speaker is not NARRATOR and piece.speaker != last:
            out.append(f"{piece.speaker}: ")
        last = piece.speaker
        out.append(piece.text)
    return re.sub(r"[ \t]{2,}", " ", "".join(out)).strip()
