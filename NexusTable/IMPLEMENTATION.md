# Nexus Table: implementation

What is implemented on branch `feature/nexus-table` of `youcapmatte/vaesen_game`, in the folder `NexusTable/`.
Nexus Table is a web tabletop for playing Vaesen by voice, with an AI Gamemaster.

## Parts

| Folder | What it is | Stack |
|---|---|---|
| `Api/Nexus.Table.Api` | The API host | .NET 10, MVC controllers, Newtonsoft JSON, SignalR, OpenAPI with Scalar at `/docs` and Swagger UI at `/swagger`, port 5240 |
| `Api/Nexus.Table` | Domain: models, storage, services, controllers, hub, rules file | Built on `VaesenAPI/Framework/Vaesen.Framework.Core` (DynamoDB `BaseRepository`, Auth0 JWT, result envelope, error middleware) |
| `Api/Nexus.Table.Tests` | API tests | xUnit, WebApplicationFactory |
| `Web` | The web app | Next.js 16 App Router, React 19, Mantine 8, SWR, `@auth0/nextjs-auth0`, `@microsoft/signalr`, `livekit-client`, `@livekit/components-react`, port 3030 |
| `GmAgent` | The voice Gamemaster | Python 3.12, LiveKit Agents 1.8 (Deepgram, ElevenLabs, Anthropic plugins), Anthropic SDK, uv |
| `NexusTable.sln` | The solution (API projects + `Vaesen.Framework.Core`) | |

Every API response has the shape `{ success, result, errorMessage, errorCode }`.

## Sign-in and accounts

- **Two ways to sign in.**
  - **Discord through Auth0.** The web app uses `@auth0/nextjs-auth0`: routes under `/auth/*`, the Discord connection, and the API audience `https://api.nexustable.game`. The API validates the Auth0 JWT.
  - **Dev login** (Development only). `POST /api/dev/login` with `{ playerId }` returns a signed token for a seed player: Mattias, Ingrid, Johan, Astrid or Erik. `GET /api/dev/players` lists them. The title page shows them as seats. Each browser tab keeps its own dev sign-in (sessionStorage), so one browser can be several players.
- **Accounts.**
  - **Fields:** subject (the token's `sub`), display name, avatar URL, role (`Player` or `Host`), and the active character.
  - **First call:** `GET /api/me` returns the account, creating it on the first call and adding it to the Society.
  - **Profile:** `PUT /api/me/profile` stores the Discord name and avatar; the web app sends them after an Auth0 sign-in.
  - **Host role:** comes from the `Nexus:HostSubjects` config list (`dev|mattias` in Development).

## The Society

- There is one Society, with its id from `Nexus:SocietyId`. It is created on first use with a name and headquarters from config.
- **Fields:** name, headquarters, member account ids, mystery ids.
- **Endpoints:**
  - `GET /api/society` returns the Society with each member: name, avatar, role, and their character.
  - `PATCH /api/society` changes the name or headquarters; only the host can.

## Characters

- **Model:** account, Society, name, archetype, age group, age, attributes, skills, Resources, talents, motivation, trauma, dark secret, relationships, memento, equipment, description, Conditions, physically/mentally Broken, experience, Advantage, insights and defects, portrait URL, and notes.
- **The rules file** (`Api/Nexus.Table/Rules/vaesen-rules.json`, served at `GET /api/rules`):
  - 4 attributes, 12 skills, 3 age groups, 10 archetypes, 54 talents, 6 Conditions, 8 Resource levels, and the 36-entry d66 memento table
  - each archetype has a main attribute, main skill, three talents, a Resources range, equipment picks, names, and motivation, trauma, dark secret and relationship suggestions
  - the mechanics are as in the rulebook; descriptions are paraphrased, and the suggestions are written fresh, not copied from the book
- **Rolling a memento:** `POST /api/rules/memento` rolls d66 on the server.
- **Creating a character:** `POST /api/characters` creates one for the caller and makes it their active character. The API checks it against the creation rules (`Services/CharacterRules.cs`):
  - attribute points by age: 15, 14 or 13
  - each attribute 2–4, the archetype's main attribute up to 5
  - skill points by age: 10, 12 or 14
  - each skill up to 2, the main skill up to 3
  - skill points can raise Resources above the archetype's minimum, up to its maximum
  - exactly one of the archetype's three talents
  - one item per equipment pick
  - motivation, trauma, dark secret and memento required
  - no two characters in the Society share an archetype (`archetype_taken`)
- **Other endpoints:**
  - `GET /api/characters` (the Society's characters) and `GET /api/characters/{id}`.
  - `PATCH /api/characters/{id}`: motivation, dark secret, relationships, memento, equipment, description, Advantage, portrait, notes. The owner or the host can change these.
  - `PUT /api/characters/{id}/conditions` with `{ condition, active }` marks or heals a Condition. When all three Conditions of a type are already marked, the next one makes the character Broken of that type. Healing a Condition of a type ends Broken of that type.
- **The dark secret:** only the character's owner and the host receive it. Everyone else gets it blank, in reads and in hub broadcasts.

## Dice

- **Rolling:** `POST /api/rolls` with `{ characterId, skill | attribute, difficulty, bonus, useAdvantage, reason }`.
  - **Who:** players roll for their own character; the host can roll for anyone.
  - **The pool:** attribute + skill + difficulty (−3 to +3) + gear/help (−3 to +3) + 2 for the Advantage, minus 1 for each Condition of the test's type (physical for Physique and Precision, mental for Logic and Empathy). At least one die is always rolled.
  - **The roll:** the server throws the dice with a cryptographic RNG; each six is a success. A Broken character can't roll.
- **Pushing:** `POST /api/rolls/{id}/push` with `{ condition }`. Only a failed roll can be pushed, and only once (`already_pushed`). The sixes stay and the other dice are thrown again. The character takes the chosen Condition of the roll's type, or becomes Broken when none is left.
- **Recent rolls:** `GET /api/rolls` returns the Society's latest rolls (50), newest first.
- **What a roll stores:** the pool part by part, the dice, the pushed dice, successes and the reason.

## Live updates: the SignalR hub at `/hubs/table`

- **Server to clients:**
  - `societyChanged`
  - `characterChanged`, with the dark secret left out
  - `rollMade`
  - `presenceChanged`: who is connected, and who holds push-to-talk
- **Clients to server:** `GetPresence`, and `SetPushToTalk(bool)`.
- The token goes in the `access_token` query parameter.

## The voice call (LiveKit)

- **Joining:** `POST /api/call/join` returns `{ url, token, room, identity }` for the Society's room, `nexus-{societyId}`.
  - The token is HS256 with the LiveKit API key and secret, and grants join, publish, subscribe and data.
  - The identity is the account id, and the name is the character's name.
  - The participant metadata (account id, display name, role, character id and name) is set by the API, so the agent can trust it.
- **Bringing in the Gamemaster:** `POST /api/call/gamemaster` (host only) dispatches the agent `nexus-gm` into the room. It calls LiveKit's `AgentDispatchService/CreateDispatch` over Twirp with a room-admin token.
- **Agent-only endpoints**, authorized by the `X-Agent-Key` header (`LiveKit:AgentKey`):
  - `GET /api/agent/table`: the Society and every player's full character, dark secrets included.
  - `GET /api/agent/rolls/{id}`: a roll as the server threw it.
- **Without LiveKit settings** the call endpoints answer `call_not_configured`.

## The Gamemaster agent (`GmAgent/agent.py`, `GmAgent/nexus_gm/`)

- **Registration and start.**
  - It is a LiveKit Agents worker registered as `nexus-gm` (`AgentServer`, `@server.rtc_session`).
  - When dispatched, it reads the table from the API and builds the system prompt: the Gamemaster's rules plus a brief of every character.
  - Then it joins the room as an audio-only participant and greets the table.
- **Push-to-talk turns** (`turn_detection="manual"`). The agent's audio input is off except during a turn, so table talk never reaches Claude.
  - `start_turn` RPC: interrupts any speech and starts listening to that one participant.
  - `end_turn` RPC: stops listening and commits the turn.
  - `cancel_turn` RPC: drops the turn.
- **Speech to text:** Deepgram `nova-3` in English, with the character names boosted. The finished turn is prefixed with the speaker, e.g. `[Elin Gregorius, played by Ingrid]`. Empty turns get no reply.
- **Claude** (`nexus_gm/claude.py`):
  - Called through the Anthropic SDK: `claude-haiku-5-5`, streaming, thinking disabled, effort `low`, `max_tokens` 2048.
  - The rules and the table brief are a cached system prompt, and the conversation is cached too.
  - A refusal is answered with a line in the fiction.
- **The Gamemaster's instructions** (`nexus_gm/prompt.py`):
  - spoken prose only, usually one to three sentences
  - NPCs answer in character immediately when addressed
  - every NPC line is marked `<npc name="...">...</npc>`
  - tests are asked for with a skill and a difficulty
  - the players' dark secrets are used but never revealed
  - `[Table]` lines are the host's instructions
  - with no prepared mystery, it improvises a small situation at the Society's headquarters
- **Voices** (`nexus_gm/speech.py`):
  - The reply streams to ElevenLabs `eleven_flash_v2_5` as it is generated.
  - A streaming parser splits it into narrator and NPC pieces. Each speaker change starts a new ElevenLabs stream in that voice: the narrator voice, or a shared NPC voice (both set by environment variables).
  - The transcript shows NPC lines as `Name: ...`.
- **Rolls:** a roll id sent on the data topic `nexus.roll` is read back from the API, turned into a `[Dice]` line, and narrated.
- **Host controls** (the `gm_control` RPC, host only, checked from the metadata): `pause`, `resume`, `repeat` (the last reply), `recap`, and `leave`.
- **Published participant attributes:**
  - `nexus.gm.state`: `idle`, `listening`, `thinking`, `speaking` or `paused`
  - `nexus.gm.latencyMs`: milliseconds from releasing the talk key to the first audio
- **Tests** (`GmAgent/tests`, 7): the speaker parser, chat history to Messages API conversion, and roll descriptions.

## Storage

- **Interfaces** (`Api/Nexus.Table/Data/Stores.cs`) for accounts, the Society, characters and rolls. There are two implementations, chosen by `DataService:Provider`:
  - **`DynamoDb`** (`DynamoStores.cs`), in table `nexus-table-{env}`:
    - `{id}/ITEM` for each model's JSON
    - `SUBJECT#{sub}/SUBJECT` keeps one account per login
    - GSI1 `SOCIETY#{id}` with `CHAR#{name}` for characters and `ROLL#{time}` for rolls
  - **`LocalFile`** (`LocalFileStores.cs`): one JSON file per model in `.data/`. This is the default in Development. It keeps the same rules: version checks on update, soft delete, and unique logins.
- **Models** carry `id` (`{type}-{guid}`), `createdAt`, `updatedAt`, `createdBy`, `updatedBy`, `isDeleted` and `version`.

## The web app (`Web/`)

- **Look.** Like the Vaesen rulebook:
  - colours: umber night background, parchment cards, sepia ink, blood red and tarnished gold
  - type: IM Fell English SC for display, IM Fell English for flourishes, EB Garamond for body text
  - SVG ornaments, corner flourishes and noise textures
- **Structure** follows Redkeep:
  - `app/` routes
  - `modules/<feature>/`
  - `common/api/*.ts` (typed endpoints)
  - `common/client/core` (`fetcher`, `useApiData` on SWR, `useApi`)
  - `common/client/security/SecurityProvider` (Auth0 or dev token)
  - `common/client/signalr/useTable` (the hub connection, presence, events)
  - `proxy.ts` (the Auth0 middleware)
- **`/` (title page):** Sign in with Discord, plus the dev seat picker.
- **`/society` (Society hall):** the Society's name and headquarters, member cards with their characters, empty chairs up to four, a prompt to create a character, and host editing.
- **`/characters/new` (character wizard):** nine pages for the twelve rulebook steps:
  1. archetype: taken ones are disabled
  2. age group, age and name: with name suggestions
  3. attributes
  4. skills and Resources: with live point budgets
  5. talent
  6. motivation, trauma and dark secret: with archetype suggestions
  7. relationships: with every other character prefilled
  8. memento (a d66 roll or the table) and equipment
  9. description and review on the sheet
  
  It checks each step as you go, with the same rules as the API. The draft is kept in the browser across reloads.
- **`/characters/{id}` (character sheet):**
  - laid out on parchment: attributes with their skills as pips, Resources, experience, Conditions as checkboxes with Broken, talents with their effects, and the personal fields
  - the owner and the host can mark Conditions and edit the Advantage and notes
  - the dark secret is visible only to them
- **`/table`, the main screen:**
  - **Left column:**
    - The call bar: Join the call, mute/unmute, camera on/off, Leave.
    - Four seats with the player's camera video (or their portrait), mic state, name, archetype and player. A seat glows gold while that person speaks, red while they hold push-to-talk, and fades when they aren't connected. Clicking a seat opens that character's sheet.
    - The Gamemaster's seat: whether it's present, its state, and the latency badge. The host sees Bring in the Gamemaster, and once it's there, Host controls: pause/resume, repeat, recap, send away.
  - **Centre:**
    - The scene panel with the Society's headquarters.
    - The push-to-talk bar: the key, hold or toggle, and a settings popover.
    - The dice overlay: every roll lands for everyone as 3D bone dice, with a red Sight star on each six and the outcome. Pushes rethrow only the non-sixes.
    - The clue shelf.
  - **Right column:**
    - The log, newest first: rolls (pool breakdown, faces, pushed faces, outcome, a Push button on your own failed rolls) and what was said to and by the Gamemaster (from LiveKit transcriptions).
    - The dice tray: rolling for (host), skill or attribute test with the pool size, difficulty −3 to +3, gear and help, Advantage, a reason, and a live pool preview.
  - **Drawer:** the character sheet ("My sheet").
- **Push-to-talk:**
  - the key is configurable: backtick, T, V, Caps Lock, or a mouse side button
  - hold or toggle, saved in the browser
  - it's ignored while typing in a field, and released when the tab loses focus
  - pressing it shows the table that you're talking to the Gamemaster (hub) and opens the Gamemaster's turn (LiveKit RPC); releasing it closes the turn
  - after your own roll or push, the browser sends the roll id to the Gamemaster
- **Phone layout:** the pages fit a phone without sideways scrolling. On the table the columns stack, the seats scroll sideways, and the push-to-talk bar sits under the scene. The wizard shows a compact step line.
- **Tests** (10, Jest): the wizard's point budgets and step checks, and the dice pool preview and push rules.

## Configuration

- **API** (`appsettings.json`, `appsettings.Development.json`, user-secrets id `nexus-table-api`):
  - `Auth0:Domain`, `Auth0:Audience`
  - `DataService:Provider`, `DataService:TableName`, `LocalData:Directory`
  - `Cors:Origins`
  - `Nexus:SocietyId`, `SocietyName`, `Headquarters`, `HostSubjects`, `RecentRolls`
  - `LiveKit:Url`, `ApiKey`, `ApiSecret`, `AgentName`, `AgentKey`, `TokenLifetimeHours`
  - `Dev:SigningKey`, `Dev:Players`
- **Web** (`Web/.env.local`, from `.env.example`):
  - `NEXT_PUBLIC_API_URL`, `NEXT_PUBLIC_DEV_LOGIN`
  - `APP_BASE_URL`, `AUTH0_DOMAIN`, `AUTH0_CLIENT_ID`, `AUTH0_CLIENT_SECRET`, `AUTH0_SECRET`, `AUTH0_AUDIENCE`, `AUTH0_CONNECTION`
- **Gamemaster agent** (`GmAgent/.env.local`, from `.env.example`):
  - `LIVEKIT_URL`, `LIVEKIT_API_KEY`, `LIVEKIT_API_SECRET`
  - `DEEPGRAM_API_KEY`, `ANTHROPIC_API_KEY`, `ELEVEN_API_KEY`
  - `NEXUS_API_URL`, `NEXUS_AGENT_KEY`
  - optional: `NEXUS_GM_MODEL`, `NEXUS_NARRATOR_VOICE`, `NEXUS_NPC_VOICE`, `NEXUS_TTS_MODEL`

## Running

```bash
dotnet run --project NexusTable/Api/Nexus.Table.Api
```

```bash
cd NexusTable/Web && npm install && npm run dev
```

```bash
cd NexusTable/GmAgent && uv sync && uv run agent.py dev
```

Tests:

```bash
dotnet test NexusTable/NexusTable.sln
```

```bash
cd NexusTable/Web && npm test
```

```bash
cd NexusTable/GmAgent && uv run pytest
```

| Suite | Tests | Result |
|---|---|---|
| API | 19 | pass |
| Web | 10 | pass |
| Gamemaster agent | 7 | pass |

The Next.js production build succeeds.
