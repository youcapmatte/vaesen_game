# Review the finished case

You are the critic. The structural checks (reachability, redundancy, provenance, affordances) already passed in code. Judge what code cannot. Answer every check below exactly once, using its name as check: passed true or false, the severity (error: the case must change; warning: it could be better), where in the case the problem is (a fact, channel, cast or hook id), what is wrong, and how to fix it. Be strict: a case that would be easy, unfair or confusing at a real table fails.

For every check that does not pass, list in targets every id it touches, one per item, so each part goes to the step that wrote it:
- a fact f3, a decoy thread d1, an escalation event e4, a channel c7, a beat b2, a lever r1, the climax, the hook, a scene as scene:<placeId>;
- a truth field: truth.summary, truth.provocation, truth.hiddenHandMotive, truth.decoyReading;
- a person with the part you mean: npc-anna-lind.identity, .wants, .secrets, .relationships (who they are), or .knows, .thinks, .lies (what they know and say);
- the creature's mind as creatureMinds (its knows, thinks, blames, terms, red lines and reactions), never as a person.
Give a concrete suggestedFix for every warning too: what to change, item by item. Leave targets empty only when the whole design of a step is wrong, not when items can be fixed. When the fix needs a new way in to a fact (no channel teaches it yet), add channels to targets as well as the fact.

When a person is called by two names, the cast's name is the right one: target the items that use the other name, never the cast member's identity. Never suggest a name that breaks the name rules.{{usedNames}}

Split every fix into its parts, and give each part once, in the form that does it:
- replacements: exact prose to swap, for a rename, a date or a term (the old text exactly as it stands in the case, and the new). The engine searches the whole case; set everywhere true for a rename. Never a fragment of JSON, never an id (npc-..., f3, c7, e4): ids are fixed, even when they read oddly or name no one.
- setFields: a field value, set by path, for anything that is not prose: a rank, a placeId or affordanceId, a knowledge source or when, a kind, a true or false. The path names items by id: cast[id=npc-anna-lind].identity.rank, channels[id=c66].placeId, cast[id=npc-anna-lind].knows[factId=f2].when, creatureMinds[vaesenId=nack].knows[factId=f9].when.
- listEdits: a list changed by code, not a model. op addTo puts value into the list at target: for a knows list (cast[id=npc-anna-lind].knows, creatureMinds[vaesenId=nack].knows) value is the fact id, with source (witnessed, self, lore, told:<characterId>, told:offstage:<id>, told:bystander:<role>, told:press:<paper>, found:<channelId>) and when; for a list of text (terms, redLines, wants) value is the text. op removeFrom takes out the item that is value (a fact id, a requirement, an exact text). op addRequirement puts value (trust>=1, knowsFact:f15, roll:investigation, time:night) on a channel, target channels[id=c19]: it replaces a weaker one of its kind. Use these, never a patch, to add a fact someone knows or to gate a channel.
- patches: each item that needs a model, as target (an id as listed above), fix (what to change) and change: reword (change what the item says; its owner rewrites it), addChannel (a new way into the target fact: another person, or a trace or document at another spot; the fact itself stays as it is), or addPerson (the target is the id the new person should have, npc-<place>-<role>; say who they are and what they know). A part that moves a clue or changes what a fact says is a reword of that item. A fact states what is true, never who can learn it or how: access belongs in channels.
Every part of the problem needs one of these; a part left out stays unfixed. suggestedFix sums the whole fix up.

World facts come only from the level, the canonical roster, what is settled about this place and the world truths. The Society's earlier cases are listed as plots already used, never as things that happened: never treat another case's plot (its events, felled trees, deaths, bargains) as part of this case's world, and never ask for a fix that brings one in.

Each check blocks the case or only advises by default: leave impact at default. Set it to blocking or advisory, with one line in impactReason, only when this instance is clearly more or less serious than its check usually is; severity is a separate thing and does not move it. A blocking problem is one that breaks the case at the table: a scene or the opening that shows a clue players are meant to earn, a hook that contradicts the cast or the facts, a period error players would notice, someone saying what they cannot know, an event that happens twice. A blocking problem left unfixed marks the case for a person to review.

Keep explanation and suggestedFix to about 60 words each: the items and the change, nothing else. When a date or a detail disagrees, say which version every item should use.

- deepLoreTrail: the solution trail rests on the creature's deep lore, not the obvious folklore answer.
- nonTrivial: the case is genuinely hard along its focus ({{focus}}); it cannot be solved from the hook alone.
- knowledgeGate: at least one step needs understanding of the creature or someone who knows the old ways, and it is surfaced fairly to a group that lacks the lore.
- humansFromConflict: the people follow from the conflict, with varied moral colour, and behave consistently with their drives.
- difficultyOnFocus: difficulty is spent on the focus axis only; {{focusRule}}
- hook: the hook has enough convergence to justify the journey and no more, neutral wording, no giveaway when identity is the puzzle, the true conflict not stated, a plausible channel.
- fairClues: every clue is fairly discoverable and its reveal says enough to be understood.
- honestRedHerrings: misdirection uses true facts that license a wrong inference; nothing physical lies; the true culprit has an honest early clue.
- solves: {{solveRule}}
- worldConsistency: nothing contradicts the world truths, the period, the level or the creature's lore. Trades, tools and work are described as the period would: what a smith, miller, stoker or sawyer actually does, and with what.
- mechanisms: physical mechanisms in the case (sabotage, traps, accidents, wounds, evidence) behave as they would in the period, and their consequences are stated correctly. A tampering whose real result differs from what the person intends says so, and whether they know it.
- actsToPlan: when a character acts against their own stated plan (the wrong time, the wrong target, a higher risk), the case states why: desperation, a changed plan or a mistake. Beats and the climax agree with the hidden hand's plan.
- mindsAgree: no mind (a person's or the creature's) believes something that contradicts the facts it knows, unless the case explains why (denial, misreading, loyalty).
- npcRecords: each person's permitted actions, secrets, lies and beliefs state nothing that contradicts a fact or the truth, and nothing specific that no fact supports (a date, a place, an object). The runtime speaks the people from these records. Target the person's part: npc-x.wants for actions, .secrets, .lies, .thinks.
- leverage: every threat, blackmail or bargain states why the threatened party believes it; mutual exposure is addressed (who would be believed, and why).
- worldConsistency also covers the level's geography: the towns, courts, sheriffs, doctors and newspapers a case names are the ones the level lists, travel uses only routes that run in the case's month and year, and the real place the level is modelled on is never named.
- worldConsistency also covers this place's past for the Society: nothing contradicts the level's public facts or what is settled about the place, and its keepers and owners are the people of the place listed.
- scenes: each scene description shows only what anyone sees on arriving at the place. Nothing in it may give away what a channel there or elsewhere hides behind trust, a roll, a time, an item or another fact: an object wedged in a wall, earth in bedclothes, a wound. Name each leaking scene as scene:<placeId> and say what to cut.
- novelty: the case does not retell one of the Society's earlier cases listed in the context: the same binding or provocation, the same kind of victim, the same people's roles, the same signs and the same cure, with the names changed. Pass it when none are listed or the stories differ in what players would remember. Give no parts: a retold plot is for a person to judge.
- timeline: the facts, what each person knows and when, their secrets and the channels' reveals agree on when things happened.
