# Nexus Table

A web tabletop where Claude is the Gamemaster and up to four friends play Vaesen mysteries by voice.
The design is in [DESIGN.md](DESIGN.md). This folder has the web app and API around the table (milestone 1) and
the voice Gamemaster (milestone 2: LiveKit call, push-to-talk, a Python agent running Deepgram, then Claude, then ElevenLabs).

| Folder | What |
|---|---|
| `Api/Nexus.Table.Api` | The .NET 10 host: OpenAPI with Scalar and Swagger UI, CORS, SignalR hub. |
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
- API docs: http://localhost:5240/docs (Scalar) or http://localhost:5240/swagger (Swagger UI), both on the document at /openapi/v1.json.
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
3. Claude Sonnet 5.5 answers, streaming (`NEXUS_GM_MODEL`; Haiku 5.5 is faster and weaker at keeping a case straight). Thinking is off, effort is low, and the rules, the table brief and where the session stands are prompt-cached (`nexus_gm/claude.py`).
4. The reply streams to ElevenLabs as it comes. NPC lines (`<npc name="...">...</npc>`) switch to the NPC voice (`nexus_gm/speech.py`).

Table talk never reaches Claude, because the agent's audio input is off outside a turn.

Rolls reach the Gamemaster too. The roller's browser sends the roll id over LiveKit, and the agent reads the roll back from the API (`GET /api/agent/rolls/{id}`), so a browser can't invent results. It speaks once the dice have landed on everyone's screen: 2.2 s of tumbling, a little more per die, then `NEXUS_ROLL_PAUSE` (0.8 s).

### Sessions, the play log and resuming

A mystery may take several evenings. Each is a **session** (`/api/sessions`):

- **Starting.** The host starts it from the table (or bringing in the Gamemaster starts it). It begins from the play state the last session ended with. Ending it is the host's: the Gamemaster's scribe brings the play state up to date first. Closing the case, or another case coming to the table, ends it too.
- **The log.** Everything said to and by the Gamemaster, every roll, every test it asks for and every note goes into the session log with its time (`SessionEvent`). The table's log panel shows it, and it survives a reload.
- **The play state.** A scribe (`nexus_gm/scribe.py`, `NEXUS_SCRIBE_MODEL`, Sonnet 5.5) rewrites where the mystery stands after each exchange, in the background, in the case file's ids: the story so far, the scene, facts found and how, people met, beats and events that happened, deeds done, open threads. A found fact is never dropped.
- **Resuming.** When the Gamemaster joins, it gets the play state and the session's last 40 log lines. The first session opens with the hook. A later session opens with a recap. A Gamemaster that rejoins mid-session picks up where it left off.
- **Notes to the Gamemaster.** "Note to the Gamemaster" at the table: typed, or dictated while a button is held (the talk key in note mode). Logged with the time. The Gamemaster takes notes as true and doesn't answer them aloud.
- **Tests.** The Gamemaster asks for a test aloud and with a `<test who=".." skill=".." difficulty=".." sixes=".." why=".."/>` tag, which isn't spoken. The table shows the test with the dice the character has and the sixes it needs, and "Roll it" throws it. Rolls carry `successesNeeded` (1 to 5), and a roll short of it can be pushed.
- **Following up.** The case file's **Sessions** tab shows each session's play state, and downloads the play log (every session, or one) as Markdown for Claude, to read with the case report.

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
   Tests: `uv run pytest`. The agent is pinned to Python 3.12 (`.python-version`), since LiveKit's native wheels lag new releases.
   If `uv sync` stops with "Missing expected target directory for Python minor version link", make the virtual environment from the Python uv just installed, then sync again:
   ```bash
   uv venv --python "%APPDATA%/uv/python/cpython-3.12.15-windows-x86_64-none/python.exe" && uv sync --python .venv/Scripts/python.exe
   ```
4. **Play.**
   1. Join the call from the table.
   2. As the host, press **Bring in the Gamemaster**.
   3. Hold the talk key and speak; let go to hear the answer.
   4. Read the latency badge on the Gamemaster's seat (target: about 1 s).

Voices default to two ElevenLabs premade voices. Set `NEXUS_NARRATOR_VOICE` and `NEXUS_NPC_VOICE` to the ones you like.

## Docker images

Each service has a Dockerfile. They are for hosting (AWS ECS Fargate is the plan); local development doesn't need Docker.

| Image | Build (from the repository root) | Port | Settings |
|---|---|---|---|
| API | `docker build -f NexusTable/Api/Dockerfile -t nexus-table-api .` | 8080 | Environment variables with `__` for `:`, e.g. `Auth0__Domain`, `LiveKit__ApiSecret`, `Cors__Origins__0`, `Nexus__HostSubjects__0`. Runs as `Production`, so DynamoDB and no dev login. |
| Web | `docker build -t nexus-table-web --build-arg NEXT_PUBLIC_API_URL=https://api.example.com NexusTable/Web` | 3030 | `NEXT_PUBLIC_API_URL` (required) and `NEXT_PUBLIC_DEV_LOGIN` are build arguments, baked into the browser code. The `AUTH0_*` values and `APP_BASE_URL` are read at runtime. |
| Gamemaster agent | `docker build -t nexus-gm NexusTable/GmAgent` | 8081 (health only) | The `GmAgent/.env.example` variables. Runs `agent.py start`. |

The API image's context is the repository root because it references `VaesenAPI/Framework`; `NexusTable/Api/Dockerfile.dockerignore` keeps the rest of the repository out of it.

## Deploying to AWS

[Infra/](Infra/README.md) is an AWS CDK app (C#) that runs everything at https://nexus.norse.games: DynamoDB, the three images on ECS Fargate behind one HTTPS load balancer, and the DNS. Deploy with `Infra/deploy.ps1`; the README there walks through the first deploy.

## Next milestones (from DESIGN.md)

1. **Measure and tune the voice loop.** Measure with real keys; then try the routing step (Haiku decides between NPC speech, a question, a rules question and an action), per-NPC prompts and distinct voices.
2. **The mystery generator.** Sonnet or Opus writes the hidden GM file (the vaesen, the truth, NPCs, clues with several routes, locations). It is stored in S3 with a reference in DynamoDB. The host's *Start a mystery* button and the clue shelf are already in place for it.
3. **Sessions and the log.** Every utterance, roll, clue and state change in order, for replay and evaluation. Rolls are already stored per Society.
