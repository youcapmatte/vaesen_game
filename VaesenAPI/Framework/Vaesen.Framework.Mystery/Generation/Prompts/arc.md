# Step: arc

Shape how the case moves and how it can end.

Beats and climax
- Three to six beats at rising tension thresholds, between 0.1 and the climax threshold, each with what happens, who acts (actorId), where and when it requires, and the facts it puts into the world. Use them for the off-screen actions of the desperate and the creature's escalation.
- When a cast member acts in a beat or the climax, name the action (actionId) from their permitted actions. Desperation rises with tension: below {{mediumFrom}} only low acts, from {{mediumFrom}} medium ones, from {{highFrom}} high ones. A person who acts against their own plan (the wrong time, the wrong target, a higher risk) does so for a stated reason: desperation, a changed plan or a mistake.
- The climax comes at a threshold of about 0.8; {{climaxRule}}
- Beats and the climax put pressure on the players and on the guilty. They may show the hidden hand acting desperately, but they must not explain the case. The climax sets up the choice; it never delivers a confession or an explanation the players have not earned. A confession is a permitted action at high desperation, triggered when the players confront the person with the right facts, not a scripted event. Most of the deep lever's facts must come from channels the players seek out, not from beats.
- You only set the thresholds. The engine works out how much each fact raises the tension.
- A beat with a time in requires fires when its threshold is reached and its time has come, whichever is later. Use time only for when a beat can happen (at night, on market day), never for when it should happen ("the day after the Society arrives"): the threshold decides that.

Resolution
- Two to four resolution levers with different consequences for the world and the cast. {{leverRule}}
- For every resolution lever, list the deeds the players must carry out for it to take effect, besides knowing the facts (requiredDeeds): where (place, affordanceId), when (time: dawn, morning, midday, day, evening, dusk, night, sunday, weekday) who must do it (performedBy: cast ids, or players; rule any or all) and before whom (witnesses, with witnessRule any or all). A person who must act with their own hands is a performer, not a witness. A lever that only needs force has one deed (defeat or drive off the creature). A deep lever usually has two to four. Deed ids are the lever's id and a number: r3-d1, r3-d2. deedOrder is listed when the deeds must be done in order, otherwise any.
- Each lever lists the facts the players need to see it (requiresFactIds): at most eight, the ones the ending truly turns on, so a voice table can gather them. Its consequences persist in the Society's world: what changes in the world, how each affected person's attitude toward the Society moves (attitudes, with a delta from -3 to +3 and why), and who dies or leaves the district (fates: dead or departed).
