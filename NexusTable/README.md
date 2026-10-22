# Nexus Table

A web tabletop where Claude is the Gamemaster and up to four friends play Vaesen mysteries by voice.
The design is in [DESIGN.md](DESIGN.md). This folder has the web app and API around the table (milestone 1) and
the voice Gamemaster (milestone 2: LiveKit call, push-to-talk, a Python agent running Deepgram, then Claude, then ElevenLabs).

| Folder | What |
|---|---|
| `Api/Nexus.Table.Api` | The .NET 10 host: OpenAPI + Scalar docs, CORS, SignalR hub. |
| `Api/Nexus.Table` | Accounts, the Society, characters, dice, the table hub. Built on `VaesenAPI/Framework/Vaesen.Framework.Core` (DynamoDB `BaseRepository`, Auth0, result envelope). |
| `Api/Nexus.Table/Rules/vaesen-rules.json` | The rulebook's character rules: archetypes, attributes, skills, talents, Conditions, mementos. Mechanics as in the book; descriptions paraphrased. |
| `Api/Nexus.Table.Tests` | xUnit tests: creation rules, rolls and pushes, permissions, dark-secret privacy. |
| `GmAgent` | The Gamemaster: a Python LiveKit Agents worker (`agent.py`, `nexus_gm/`). |
| `Web` | Next.js 16 (App Router) + React 19 + Mantine 8 + SWR, laid out like Redkeep (`app/`, `modules/<feature>`, `common/api`, `common/client`). |

## Running it locally

You need the .NET 10 SDK and Node 20.9 or later. Docker and AWS are not needed: in Development the API keeps its data in JSON files
(`Api/Nexus.Table.Api/.data`, ignored by git).

```bash
dotnet run --project NexusTable/Api/Nexus.Table.Api
```

```bash
cd NexusTable/Web && cp .env.example .env.local && npm install && npm run dev
```

- Web: http://localhost:3030. Without Auth0 configured, pick a seat on the title page (dev login). Mattias is the host.
- API docs: http://localhost:5240/docs.
- Each browser tab keeps its own dev sign-in, so one browser can be several players.

Tests:

```bash
dotnet test NexusTable/NexusTable.sln
```

```bash
cd NexusTable/Web && npm test && npm run lint
```

## What works

- **Sign-in.** Discord through Auth0 (`@auth0/nextjs-auth0`, straight to the Discord connection), or the dev login. The API creates the account on first call and seats it in the Society. Hosts are listed in `Nexus:HostSubjects`.
- **The Society.** One guild (`Nexus:SocietyId`), its members and their characters. The host can rename it and its headquarters.
- **Character creation by the book.** Twelve steps on nine pages:
  - archetype, one per Society
  - age group, with 15/10, 14/12 or 13/14 points
  - name
  - attributes: 2–4, the main one up to 5
  - skills: up to 2, the main one 3, with skill points also buying Resources
  - one of three talents
  - motivation, trauma and dark secret
  - relationships
  - a d66 memento rolled on the server
  - equipment picks
  
  The API validates everything again (`CharacterRules.cs`). The browser keeps the draft across a reload.
- **The character sheet.** Shown on parchment, with Conditions (Broken after the fourth of a type), notes and the Advantage. The dark secret is shown only to its owner and the host: the API leaves it out for everyone else, including in broadcasts.
- **The table, Roll20-style.**
  - Seats down the side show who is connected, plus a Gamemaster seat.
  - A scene stage with a clue shelf sits in the middle.
  - On the right are the log and the dice tray.
  - The sheet is one click away.
- **Dice.**
  - The pool is attribute + skill + difficulty (−3 to +3) + gear/help + Advantage (+2), minus 1 per Condition of the test's type, with at least one die.
  - Dice are thrown on the server.
  - Everyone sees them land as 3D bone dice; sixes show the Sight star.
  - A failed roll can be pushed once: the sixes stay, the rest are rethrown, and the player takes a Condition of the roll's type.
- **Push-to-talk.** A configurable key (backtick, T, V, Caps Lock, mouse side buttons), held or toggled. The whole table sees who is talking to the Gamemaster, and only that speech reaches it.
- **The call (LiveKit).**
  - Players join from the table and appear in their seats on camera.
  - Everyone hears everyone.
  - The host brings in the Gamemaster as an audio-only fifth seat, then controls it from there: pause, resume, repeat, recap, send away.
  - The Gamemaster's seat shows what it is doing, and the time from releasing the talk key to its first word.
  - The log shows what was said to and by the Gamemaster.

## Configuration

API (`appsettings.json`, override per environment):

| Key | |
|---|---|
| `Auth0:Domain`, `Auth0:Audience` | The Auth0 tenant and the API identifier (default `https://api.nexustable.game`). |
| `DataService:Provider` | `DynamoDb` (default) or `LocalFile` (Development). |
| `DataService:TableName` | `nexus-table-{env}`, separate from the game's `vaesen-{env}`. |
| `Cors:Origins` | Where the web app runs. |
| `Nexus:HostSubjects` | Token subjects that get the host role, for example Mattias's Auth0 `user_id`. |

Web (`.env.local`): see [.env.example](Web/.env.example).

### Auth0 with Discord (not set up yet)

1. In Auth0, create a **Regular Web Application**.
   - Allowed callback URL: `http://localhost:3030/auth/callback`
   - Allowed logout URL: `http://localhost:3030`
2. Create an **API** with the identifier `https://api.nexustable.game`.
3. Add the Discord social connection (Authentication > Social; it needs a Discord application's client id and secret) and enable it for the application.
4. Fill in `AUTH0_*` in `.env.local` and `Auth0:Domain` in the API settings.
5. After your first sign-in, put your `user_id` in `Nexus:HostSubjects`.

### DynamoDB (not set up yet)

Create `nexus-table-dev` in eu-central-1 with the same schema as the game's table: `PK`/`SK` strings, and GSI `GSI1` on `GSI1PK`/`GSI1SK` (projection ALL). See `DynamoDbTable.CreateAsync` and [VaesenAPI/docs/DynamoDB.md](../VaesenAPI/docs/DynamoDB.md). To use it, run the API with the `http (DynamoDB)` launch profile.

Items:

- `{id}/ITEM` for every model
- `SUBJECT#{sub}/SUBJECT` for one account per login
- GSI1 `SOCIETY#{id}` with `CHAR#{name}` for characters and `ROLL#{time}` for rolls

## The voice Gamemaster

The loop for one turn:

1. Holding the talk key sends a `start_turn` RPC to the agent, and it starts listening to that one player.
2. Releasing it sends `end_turn`. Deepgram finishes the transcript, and the agent prefixes it with the speaker: `[Elin Gregorius, played by Ingrid]`.
3. Claude Haiku 5.5 answers, streaming. Thinking is off, effort is low, and the rules and the table brief are prompt-cached (`nexus_gm/claude.py`).
4. The reply streams to ElevenLabs as it comes. NPC lines (`<npc name="...">...</npc>`) switch to the NPC voice (`nexus_gm/speech.py`).

Table talk never reaches Claude, because the agent's audio input is off outside a turn.

Rolls reach the Gamemaster too. The roller's browser sends the roll id over LiveKit, and the agent reads the roll back from the API (`GET /api/agent/rolls/{id}`), so a browser can't invent results.

The agent reads the full table, dark secrets included, from `GET /api/agent/table` with the shared `X-Agent-Key`.

### Setting it up

1. **Accounts.** Create a LiveKit Cloud project, and get keys for Deepgram, ElevenLabs and the Anthropic API (set a monthly spend limit in the Console).
2. **API secrets** (stay out of git):
   ```bash
   cd NexusTable/Api/Nexus.Table.Api
   dotnet user-secrets set "LiveKit:Url" "wss://<project>.livekit.cloud"
   dotnet user-secrets set "LiveKit:ApiKey" "<key>"
   dotnet user-secrets set "LiveKit:ApiSecret" "<secret>"
   ```
3. **The agent.** Install [uv](https://docs.astral.sh/uv/). Then copy `GmAgent/.env.example` to `GmAgent/.env.local` and fill it in, and run:
   ```bash
   cd NexusTable/GmAgent && uv sync && uv run agent.py dev
   ```
   Tests: `uv run pytest`.
4. **Play.**
   1. Join the call from the table.
   2. As the host, press **Bring in the Gamemaster**.
   3. Hold the talk key and speak; let go to hear the answer.
   4. Read the latency badge on the Gamemaster's seat (target: about 1 s).

Voices default to two ElevenLabs premade voices. Set `NEXUS_NARRATOR_VOICE` and `NEXUS_NPC_VOICE` to the ones you like.

## Next milestones (from DESIGN.md)

1. **Measure and tune the voice loop.** Measure with real keys; then try the routing step (Haiku decides between NPC speech, a question, a rules question and an action), per-NPC prompts and distinct voices.
2. **The mystery generator.** Sonnet or Opus writes the hidden GM file (the vaesen, the truth, NPCs, clues with several routes, locations). It is stored in S3 with a reference in DynamoDB. The host's *Start a mystery* button and the clue shelf are already in place for it.
3. **Sessions and the log.** Every utterance, roll, clue and state change in order, for replay and evaluation. Rolls are already stored per Society.
