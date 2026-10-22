# Nexus Table: status

What is built on branch `feature/nexus-table` as of 2026-10-22, what has been verified, and what hasn't.
Design: [DESIGN.md](DESIGN.md). How to run: [README.md](README.md).

## Summary

| Area | Built | Verified |
|---|---|---|
| API (.NET 10) | Yes | 19 automated tests pass; used from the browser |
| Web app (Next.js) | Yes | 10 unit tests pass, typecheck clean, production build succeeds, clicked through in the browser |
| Voice call (LiveKit) in the web app | Yes | Only without keys (shows "the call isn't set up yet") |
| Gamemaster agent (Python) | Yes | 7 tests pass; imports and every LiveKit call checked against livekit-agents 1.8.8; **never run in a real room** |
| Discord sign-in (Auth0) | Wired | **Not tested**: no Auth0 tenant yet; the dev login is used instead |
| DynamoDB storage | Written | **Not tested**: no AWS table yet; development stores JSON files |
| Docker images (API, web app, agent) | Yes | API published and web app built with the same steps as the images; **no image built yet** (no Docker on this machine) |
| Mystery engine (`VaesenAPI/Framework/Vaesen.Framework.Mystery`) | Yes: content pack, assignment, generation pipeline, validator, critic, CaseLab, ContentTool, host API | 42 engine tests and 2 API tests pass; **never run against the real Anthropic API** (no key); content is draft, not reviewed |
| Session log, runtime GM in .NET, AWS deployment | No | - |

## Commits (oldest first)

1. `05c7455` API: accounts, the Society, Vaesen characters and server-side dice
2. `ec3778b` Web: Next.js app with the rulebook look, sign-in and API plumbing
3. `02f78ce` Society hall, character sheet and the twelve-step character wizard
4. `f56930b` The table: camera seats, scene stage, animated dice, roll log, push-to-talk
5. `80b0db8` README, design sketch, phone layouts
6. `4cccd15` Call API: LiveKit join tokens, host dispatch of the Gamemaster, agent-only table
7. `4d55997` Voice Gamemaster: LiveKit call at the table and a Python GM agent
8. `314e232` GM agent: pin Python 3.12, lock dependencies, document the uv workaround

## Done in detail

### API: `Api/`

- **Projects.** A host project (`Nexus.Table.Api`), a domain library (`Nexus.Table`), tests (`Nexus.Table.Tests`) and `NexusTable.sln`. They reuse `VaesenAPI/Framework/Vaesen.Framework.Core` for the DynamoDB `BaseRepository`, Auth0 JWT, the result envelope and the error middleware.
- **Storage.**
  - DynamoDB repositories write to their own table, `nexus-table-{env}`.
  - A LocalFile store (JSON files in `.data/`) is the default in Development. It has the same rules: version checks, soft delete and unique logins.
- **Accounts.** An account is created on the first call to `/api/me` and seated in the Society. The host role comes from `Nexus:HostSubjects`. `PUT /api/me/profile` stores the Discord name and avatar.
- **The Society.** There is one, from `Nexus:SocietyId`. `GET /api/society` returns its members and their characters; `PATCH` is host only.
- **Characters** (`/api/characters`). Validated against the rulebook rules (`Services/CharacterRules.cs`):
  - attribute and skill points by age: 15/10, 14/12 or 13/14
  - attributes 2–4, with the main attribute up to 5
  - skills up to 2, with the main skill up to 3
  - skill points can buy Resources, within the archetype's range
  - one of the archetype's three talents
  - one item per equipment pick
  - one character per archetype in the Society
- **Conditions.** `PUT /api/characters/{id}/conditions` marks or clears one; a fourth Condition of a type means Broken.
- **The rules file** (`Rules/vaesen-rules.json`, served at `GET /api/rules`):
  - 10 archetypes, 54 talents, 12 skills, 6 Conditions and a 36-entry d66 memento table
  - the mechanics are as in the book
  - descriptions are paraphrased, and the motivation, trauma, secret and relationship suggestions are original
- **Dark secret privacy.** Only the owner and the host can read it, in reads and in hub broadcasts.
- **Dice** (`/api/rolls`). Rolled on the server with a cryptographic RNG.
  - The pool is attribute + skill + difficulty + gear/help + Advantage, minus 1 per Condition of the matching type, with at least one die.
  - Sixes are successes.
  - A failed roll can be pushed once. The sixes stay, the rest are rethrown, and the player takes a Condition of the roll's type (or becomes Broken).
- **The SignalR hub** (`/hubs/table`). Broadcasts Society, character, roll and presence changes, and who is holding push-to-talk.
- **The call** (`/api/call`).
  - LiveKit join tokens for the Society's room. The participant metadata (account, role, character) is set by the API, so the agent can trust it.
  - Host-only dispatch of the `nexus-gm` agent through LiveKit's AgentDispatchService.
- **Agent-only endpoints** (`/api/agent/table` and `/api/agent/rolls/{id}`, using an `X-Agent-Key` header). The full table with dark secrets, and rolls as the server threw them.
- **Dev login** (`/api/dev`, Development only). Seed players Mattias (host), Ingrid, Johan, Astrid and Erik.
- **Docs.** OpenAPI with Scalar at `/docs`.

### Web: `Web/`

- **Stack.** Next.js 16 App Router, React 19, Mantine 8 and SWR, laid out like Redkeep (`app/`, `modules/<feature>`, `common/api`, `common/client`).
- **Look.** Like the rulebook: umber pages, parchment cards, IM Fell and EB Garamond type, ornaments drawn in SVG, nothing copied from the book.
- **Sign-in.** Discord through `@auth0/nextjs-auth0` once the `AUTH0_*` settings are in place. Otherwise a dev seat picker; each browser tab keeps its own seat, so one browser can be several players.
- **Society hall.** Members, their characters and empty chairs. The host can edit the Society.
- **Character wizard.** The 12 rulebook steps on 9 pages:
  - live point budgets
  - suggestions per archetype
  - a d66 memento rolled on the server
  - a draft that survives a reload
  - a final review on the sheet
- **Character sheet.** Laid out on parchment like the book's. Conditions, notes and the Advantage can be edited by the owner and the host.
- **The table, Roll20-style.**
  - Seats down the side with camera video, mic state, a speaking glow and a red glow while holding push-to-talk.
  - A Gamemaster seat with its state, the latency of the last answer, and the host's controls: bring in, pause, resume, repeat, recap and send away.
  - A scene stage, a clue shelf (empty for now), and the log with rolls and the Gamemaster conversation.
  - The dice tray with a live pool preview.
  - The sheet one click away in a drawer.
- **Dice.** 3D bone dice for everyone, with a red Sight star on the six.
- **Push-to-talk.** A configurable key or mouse side button, held or toggled. It works only while the tab has focus.
- **Phone width.** The pages fit without sideways scrolling.

### Gamemaster agent: `GmAgent/`

- **What it is.** A Python LiveKit Agents worker named `nexus-gm` (`agent.py`, `nexus_gm/`), pinned to Python 3.12 with dependencies locked in `uv.lock`.
- **Push-to-talk turns.** The `start_turn` and `end_turn` RPCs, sent from the web app, decide what it hears. The rest of the time its audio input is off, so table talk never reaches Claude.
- **Speech to text.** Deepgram nova-3, with the character names boosted. Each turn is prefixed with who is speaking.
- **Claude.** Through the Anthropic SDK: `claude-haiku-5-5`, streaming, thinking off and low effort. The rules and the table brief are prompt-cached. A refusal is answered with a line in the fiction.
- **Voices.** The reply streams to ElevenLabs (`eleven_flash_v2_5`) in two voices: the narrator, and a shared NPC voice for `<npc name="...">` lines.
- **Rolls.** The agent reads each roll back from the API before narrating it.
- **Host controls.** Pause, resume, repeat the last reply, recap, leave.
- **Latency.** It publishes the time from releasing the talk key to its first audio.

## Mystery engine (added 2026-10-22)

Built to the build spec, [SPEC.md](SPEC.md), milestones 3 and 4. The engine is a framework library, `VaesenAPI/Framework/Vaesen.Framework.Mystery`, so the game's backend can use the same code later. Its [README](../VaesenAPI/Framework/Vaesen.Framework.Mystery/README.md) describes how it works and how to run it.

- **Content pack** (`Content/vaesen/`). Written by Claude into the content schemas and checked by the ContentValidator; all of it is draft:
  - the setting and an affordance vocabulary of 113 tags
  - 4 creatures: Näck, nisse, skogsrå, werewolf
  - 14 place templates and 8 text levels (3 in October)
  - 10 conflict archetypes
  - 6 canonical characters
- **Assignment.** Deterministic, with no model: creature cooldown, level and affordance fit, the reason kind and origin, whether there is a hidden hand, and the focus. Hook channel and hook-giver type are kept off the last ones used.
- **Generation.** Typed steps run in order: conflict, escalation, humans, clues, then the critic. Each step uses Opus 5.5 with structured outputs, streaming, prompt caching and server-side fallbacks, and is re-rolled with its issues fed back. The whole case is re-rolled when a step gives up, and every call is logged.
- **Structural validator.** Reachability by walking the fact graph, redundancy, single-channel loss, entry styles, provenance, decoys and fair play, affordance fit, vocabulary, cast coherence, giveaways, opening convergence, resolution, beats and tension.
- **CaseLab and ContentTool.** Command-line tools in `VaesenAPI/Tools/`.
- **Host-only API** (`/api/mysteries`): generate in the background and poll the job, list cases, read a case file and its Markdown report. Players get 403. Nothing reaches the voice Gamemaster yet: spec milestone 4 says to wait until you are happy with the generated cases.
- **Not done:**
  - a real run against the API
  - your review and approval of the content
  - a web page for the host's CaseLab view and the Start mystery button
  - DynamoDB/S3 storage for case packets (files only for now)
  - seasons and the persistent cast

## Not done yet

- **Never run with real services.** That means the LiveKit call, the agent in a room, Deepgram, ElevenLabs and Claude. Latency against the ~1 s target is unmeasured, and the first real run will likely need fixes.
- **Not set up** (all your accounts): Auth0 with Discord, the DynamoDB table, LiveKit Cloud, and the Deepgram, ElevenLabs and Anthropic keys.
- **The default voices.** The two ElevenLabs voice ids are premade voices I didn't check; set your own.
- **Not built yet:**
  - persistent NPCs
  - clues on the shelf
  - the session log for replay and evaluation
  - logging of table talk
  - the routing step (NPC speech, question, rules question or action)
  - per-NPC voices
  - Advantage tracking per session
  - deployment to AWS (the Dockerfiles exist)

## Working on another computer

Prerequisites: git, the .NET 10 SDK, Node 20.9 or later, and [uv](https://docs.astral.sh/uv/) (it installs Python 3.12 itself).

```bash
git clone https://github.com/youcapmatte/vaesen_game.git && cd vaesen_game && git checkout feature/nexus-table
```

Then follow [README.md](README.md) → *Running it locally* and *The voice Gamemaster → Setting it up*.

These are not in git, so set them up again on each machine:

- `Web/.env.local`: copy `Web/.env.example`.
- `GmAgent/.env.local`: copy `GmAgent/.env.example`, then add the keys.
- The API's LiveKit and Auth0 secrets: `dotnet user-secrets` in `Api/Nexus.Table.Api`.
- `Api/Nexus.Table.Api/.data/`: the local development data (dev characters and rolls). It starts empty on a new machine.

On the first computer the branch is checked out as a separate worktree at `C:\Projects\vaesen_game_nexus`, because Unreal Editor locks files in `C:\Projects\vaesen_game`. On a computer without Unreal open, a normal checkout is fine.
