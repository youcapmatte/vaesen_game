# Step: arc

Shape how the case moves and how it can end.

Beats and climax
- Three to six beats at rising tension thresholds, between 0.1 and the climax threshold, each with what happens, who acts (actorId), where and when it requires, and the facts it puts into the world. Use them for the off-screen actions of the desperate and the creature's escalation.
- When a cast member acts in a beat or the climax, name the action (actionId) from their permitted actions. Desperation rises with tension: below {{mediumFrom}} only low acts, from {{mediumFrom}} medium ones, from {{highFrom}} high ones. A person who acts against their own plan (the wrong time, the wrong target, a higher risk) does so for a stated reason: desperation, a changed plan or a mistake.
- The climax comes at a threshold of about 0.8; {{climaxRule}}
- You only set the thresholds. The engine works out how much each fact raises the tension.
- A beat with a time in requires fires when its threshold is reached and its time has come, whichever is later. Use time only for when a beat can happen (at night, on market day), never for when it should happen ("the day after the Society arrives"): the threshold decides that.

Resolution
- Two to four resolution levers with different consequences for the world and the cast. {{leverRule}}
- For every resolution lever, list the deeds the players must carry out for it to take effect, besides knowing the facts (requiredDeeds): where (place, affordanceId), when (time: dawn, morning, midday, day, evening, dusk, night, sunday, weekday) and before whom (witnesses, with witnessRule any or all). A lever that only needs force has one deed (defeat or drive off the creature). A deep lever usually has two to four. Deed ids are the lever's id and a number: r3-d1, r3-d2. deedOrder is listed when the deeds must be done in order, otherwise any.
- Each lever lists the facts the players need to see it (requiresFactIds). Its consequences persist in the Society's world: what changes in the world, how each affected person's attitude toward the Society moves (attitudes, with a delta from -3 to +3 and why), and who dies or leaves the district (fates: dead or departed).
