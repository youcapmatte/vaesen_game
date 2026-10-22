# Nexus Table – AI Game Master Voice Tabletop

Oct 21, 2026 · @Mattias Persson

## Purpose

A lightweight, web-based virtual tabletop (Roll20-style look) where Claude is the game master and up to four friends play Vaesen-style mysteries **by voice**. No maps or movement. The goal is to test what matters most for Nexus: whether the AI can generate coherent, engaging mysteries that evolve over time, and to build up a full session log for learning. The mystery generator, persistent NPC records and clue tracking should carry over into the Unreal game.

## Scope for the first test

- One Society (guild), up to 4 players.
- Discord login, character creation, start a mystery, play it by voice with cameras on.
- Out of scope: maps, tokens, 3D, competition metrics, Rosenberger sabotage, multiple Societies.

## Roles

- **Host (Mattias):** the only one who configures the AI connection (Anthropic API key) and presses **Start mystery**.
- **Players:** log in with Discord, create a character, join the call.

## Look and layout (Roll20-inspired)

- Camera tiles for the four players along the side.
- A large centre area for scene images, handouts and discovered clues instead of a battle map.
- Chat and roll log, with every GM and NPC line also shown as text.
- Character sheets one click away.
- An indicator showing who is currently talking to the GM.

## Architecture

- **Web app:** React with Mantine UI and a .NET Web API (handles login, characters, mysteries, logs and LiveKit join tokens), following the architecture of an existing template project that will be added to the repo. Visual style should feel like a game service rather than a business app. database on AWS for state, matching where Nexus now hosts its data. A normal web app that can be hosted anywhere.
- **Auth:** Auth0 with the Discord social connection.
- **Call and camera:** LiveKit (WebRTC) room with video for 4 players. The AI game master joins as a fifth, audio-only participant using the LiveKit Agents framework.
- **AI:** Claude through Anthropic's own API (pay-as-you-go via the Anthropic Console). No Azure Foundry or AWS Bedrock needed, so there are no quota approvals and no link to the business setup.
- **Speech to text:** streaming STT (e.g. Deepgram or Azure Speech) per player track, so every utterance carries the speaker's name.
- **Text to speech:** \[GM agent: a separate Python service using LiveKit Agents, since the framework runs in Python or Node, not .NET.\] ElevenLabs streaming API (decided). The Anthropic API has no voice, so an external TTS service is required. The GM agent worker runs on AWS and calls ElevenLabs and Anthropic directly.

## Talking to the GM: push-to-talk

- Players always hear each other. Table talk is never sent to Claude during play.
- Holding a **configurable key** (default suggestion: mouse side button or backtick rather than Shift) sends that audio to the GM. An optional toggle mode is available.
- Limitation: the browser only catches the key while the game tab has focus.
- The GM knows which player (and character) is speaking.

## The GM and NPC response model

- One AI game master who steps aside when a player speaks in character.
- **Routing step (fast model):** decides whether an utterance is in-character speech to an NPC, a question to the GM ("does he look angry?"), a rules question, or an action.
- **Reply format:** an ordered list of segments, each tagged with a speaker: `narrator` or a specific NPC. Example: narrator says "He slams the book shut", then the priest speaks his line.
- Speech directed at an NPC gets an **immediate in-character reply** with no GM preamble. Narration is added only when something needs explaining that a video game would show visually.

## Models and latency

- **NPC dialogue and routing:** Claude Haiku (fastest). Small, focused per-NPC prompt: identity, manner of speech, knowledge, secrets, clues they can give away.
- **Mystery generation, narration and scene changes:** Sonnet or Opus. Also updates clue and world state in the background during NPC conversations.
- **Latency tactics:** streaming end to end, TTS starts on the first finished sentence, prompt caching for the mystery file, extended thinking off for dialogue, compact context.
- **Target:** about 1 second from releasing push-to-talk to first audio.

## Voices

- Low priority for this test: the mystery is what's being tested.
- Start with one narrator voice plus one shared NPC voice so the switch is audible.
- Because segments are already speaker-tagged, giving each NPC a distinct voice later is a small change.

## Mystery generation

Generated up front by the large model when the host presses Start, and stored as a hidden GM file in the AWS database:

- The key vaesen (one or more) and the truth behind the mystery.
- NPCs with motives, secrets and relationships (persistent across mysteries).
- Clues: what, where, from whom, and what they point to (several routes to each key conclusion).
- The opening scene and the hook to the Society.
- Locations as described scenes.

## Session log (learning)

- Every utterance logged with a timestamp and speaker: player GM-directed speech, table talk (consented), narration, NPC lines, clues revealed, rolls and state changes.
- Used for replay and to evaluate the mystery: were clues found, did they lead anywhere, where did players get stuck or confused.
- Note: score with measurable signals, not only how fun it felt, since friends at a narrated table forgive more than players in a 3D world.

## Domain model

One DynamoDB table for this project (single-table design), separate from the game's table. Large documents (full mystery files, session logs) go in S3 with a reference in DynamoDB.

- **Account:** created on first Discord login (via Auth0). Discord id, display name, avatar, role (host or player).
- **Society:** the one guild for now. Name, members, headquarters, history of mysteries.
- **Character:** belongs to an account and to the Society. Built exactly as in the Vaesen rulebook (the PDF goes into the Claude Code project as the reference): archetype, age, attributes, skills, talents, motivation, trauma, dark secret, relationships, memento, equipment, plus conditions and experience over time.
- **Mystery:** belongs to the Society. Status (generated, in progress, solved, failed), the hidden GM file, current state: clues found, NPC states, scene, open threads.
- **NPC:** persistent across mysteries, with relationships and memory of past dealings with the Society.
- **Session:** one play session of a mystery. Participants, start and end time, state snapshot at the start and the end, reference to its log.
- **Log event:** every event in a session, in order (see Session log).

## Session lifecycle

1. **Joining:** no lobby. Like Teams or Zoom, the host opens the call for a mystery and players pop up as they connect. Camera and mic status shows on each tile, and anyone not yet connected is shown as missing.
2. **Start:** when the host is happy with who's there, they press Start. The AI game master joins and resumes from the mystery's saved state, with a short recap if it isn't the first session.
3. **Play:** push-to-talk to the GM, table talk recorded but not sent to the AI. State is saved continuously, not only at the end.
4. **Disconnects:** a dropped player just reconnects and pops back in. The host can pause the GM until they're back.
5. **Ending:** the host can close the call at any time for everyone. State is snapshotted and the log closed, so the next session continues from exactly there.
6. **Host controls:** pause and resume the GM, skip or repeat the last reply, force a scene recap, and close the call.

## Dice

- Players roll their own dice in the UI, Foundry-style, with animated 3D dice visible to everyone (e.g. a library such as dice-box, similar to Foundry's Dice So Nice).
- Vaesen rules: a pool of six-sided dice equal to attribute plus skill, adjusted by modifiers, gear and conditions. Each six is a success.
- When the GM calls for a roll, the app builds the pool from the character sheet and the difficulty modifier, and the player can push the roll as the rules allow.
- Results are generated on the server, so nobody can cheat, then animated for everyone, sent to the GM and saved in the session log.

## Security and separation

- Decision: use the Anthropic API directly instead of the business's Azure AI Foundry quota. The game is fully separate from the other business, so there's nothing for its audits to flag.
- Alternative if ever needed: Amazon Bedrock (Claude Haiku, Sonnet, Opus), but new accounts often start with low quotas that need raising through Service Quotas.
- Keep the API key server-side only, with a monthly spend limit set in the Anthropic Console.
- Players log in through Discord and Auth0, never as accounts in any business tenant.
- Players' voices and transcripts are personal data: get consent and keep retention short.

## Open questions

- Speech to text provider: Deepgram suggested (fast, instant sign-up); confirm during the prototype.
- Which Vaesen rules the GM enforces strictly vs. narrates loosely (conditions, pushing rolls, mental harm).
- How clues and handouts appear in the centre area (text cards, generated images?).
- How mysteries persist and evolve between sessions.

## Next steps

1. Create an Anthropic API account and confirm Haiku and Sonnet latency.
2. Prototype the LiveKit room with push-to-talk and a GM agent (STT → Claude → TTS).
3. Build the mystery generator and the GM file schema.
4. Discord login, Society and character creation.
5. The Roll20-style UI and the session log.

## Decisions so far

- Web-based, no Discord voice channel. Discord is used for login only.
- Claude via Anthropic's API directly. No Azure Foundry, no Bedrock, no quota requests.
- Hosting and data on AWS, with a separate DynamoDB table (single-table design) and S3 for large files.
- React + Mantine front end, .NET Web API, Python GM agent on LiveKit Agents.
- ElevenLabs for voices. One narrator voice and one shared NPC voice to start.
- Haiku for NPC dialogue and routing; Sonnet or Opus for mystery generation and narration.
- Push-to-talk (hold, configurable key) to address the GM; table talk is logged but never sent to the AI.
- No lobby: Teams/Zoom-style joining, host starts the GM and can close the call.
- Players roll animated dice in the UI; results decided server-side.

## Getting started with Claude Code

1. Export this doc as `DESIGN.md` into the repo, together with the Vaesen rulebook PDF and the template project.
2. Create accounts and keys: Anthropic API, LiveKit Cloud, Deepgram, ElevenLabs, a Discord application linked to Auth0, and AWS (new DynamoDB table, S3 bucket, own IAM role).
3. First milestone: a LiveKit room where the host holds the talk key, speaks, and hears Claude answer through ElevenLabs. Measure latency before building anything else.
