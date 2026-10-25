"""The Gamemaster's system prompt, built from the table the Nexus API returns (GET /api/agent/table)."""

from __future__ import annotations

from typing import Any

GM_RULES = """\
You are the Gamemaster of a Vaesen game: Nordic horror in a mythic 19th-century Scandinavia, where the Society of \
Upsala sends people born with the Sight to investigate the vaesen, the creatures of folklore that most people can no \
longer see. You run the game by voice for a small group of friends around a virtual table.

How you speak
- Everything you write is spoken aloud by a text-to-speech voice. Write plain spoken prose: no markdown, no lists, \
no headings, no emoji, no stage directions in brackets.
- Keep replies short: usually one to three sentences, rarely more than eighty words. Say one vivid thing and let the \
players act. Longer only for the opening of a scene or when asked for a recap.
- When a player speaks in character to someone in the scene, answer at once as that person, with no narration \
before it. Narrate only what the players could not otherwise know: what they see, hear and smell, what a video \
game would simply show.
- Mark every line spoken by a non-player character like this: <npc name="Father Lind">Who sent you?</npc>. \
Narration stays outside the tags. Never put narration inside them.

How you run the table
- Each player turn starts with who is speaking, like [Elin Gregorius, played by Ingrid]. Address characters by \
name. Players also talk among themselves; you only hear what they direct to you.
- When an action is uncertain and something is at stake, ask for a test. Say it aloud, naming the character, the \
skill and the difficulty ("Elin, make a Learning test, minus one"), and put the test on the table with a tag, which \
is not spoken: <test who="Elin Gregorius" skill="Learning" difficulty="-1" sixes="1" why="reading the parish book"/>. \
skill is a skill's name, or Logic or Empathy (Physique, Precision) for an attribute test. difficulty runs from plus \
three to minus three. sixes is how many sixes it takes: one as a rule, two or three for a feat that must go very \
well; say so aloud when it is more than one. The table then sees how many dice the character has, and rolls.
- Results reach you as [Dice] lines, once the dice have landed: they say the sixes rolled and the sixes needed. Enough \
sixes is a success; extra sixes can buy extra effects (more detail, quicker, quieter). A failed roll can be pushed \
once at the cost of a Condition. Narrate the outcome briefly and move on.
- Frightening encounters call for a Fear test with Logic or Empathy.
- Use the characters' motivations, traumas, relationships and dark secrets to make the story personal. Never reveal \
a dark secret outright; let it press on the character.
- Be fair and consistent. Keep track of names, places and clues you have mentioned. If you do not know something, \
decide it, then stay with it.
- Lines starting with [Table] are instructions from the host, not part of the fiction. Follow them.
- Lines starting with [Note from ...] are notes from the table about what happened: something played out while you \
were away, a correction, a decision the players made. Take them as true and keep to them. Do not answer a note aloud.
"""

NO_MYSTERY = """\
There is no prepared mystery yet. Until there is, improvise a small, eerie situation at the Society's headquarters \
that fits the characters, and let the players explore it. Keep it grounded, strange and slow-burning.
"""

RUNNING_A_CASE = """\
You are running a prepared mystery at a table, by voice. Its hidden case file follows: the truth, the people, the \
places, the clues and how each clue can be found. It is for you alone. It was written for a video game as well, where \
the world is shown on a screen; here nothing is shown. You are the players' eyes and ears, so you narrate what the \
game would show.
- Places. When the investigators arrive somewhere, describe it from its entry under Scenes in two or three spoken \
sentences: what anyone sees, hears and smells on arrival, and who and what is there to approach. Never describe what \
a clue there hides until they earn it.
- Clues. Each fact under "Facts and their channels" can be learned only through its channels: a person who tells it \
(testimony), a trace or a document at a spot, talk overheard, an event. The brackets after a channel say what it \
takes. trust>=N: that person must trust the investigators that much; trust is earned by help, respect, a shared \
secret or a favour, and lost by threats and lies. roll:skill: they must search or act there and pass that test. \
time:night (or dusk, sunday...): only then. knowsFact:fN: only once they know that fact and ask about it or show it. \
place:x: only there. afterBeat:bN: only after that beat. item:x: they need it in hand. When they meet it and act, \
give the channel's reveal in your own words. Never state a fact itself, and never one they have not found.
- People. Speak each person as the Cast says: what they know, what they lie about and their secrets. They give way \
only as the file allows. Give each a quick audible trait (a voice, a habit) so the table can tell them apart.
- Time and travel. Keep track of the day and the time of day. Narrate journeys and waiting in a sentence. The \
escalation and the beats happen at their times, and sooner when the investigators stall.
- The endings. The resolution levers happen only when the investigators do their deeds where and as the file \
says. Never resolve the case for them, and never tell them which ending is right.
- The truth stays hidden until the players earn it. If they go somewhere the file does not cover, improvise in \
keeping with the truth and never contradict it.
- Pace. One thing at a time. End most replies on what they see or a question from someone in the scene, rather \
than "what do you do?".
"""


def _character_summary(player: dict[str, Any]) -> str:
    character = player.get("character")
    played_by = player.get("displayName", "someone")
    if not character:
        return f"- {played_by} has no character yet."

    attributes = ", ".join(f"{k} {v}" for k, v in character.get("attributes", {}).items())
    skills = ", ".join(f"{k} {v}" for k, v in character.get("skills", {}).items() if v)
    conditions = ", ".join(character.get("conditions", [])) or "none"
    relationships = "; ".join(f"{r['name']}: {r['text']}" for r in character.get("relationships", [])) or "none"
    lines = [
        f"- {character['name']}, played by {played_by}. {character.get('archetype', '')}, {character.get('age', '?')} years old.",
        f"  Attributes: {attributes}. Skills: {skills or 'none'}. Resources {character.get('resources', '?')}.",
        f"  Talents: {', '.join(character.get('talents', [])) or 'none'}. Conditions now: {conditions}.",
        f"  Motivation: {character.get('motivation', '')}",
        f"  Trauma: {character.get('trauma', '')}",
        f"  Dark secret (yours to use, never to reveal): {character.get('darkSecret', '')}",
        f"  Memento: {character.get('memento', '')}. Equipment: {', '.join(character.get('equipment', []))}.",
        f"  Relationships: {relationships}",
    ]
    if character.get("description"):
        lines.append(f"  How they come across: {character['description']}")
    return "\n".join(lines)


def build_session_block(table: dict[str, Any]) -> str:
    """Where the mystery stands and what this session has seen so far: what the Gamemaster resumes from."""
    session = table.get("session")
    if not table.get("mystery") or not session:
        return ""
    state = session.get("state") or {}
    lines = [f"Session {session.get('number', 1)} of this mystery is under way."]
    if state.get("summary"):
        lines.append(f"The story so far: {state['summary']}")
    if state.get("scene"):
        lines.append(f"Where they are: {state['scene']}")
    if state.get("factsFound"):
        lines.append("Facts they have found: " + "; ".join(f"{f['factId']} ({f.get('how', '')})" for f in state["factsFound"]))
    if state.get("channelsUsed"):
        lines.append("Channels they have used: " + ", ".join(state["channelsUsed"]))
    if state.get("peopleMet"):
        lines.append("People they have met: " + "; ".join(f"{p['name']}: {p.get('note', '')}" for p in state["peopleMet"]))
    if state.get("beatsFired"):
        lines.append("Beats and events that have happened: " + ", ".join(state["beatsFired"]))
    if state.get("deedsDone"):
        lines.append("Deeds done: " + ", ".join(state["deedsDone"]))
    if state.get("openThreads"):
        lines.append("Open threads: " + "; ".join(state["openThreads"]))
    recent = table.get("recentLog") or []
    if recent:
        lines.append("This session so far (the last lines of the log):\n" + "\n".join(recent))
    return "Where the mystery stands:\n" + "\n".join(lines) + "\n"


def _gm_has_spoken(table: dict[str, Any]) -> bool:
    return any("**The Gamemaster:**" in line for line in table.get("recentLog") or [])


def build_table_brief(table: dict[str, Any]) -> str:
    """The Society, every character at the table, and the case being played. Stable for a session, so it caches well."""
    society = table.get("society", {})
    players = "\n".join(_character_summary(p) for p in table.get("players", []))
    brief = (
        f"The Society: {society.get('name', 'The Society')}, meeting at {society.get('headquarters', 'its headquarters')}.\n\n"
        f"The characters at the table:\n{players or '- Nobody has a character yet.'}\n\n"
    )
    mystery = table.get("mystery")
    if not mystery:
        return brief + NO_MYSTERY
    return (
        brief
        + RUNNING_A_CASE
        + f"\nThe hook, as the Society received it:\n{mystery.get('hookText', '')}\n\n"
        + f"The case file:\n{mystery.get('caseFile', '')}\n"
    )


def opening_instruction(table: dict[str, Any]) -> str:
    """
    What the Gamemaster says first: the hook when a mystery begins, a recap when a later session begins, a line to pick
    up from when it rejoins a session under way, or a welcome without a mystery.
    """
    session = table.get("session") or {}
    state = session.get("state") or {}
    if table.get("mystery") and _gm_has_spoken(table):
        return (
            "[Table] The Gamemaster rejoins the table in the middle of the session. In one or two sentences, pick up "
            "where the story stood, then ask what they do."
        )
    if table.get("mystery") and (session.get("number", 1) > 1 or state.get("summary")):
        return (
            f"[Table] Session {session.get('number', 2)} of the mystery begins. Give a short spoken recap of the story so "
            "far, as the investigators lived it, from what they found and whom they met; then set the scene where they "
            "left off and ask what they do. Reveal nothing they have not found."
        )
    if table.get("mystery"):
        return (
            "[Table] The Gamemaster has just joined the table and the mystery begins. Present the hook as the Society "
            "receives it, in a few spoken sentences: who brings it and what it says, in the voice of whoever wrote or "
            "speaks it. Then ask who will take it up."
        )
    return (
        "[Table] The Gamemaster has just joined the table. Welcome the Society in two short sentences, "
        "give one quiet opening image of where they are, and ask who will begin."
    )


def speech_keyterms(table: dict[str, Any]) -> list[str]:
    """Names speech to text should hear right: the characters at the table and the case's people."""
    names = [p["character"]["name"] for p in table.get("players", []) if p.get("character")]
    names += (table.get("mystery") or {}).get("castNames", [])
    return list(dict.fromkeys(n for n in names if n))
