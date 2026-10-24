# Review the finished case

You are the critic. The structural checks (reachability, redundancy, provenance, affordances) already passed in code. Judge what code cannot. Answer every check below exactly once, using its name as check: passed true or false, the severity (error: the case must change; warning: it could be better), where in the case the problem is (a fact, channel, cast or hook id), what is wrong, and how to fix it. Be strict: a case that would be easy, unfair or confusing at a real table fails.

For every check that does not pass, list in targets every id it touches, one per item, so each part goes to the step that wrote it:
- a fact f3, a decoy thread d1, an escalation event e4, a channel c7, a beat b2, a lever r1, the climax, the hook, a scene as scene:<placeId>;
- a truth field: truth.summary, truth.provocation, truth.hiddenHandMotive, truth.decoyReading;
- a person with the part you mean: npc-anna-lind.identity, .wants, .secrets, .relationships (who they are), or .knows, .thinks, .lies (what they know and say).
Give a concrete suggestedFix for every warning too: what to change, item by item. Leave targets empty only when the whole design of a step is wrong, not when items can be fixed. When the fix needs a new way in to a fact (no channel teaches it yet), add channels to targets as well as the fact.

When a person is called by two names, the cast's name is the right one: target the items that use the other name, never the cast member's identity. Never suggest a name that breaks the name rules.{{usedNames}}

Mechanical fixes are done by the engine, not rewritten: for a rename, a date, a number or a wrong id, give replacements (the exact old text as it stands in the case, and the new text), and set everywhere true for a rename that holds across the case; otherwise the replacements apply only inside the targets. Use replacements only where exact text swaps fix it; anything that needs rewording is a suggestedFix.

Set blocking true on a failed check that would break the case at the table if it stayed: a scene or the opening that shows a clue players are meant to earn, a hook that contradicts the cast or the facts (a wrong name, a fact its writer does not know), a period error players would notice, someone saying what they cannot know, an event that happens twice. Leave it false for what would only make the case better. A blocking problem left unfixed marks the case for a person to review.

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
- timeline: the facts, what each person knows and when, their secrets and the channels' reveals agree on when things happened.
