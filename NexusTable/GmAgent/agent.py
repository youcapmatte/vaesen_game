"""
The Nexus Table Gamemaster: a LiveKit voice agent that joins the Society's call as the fifth, audio-only seat.

  players hold the talk key -> start_turn / end_turn RPC -> Deepgram (only that player's audio)
  -> Claude (nexus_gm/claude.py) -> ElevenLabs: the narrator's voice, and a second voice for NPC lines

Table talk is never sent to Claude: the agent's audio input is off except between start_turn and end_turn.
Dispatched by the API when the host brings in the Gamemaster (POST /api/call/gamemaster, agent name "nexus-gm"), which
opens a session of the mystery at the table. Everything said to and by the Gamemaster goes into the session's log,
a scribe keeps the play state the next session resumes from, tests it asks for go on the table, and notes from the
table (typed, or dictated with the talk key in note mode) are taken in without a reply.

Run locally: uv run agent.py dev  (see NexusTable/README.md)
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import time
from collections.abc import AsyncIterable
from typing import Any

import aiohttp
from dotenv import load_dotenv

load_dotenv(".env.local")

from livekit import agents, rtc  # noqa: E402
from livekit.agents import (  # noqa: E402
    Agent,
    AgentServer,
    AgentSession,
    ModelSettings,
    TurnHandlingOptions,
    llm,
)
from livekit.agents.llm import StopResponse  # noqa: E402
from livekit.plugins import anthropic as anthropic_plugin  # noqa: E402
from livekit.plugins import deepgram, elevenlabs  # noqa: E402

from nexus_gm import nexus_api  # noqa: E402
from nexus_gm.claude import MODEL, GamemasterModel, Turn  # noqa: E402
from nexus_gm.prompt import (  # noqa: E402
    GM_RULES,
    MOVE_ON,
    NUDGE,
    build_session_block,
    build_table_brief,
    opening_instruction,
    scribe_line,
    speech_keyterms,
)
from nexus_gm.scribe import Scribe  # noqa: E402
from nexus_gm.speech import NARRATOR, SpeakerParser, find_tests, strip_tags  # noqa: E402

logger = logging.getLogger("nexus-gm")

AGENT_NAME = os.getenv("NEXUS_AGENT_NAME", "nexus-gm")
# ElevenLabs voices: one narrator and one shared NPC voice for the first test (DESIGN.md: Voices).
NARRATOR_VOICE = os.getenv("NEXUS_NARRATOR_VOICE", "JBFqnCBsd6RMkjVDRZzb")
NPC_VOICE = os.getenv("NEXUS_NPC_VOICE", "N2lVS1w4EtoT3dr4eOWO")
TTS_MODEL = os.getenv("NEXUS_TTS_MODEL", "eleven_flash_v2_5")

ROLL_TOPIC = "nexus.roll"
NOTE_TOPIC = "nexus.note"
# The dice tumble for 2.2 s, each a little after the one before; the Gamemaster speaks a moment after the last lands.
DICE_TUMBLE_S = 2.2
DICE_STAGGER_S = 0.07
ROLL_PAUSE_S = float(os.getenv("NEXUS_ROLL_PAUSE", "0.8"))

TURN_SPEAK = "speak"
TURN_NOTE = "note"
TURN_PAUSED = "paused"
# A talk key held this long with nothing transcribed is logged: speech to text failed, or the microphone was off.
HELD_WITHOUT_WORDS_S = 1.5
# Log lines the API did not take are sent again, every few seconds, for a while.
LOG_RETRY_S = 5.0
LOG_RETRY_FOR_S = 15 * 60
ATTR_STATE = "nexus.gm.state"  # listening | thinking | speaking | paused
ATTR_LATENCY = "nexus.gm.latencyMs"  # release of the talk key -> first audio, last turn
ATTR_NUDGE = "nexus.gm.nudge"  # "on" or "off": the Gamemaster moves the world after a silence
# How long the table must be quiet before a nudge; off until the host turns it on.
NUDGE_AFTER_S = float(os.getenv("NEXUS_NUDGE_AFTER", "180"))


def _metadata(participant: rtc.RemoteParticipant | None) -> dict[str, Any]:
    try:
        return json.loads(participant.metadata) if participant and participant.metadata else {}
    except json.JSONDecodeError:
        return {}


def time_now() -> str:
    """Now, as the API reads a time: ISO 8601 in UTC."""
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def speaker_label(participant: rtc.RemoteParticipant | None) -> str:
    """[Elin Gregorius, played by Ingrid], from the metadata the API put in the join token."""
    meta = _metadata(participant)
    player = meta.get("displayName") or (participant.name if participant else None) or "someone"
    character = meta.get("characterName")
    return f"[{character}, played by {player}]" if character else f"[{player}, who has no character yet]"


class Gamemaster(Agent):
    def __init__(self, model: GamemasterModel) -> None:
        # Instructions live in GamemasterModel's system prompt; llm_node below never sends these.
        super().__init__(instructions="You are the Gamemaster of a Vaesen game.")
        self.model = model
        self.speaker: rtc.RemoteParticipant | None = None
        # What the held talk key is for: speaking to the Gamemaster, dictating a note, or speaking while it is
        # paused. Every one is recorded in the log; only the first is answered.
        self.turn_mode = TURN_SPEAK
        self.turn_started: float | None = None
        self.on_spoken: Any = None  # (speaker, text, mode) -> None, set once the session is up
        self.narrator_tts = elevenlabs.TTS(voice_id=NARRATOR_VOICE, model=TTS_MODEL)
        self.npc_tts = elevenlabs.TTS(voice_id=NPC_VOICE, model=TTS_MODEL)

    async def on_user_turn_completed(self, turn_ctx: llm.ChatContext, new_message: llm.ChatMessage) -> None:
        text = (new_message.text_content or "").strip()
        mode, self.turn_mode = self.turn_mode, TURN_SPEAK
        held = time.perf_counter() - self.turn_started if self.turn_started else 0.0
        if not text:
            # The talk key was held without words: no reply. Held a while, it is logged, so a failed transcript shows.
            if held >= HELD_WITHOUT_WORDS_S and self.on_spoken:
                self.on_spoken(self.speaker, "", mode)
            raise StopResponse()
        if mode != TURN_SPEAK:
            # A note, or words while the Gamemaster is paused: recorded and remembered, not answered.
            if self.on_spoken:
                self.on_spoken(self.speaker, text, mode)
            raise StopResponse()
        new_message.content = [f"{speaker_label(self.speaker)} {text}"]
        if self.on_spoken:
            self.on_spoken(self.speaker, text, mode)

    async def llm_node(
        self, chat_ctx: llm.ChatContext, tools: list[llm.FunctionTool], model_settings: ModelSettings
    ) -> AsyncIterable[str]:
        turns = [
            Turn(item.role, item.text_content or "")
            for item in chat_ctx.items
            if item.type == "message" and item.role in ("user", "assistant")
        ]
        async for text in self.model.stream_reply(turns):
            yield text

    async def tts_node(self, text: AsyncIterable[str], model_settings: ModelSettings) -> AsyncIterable[rtc.AudioFrame]:
        """
        Speaks each piece in its speaker's voice as soon as it arrives: a new ElevenLabs stream starts whenever the
        speaker changes, and its audio plays out before the next one's.
        """
        segments: asyncio.Queue[tuple[str | None, asyncio.Queue[str | None]] | None] = asyncio.Queue()

        async def split() -> None:
            parser = SpeakerParser()
            current: asyncio.Queue[str | None] | None = None
            speaker: object = object()  # no speaker yet

            def push(pieces) -> None:
                nonlocal current, speaker
                for piece in pieces:
                    if piece.speaker != speaker or current is None:
                        if current is not None:
                            current.put_nowait(None)
                        speaker, current = piece.speaker, asyncio.Queue()
                        segments.put_nowait((piece.speaker, current))
                    current.put_nowait(piece.text)

            try:
                async for chunk in text:
                    push(parser.feed(chunk))
                push(parser.finish())
            finally:
                if current is not None:
                    current.put_nowait(None)
                segments.put_nowait(None)

        splitter = asyncio.create_task(split())
        try:
            while (segment := await segments.get()) is not None:
                speaker, pieces = segment
                tts = self.narrator_tts if speaker is NARRATOR else self.npc_tts
                stream = tts.stream()

                async def feed(stream=stream, pieces=pieces) -> None:
                    while (piece := await pieces.get()) is not None:
                        stream.push_text(piece)
                    stream.end_input()

                feeder = asyncio.create_task(feed())
                try:
                    async for audio in stream:
                        yield audio.frame
                    await feeder
                finally:
                    feeder.cancel()
                    await stream.aclose()
        finally:
            splitter.cancel()

    async def transcription_node(self, text: AsyncIterable[str], model_settings: ModelSettings) -> AsyncIterable[str]:
        """What the table reads in the log: NPC lines as "Father Lind: ...", no tags."""
        parser = SpeakerParser()
        last: object = NARRATOR
        async for chunk in text:
            for piece in parser.feed(chunk):
                if piece.speaker is not NARRATOR and piece.speaker != last:
                    yield f"{piece.speaker}: "
                last = piece.speaker
                yield piece.text
        for piece in parser.finish():
            yield piece.text


server = AgentServer()


@server.rtc_session(agent_name=AGENT_NAME)
async def gamemaster(ctx: agents.JobContext) -> None:
    table = await nexus_api.get_table()
    model = GamemasterModel(GM_RULES, build_table_brief(table), build_session_block(table))
    played = table.get("session") if table.get("mystery") else None
    notes_changed: list[dict[str, Any]] = []
    scribe = Scribe(table["mystery"]["caseFile"], played, nexus_api.put_state, on_change=notes_changed.append) if played else None

    session = AgentSession(
        # Names of the characters help Deepgram hear them right.
        stt=deepgram.STT(model="nova-3", language="en", keyterm=speech_keyterms(table)),
        # The pipeline needs an LLM object; replies come from Gamemaster.llm_node (Anthropic SDK, nexus_gm/claude.py).
        llm=anthropic_plugin.LLM(model=MODEL),
        tts=elevenlabs.TTS(voice_id=NARRATOR_VOICE, model=TTS_MODEL),
        # Push-to-talk: the agent only listens between start_turn and end_turn.
        turn_handling=TurnHandlingOptions(turn_detection="manual"),
    )
    gm = Gamemaster(model)
    await session.start(room=ctx.room, agent=gm)
    session.input.set_audio_enabled(False)

    local = ctx.room.local_participant
    state = {"paused": False, "released_at": None, "last_reply": ""}
    outbox: list[dict[str, Any]] = []
    sending: dict[str, asyncio.Task | None] = {"task": None}
    quiet: dict[str, Any] = {"nudge": os.getenv("NEXUS_NUDGE", "off") == "on", "timer": None}

    def hush() -> None:
        """The table did something: the silence that a nudge waits for starts again once the Gamemaster has spoken."""
        if quiet["timer"]:
            quiet["timer"].cancel()
            quiet["timer"] = None

    def listen_for_silence() -> None:
        hush()
        if quiet["nudge"] and not state["paused"]:
            async def wait() -> None:
                await asyncio.sleep(NUDGE_AFTER_S)
                quiet["timer"] = None
                if quiet["nudge"] and not state["paused"]:
                    session.generate_reply(user_input=NUDGE)
            quiet["timer"] = asyncio.create_task(wait())

    def log(kind: str, speaker: str, text: str, account_id: str | None = None, line: str | None = None) -> None:
        """Into the session's log (in the background) and the scribe's next look."""
        if scribe:
            scribe.add(line or f"{speaker}: {text}")
        if not played:
            return

        outbox.append({"kind": kind, "speaker": speaker, "text": text, "accountId": account_id, "at": time_now()})
        if not sending["task"] or sending["task"].done():
            sending["task"] = asyncio.create_task(send_log())

    async def send_log() -> None:
        """Sends what is waiting, in order; what the API does not take is sent again until it does, or gives up after a while."""
        give_up = time.monotonic() + LOG_RETRY_FOR_S
        while outbox:
            batch = list(outbox)
            try:
                await nexus_api.log_events(batch)
            except (nexus_api.NexusApiError, aiohttp.ClientError, asyncio.TimeoutError) as error:
                if time.monotonic() > give_up:
                    logger.error("gave up logging %d line(s) after %d s: %s; they were: %s", len(batch), LOG_RETRY_FOR_S, error, batch)
                    outbox.clear()
                    return
                logger.warning("could not log %d line(s), trying again: %s", len(batch), error)
                await asyncio.sleep(LOG_RETRY_S)
                continue
            del outbox[: len(batch)]

    async def remember(text: str) -> None:
        """Adds a line to the conversation without a reply: a note from the table."""
        chat = gm.chat_ctx.copy()
        chat.add_message(role="user", content=text)
        await gm.update_chat_ctx(chat)

    async def notes_in() -> None:
        """The scribe's latest notes, once, as a [Scribe] line in the conversation."""
        if notes_changed:
            latest = notes_changed[-1]
            notes_changed.clear()
            await remember(scribe_line(latest))

    def spoken(participant: rtc.RemoteParticipant | None, text: str, mode: str) -> None:
        meta = _metadata(participant)
        who = meta.get("characterName") or meta.get("displayName") or "someone"
        label = f"{who} ({meta.get('displayName', '')})" if meta.get("characterName") else who
        if not text:
            log("playerSpoke", label, "(held the talk key; nothing was transcribed)", meta.get("accountId"))
            return
        if mode == TURN_PAUSED:
            log("playerSpoke", label, f"(while the Gamemaster was paused) {text}", meta.get("accountId"))
            asyncio.create_task(remember(f"{speaker_label(participant)} (said while you were paused; do not answer it) {text}"))
            return
        if mode == TURN_NOTE:
            name = f"{meta.get('displayName', who)} (the host)" if meta.get("role") == "Host" else who
            log("note", name, text, meta.get("accountId"), line=f"[Note from {name}] {text}")
            asyncio.create_task(remember(f"[Note from {name}] {text}"))
            if scribe:
                scribe.poke()
        else:
            log("playerSpoke", label, text, meta.get("accountId"))

    async def set_state(value: str) -> None:
        await local.set_attributes({ATTR_STATE: value})

    def caller(data: rtc.RpcInvocationData) -> rtc.RemoteParticipant | None:
        return ctx.room.remote_participants.get(data.caller_identity)

    gm.on_spoken = spoken

    @local.register_rpc_method("start_turn")
    async def start_turn(data: rtc.RpcInvocationData) -> str:
        """Payload {"mode": "note"} (the host or a player dictating a note): heard, logged, not answered."""
        try:
            note = json.loads(data.payload or "{}").get("mode") == "note"
        except json.JSONDecodeError:
            note = False
        # Paused, the Gamemaster still hears and records what is said to it; it only does not answer.
        mode = TURN_NOTE if note else TURN_PAUSED if state["paused"] else TURN_SPEAK
        hush()
        if mode == TURN_SPEAK:
            session.interrupt()
            await notes_in()
        session.clear_user_turn()
        gm.turn_mode = mode
        gm.turn_started = time.perf_counter()
        gm.speaker = caller(data)
        session.room_io.set_participant(data.caller_identity)
        session.input.set_audio_enabled(True)
        await set_state("listening")
        return "listening"

    @local.register_rpc_method("end_turn")
    async def end_turn(data: rtc.RpcInvocationData) -> str:
        session.input.set_audio_enabled(False)
        if gm.turn_mode != TURN_SPEAK:
            # Recorded, not answered. The mode is cleared where the transcript lands (on_user_turn_completed).
            await session.commit_user_turn(transcript_timeout=5.0, stt_flush_duration=1.0)
            return "noted" if gm.turn_mode == TURN_NOTE else "paused"
        state["released_at"] = time.perf_counter()
        await set_state("thinking")
        transcript = await session.commit_user_turn(transcript_timeout=5.0, stt_flush_duration=1.0)
        logger.info("%s said: %s", speaker_label(gm.speaker), transcript)
        return "ok"

    @local.register_rpc_method("cancel_turn")
    async def cancel_turn(data: rtc.RpcInvocationData) -> str:
        session.input.set_audio_enabled(False)
        session.clear_user_turn()
        gm.turn_mode = TURN_SPEAK
        return "ok"

    @local.register_rpc_method("gm_control")
    async def gm_control(data: rtc.RpcInvocationData) -> str:
        """Host controls (DESIGN.md: Host controls): pause, resume, repeat, recap, leave."""
        if _metadata(caller(data)).get("role") != "Host":
            return "forbidden"
        action = json.loads(data.payload or "{}").get("action")
        if action == "pause":
            state["paused"] = True
            session.interrupt()
            await set_state("paused")
        elif action == "resume":
            state["paused"] = False
            await set_state("idle")
        elif action == "repeat" and state["last_reply"]:
            session.say(state["last_reply"])
        elif action == "recap":
            session.generate_reply(user_input="[Table] Give the table a short spoken recap of the story so far, then ask what they do.")
        elif action == "move_on":
            hush()
            await notes_in()
            session.generate_reply(user_input=MOVE_ON)
        elif action in ("nudge_on", "nudge_off"):
            quiet["nudge"] = action == "nudge_on"
            await local.set_attributes({ATTR_NUDGE: "on" if quiet["nudge"] else "off"})
            listen_for_silence() if quiet["nudge"] else hush()
        elif action == "end_session":
            # The play state is brought up to date before the host ends the session, so the next one resumes from here.
            session.say("And there we leave it for tonight.")
            if scribe:
                await scribe.flush()
        elif action == "leave":
            session.say("The Gamemaster rises from the table. Until next time.")
            log("gmLeft", "The Gamemaster", "The Gamemaster left the table.")
            if scribe:
                await scribe.flush()
            # Whatever the log has not taken yet goes before the Gamemaster does.
            if sending["task"] and not sending["task"].done():
                try:
                    await asyncio.wait_for(asyncio.shield(sending["task"]), timeout=20)
                except asyncio.TimeoutError:
                    logger.error("left with %d log line(s) not taken by the API: %s", len(outbox), outbox)
            await asyncio.sleep(4)
            ctx.shutdown(reason="host closed the Gamemaster")
        else:
            return "unknown"
        return "ok"

    async def report_roll(roll_id: str) -> None:
        """A player's roll, checked against the server, then narrated once the dice have landed on everyone's screen."""
        try:
            roll = await nexus_api.get_roll(roll_id)
            model.update_table(build_table_brief(await nexus_api.get_table()))
        except nexus_api.NexusApiError as error:
            logger.warning("could not read roll %s: %s", roll_id, error)
            return
        line = nexus_api.describe_roll(roll)
        if scribe:
            scribe.add(line)
        hush()
        await asyncio.sleep(DICE_TUMBLE_S + DICE_STAGGER_S * len(roll.get("dice") or []) + ROLL_PAUSE_S)
        if not state["paused"]:
            await notes_in()
            session.generate_reply(user_input=line)

    async def take_note(event_id: str) -> None:
        """A note typed at the table, read back from the API, remembered without a reply."""
        try:
            event = await nexus_api.get_event(event_id)
        except nexus_api.NexusApiError as error:
            logger.warning("could not read note %s: %s", event_id, error)
            return
        text = f"[Note from {event.get('speaker', 'the table')}] {event.get('text', '')}"
        await remember(text)
        if scribe:
            scribe.add(text)
            scribe.poke()

    async def put_tests(reply: str) -> None:
        for test in find_tests(reply):
            try:
                await nexus_api.request_test(test)
            except nexus_api.NexusApiError as error:
                logger.warning("could not put the test %s on the table: %s", test, error)

    @ctx.room.on("data_received")
    def on_data(packet: rtc.DataPacket) -> None:
        if packet.topic not in (ROLL_TOPIC, NOTE_TOPIC):
            return
        try:
            body = json.loads(packet.data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        if packet.topic == ROLL_TOPIC and body.get("rollId"):
            asyncio.create_task(report_roll(body["rollId"]))
        elif packet.topic == NOTE_TOPIC and body.get("eventId"):
            asyncio.create_task(take_note(body["eventId"]))

    @session.on("agent_state_changed")
    def on_state(event) -> None:
        new_state = getattr(event, "new_state", None)
        if new_state == "speaking" and state["released_at"] is not None:
            ms = round((time.perf_counter() - state["released_at"]) * 1000)
            state["released_at"] = None
            logger.info("first audio %d ms after the talk key was released", ms)
            asyncio.create_task(local.set_attributes({ATTR_LATENCY: str(ms)}))
        if new_state in ("speaking", "thinking", "listening", "idle") and not state["paused"]:
            asyncio.create_task(set_state(new_state))
        # The silence a nudge waits for starts when the Gamemaster has finished speaking.
        if new_state == "speaking":
            hush()
        elif new_state in ("idle", "listening") and getattr(event, "old_state", None) == "speaking":
            listen_for_silence()

    @session.on("conversation_item_added")
    def on_item(event) -> None:
        item = getattr(event, "item", None)
        if item is not None and getattr(item, "role", None) == "assistant":
            state["last_reply"] = item.text_content or ""
            plain = strip_tags(state["last_reply"])
            logger.info("gm: %s", plain)
            if plain:
                log("gmSaid", "The Gamemaster", plain)
            if find_tests(state["last_reply"]):
                asyncio.create_task(put_tests(state["last_reply"]))
            if scribe:
                scribe.poke()

    log("gmJoined", "The Gamemaster", "The Gamemaster joined the table.")
    await local.set_attributes({ATTR_NUDGE: "on" if quiet["nudge"] else "off"})
    await set_state("idle")
    session.generate_reply(user_input=opening_instruction(table))


if __name__ == "__main__":
    agents.cli.run_app(server)
