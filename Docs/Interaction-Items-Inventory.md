# Interaction, Items and Inventory in Vaesen

This describes how interaction, items, pickups, pouches and the inventory are built in the Vaesen project (Unreal Engine 5.5, C++ with Blueprint data), as of the branch `feature/interaction-outline`. It's written to be read aloud or talked through, so it explains in prose rather than code.

The game is multiplayer with a dedicated server. One rule runs through all of it: **the server decides, clients ask**. A player's machine shows things (cards, outlines, the inventory screen) and sends requests. The server checks every request, changes the game, and replication brings the result back to everyone.

---

## 1. The big picture

There are six layers, each built on the one before.

1. **Interaction.** The player looks at things. Something interactable shows a small card and a yellow outline, and the player can press or hold E to interact with it.
2. **Items.** An item is a plain data object (a UObject, not an actor) describing one kind of thing: lamp oil, bread, a revolver, a herb pouch. Kinds of item are Blueprint subclasses.
3. **Pickups.** A pickup is an actor in the world holding one item stack. Interacting with it picks it up.
4. **The inventory.** A component that can go on any actor: the player, a chest, a corpse. It's **slot based**: it holds item stacks, each taking some number of slots, up to a capacity (or unlimited). It handles stacking, moving, using and dropping.
5. **Equipment.** What each character wears and holds, one item per slot, each item only in its own slot, shown on the modular body.
6. **The inventory screen.** The player's survival-style UI for all of this: grids, drag and drop, tooltips, and looting containers side by side.

Pouches, such as a herb pouch, sit across layers 2 and 4. They're items that hold other items inside the inventory.

---

## 2. Interaction

### The player's side: the Interactor component

Every player character has a component called **VaesenInteractorComponent**. On the player's own machine, every frame, it looks for something to interact with.

It does that with a **sphere sweep**, a thick line trace with a 10 cm radius, so small things are easy to aim at. The sweep follows the camera's view direction. In third person the camera sits behind the character, so the sweep starts level with the character rather than at the camera. Nothing between the camera and the character counts. The sweep reaches **200 cm in front of the character**, the same in first and third person.

If the sweep hits an actor with an **interaction component**, and that component says this player may interact (`CanInteract`), it becomes the **focused** interactable. The interactor remembers it in a small struct (`FVaesenInteractionData`): what's being looked at, when the last check ran, and whether the interact key is held. If the sweep hits the same thing again, nothing new happens. If it hits nothing, or something else, the old focus ends.

### The object's side: the Interaction component

Anything interactable carries a **VaesenInteractionComponent**. It's a **widget component in screen space**, which means it draws a small UI card flat on the screen at its position in the world. The card is the **interaction card**, 200 by 150 pixels, showing:
- the name;
- the quantity (for stacks);
- a description;
- the action, like "[E] Pick up";
- a progress bar for interactions that take time.

The actor fills in that content. The component has these settings:
- **Interaction time.** Zero means instant. Otherwise the key must be held this long.
- **Interaction distance.** How close the player's eyes must be to the nearest edge of the actor, 200 cm by default.
- **Allow multiple interactors.** Off means only one player at a time.
- **Interaction enabled.** Off means nobody can focus or use it.

`CanInteract` checks all of these.

The component has five events (delegates) that the actor binds to:
- **OnBeginFocus** and **OnEndFocus.** The local player started or stopped looking at it. Only on that player's machine.
- **OnBeginInteract** and **OnEndInteract.** A player pressed the key, or let go, looked away or walked off. On that player's machine and on the server.
- **OnInteract.** The interaction actually happened: the key was held long enough, or at once if the time is zero. **Server only.** This is where the actor does its thing: give the item, open the door, open the chest.

The component also keeps a list of the **players currently interacting** with it. On the server that's everyone; on a client, only its own player. Players are added on begin-interact and removed on end-interact.

### The outline

When something is focused, its meshes are drawn into Unreal's **custom depth stencil buffer** with the value 1. A **post-process material**, `PP_InteractionOutline`, is attached to the player's camera. It looks at the stencil buffer and paints a **yellow outline** around anything marked with 1, plus a faint yellow tint over it. Only the looking player sees it. The project setting "custom depth with stencil" is turned on for this.

### Multiplayer flow for interacting

1. **Press:** the player presses E. Their machine starts the hold timer, which drives the card's progress bar, and sends a request to the server naming the actor they're looking at.
2. **Check:** the server finds the interaction component on that actor and checks `CanInteract` itself, so the client can't cheat on distance.
3. **Hold:** the server runs its own timer. While the hold lasts, it keeps checking that the player is still in reach.
4. **Complete:** when the server's timer completes, the server calls **Interact**, and OnInteract fires.
5. **Cancel:** letting go, looking away or walking off cancels it, on both sides.

### Input

The interact key is **E**, through an Enhanced Input action called `IA_Interact` in the default mapping context. Pressing calls BeginInteract and releasing calls EndInteract.

---

## 3. Items

### The base item

The base class is **VaesenItem**, a UObject. Each kind of item is a **Blueprint subclass** (for example `BP_Item_LampOil`) with its values set in Class Defaults. That class is the item's **template**. Actual items in the game are **instances** of it, each with its own quantity.

Every item has:
- **Description:**
  - name;
  - description;
  - **pickup mesh** (a static mesh, how it looks lying in the world);
  - **thumbnail** (the inventory icon);
  - **weight** in kilograms;
  - **value**;
  - **item tags** (Gameplay Tags such as `Item.Material.Iron`, `Item.Light`, `Item.Herb`);
  - **use action text**, such as "Eat", "Read" or "Empty". If it's empty, the item can't be used on its own.
- **Flags:**
  - **quest item** (can't be dropped or sold; ticking it also turns off Can Be Dropped);
  - **can be dropped**.
- **Stacking:**
  - **stackable** and **max stack size**. If stackable is off, the max stack size is greyed out and locked at 1. Ticking stackable back on restores 10.
- **Space:**
  - **slot size**: how many inventory slots one stack of it takes, however many are in the stack. Normally 1; a rifle might take 3. **Zero means it takes no space at all**, so any number can be carried: letters, coins, keys.

Weight is still on every item, but it's **information only**: the inventory is limited by slots, not weight.

### Stacking rules by kind

Only **consumables, ingredients and ammo** stack by default, **10 per stack**. Everything else is one per stack by default: plain items, equippables, readables, pouches. Any item Blueprint can override this.

### Item functions

- **Get quantity** and **set quantity.** Setting is clamped between 1 and the max stack, and is done by the server.
- **Get stack weight.** Weight times quantity. For a pouch, it's the pouch's own weight plus everything inside.
- **Can use** and **use.** Use runs on the server, and subclasses or Blueprints decide what it does.
- **Get owning actor.** The actor holding the item, which is its outer object.
- **Get owning inventory.** The inventory it's in, if any.
- **A world.** Items report their owning actor's world, so item Blueprints can use world nodes like timers, spawning and getting the player.
- **DuplicateItem.** Makes a new copy of an item for a new owner with a given quantity, copying anything inside it too (that's how pouches keep their contents). It also works from the class defaults, to make a fresh item of a kind.
- **Nested items.** An item can report items held inside it. A pouch reports its contents, so they get replicated along with it.

When the quantity changes, the item fires **OnItemModified** and tells its inventory, so the UI updates. This also happens on clients when the new quantity arrives from the server.

### Replication of items

Items are UObjects, which Unreal doesn't replicate on their own. Each item is created with the **actor that holds it as its outer**: the pickup, the player, the chest. It's registered as a **replicated subobject** of that actor, using the modern UE 5 registered subobject list. The quantity is a replicated property. Items inside pouches are registered the same way. Moving an item to another actor creates a copy there and removes the original.

### The kinds of item

Kinds that do something different are subclasses; variants are just settings and tags. The tree is kept shallow on purpose.

- **Equippable.** Goes into exactly one equipment slot, its own (see section 7, Equipment). Right-clicking it in the inventory equips it ("Equip").
  - **Equip and Unequip:** handle what's attached to the character, so the equipment component runs them on every machine.
  - **Equipped mesh:** an optional static mesh attached to a bone while equipped, like a revolver in the right hand.
  - **Kinds of equippable:**
    - **Weapon:** main hand; melee, firearm or thrown; damage; range; two-handed (which also takes the off hand). Ammo type and magazine size only show for firearms.
    - **Armor:** this covers clothes too. Its slot is an armor slot: Head, Chest, Hands, Legs, Feet, Back or Full body. Its **fit** says how it sits on the body part: it either **replaces the body part** (a closed plate helm, full gloves) or is **worn over it** (a hat, a hood, a scarf, an open helmet), leaving the body part visible underneath. A cloak on the back is always worn over; full-body armor always replaces. The armor mesh must use the mannequin skeleton.
    - **Tool:** lantern, spyglass, compass, lockpicks. Off hand by default, and an optional item it uses up, such as lamp oil for a lantern, with how long each lasts.
    - **Talisman:** a charm against vaesen, in the talisman slot, with the tags of what it wards against and a protection value.
    - **Container:** a satchel or backpack in the bag slot, with extra slots and carry weight. Wearing one doesn't add inventory slots yet.
- **Consumable:** food, drink, medicine or poison, with a list of effects (a gameplay tag, a strength and a duration) and "consumed on use". Using one takes one from the stack.
- **Ammo:** cartridges, powder and shot, with a damage bonus.
- **Readable:** letters, journals, books, clues. Pages of text, an optional illustration and a clue flag. "Read".
- **Ingredient:** herbs and materials for remedies and crafting, told apart by tags.
- **Pouch:** described in section 6.

### Gameplay tags in use

- **Item tags:** `Item.Material.Iron`, `Item.Material.Silver`, `Item.Material.Salt`, `Item.Light` and `Item.Herb`.
- **Effect tags:** `Effect.Heal`, `Effect.Calm` (mental harm), `Effect.Nourish` and `Effect.Poison`.

They're defined in `Config/DefaultGameplayTags.ini` and can be added to in the editor.

### Where items live in the Content Browser

Item Blueprints go under `Vaesen/Blueprints/Items`, grouped by category in subfolders such as Food, Herbs, Light and Tools. They're named `BP_Item_` plus the name. Meshes and textures stay where they came from, such as an asset pack under `ThirdParty`; the item only points to them.

---

## 4. Pickups

A **VaesenPickup** is an actor with:
- a **static mesh** (its root, which blocks the interaction trace);
- an **interaction component** saying "Pick up", instant.

You place one in a level and choose an **Item Template** and a **Quantity**. In the editor, the mesh changes immediately to the item's pickup mesh, and the card shows the item's name and description. The quantity is kept within the item's stack size.

When play starts, the **server** creates the actual item from the template and replicates it with the pickup, including anything inside it.

Picking up happens on the server, in OnInteract. The pickup tries to add its item to the player's inventory:
- **All of it fits:** the pickup is destroyed.
- **Some fits:** the pickup keeps the rest and its card updates.
- **Nothing fits:** nothing happens, and the reason ("Inventory full") goes to the log for now.

**Dropped items fall.** When something is dropped, the pickup is created at hip height a little in front of the character (pulled back if that would be inside a wall) and given a gentle toss forward and up. From there the **physics engine** takes over: it falls, lands on whatever is below, and if that's a chair or a table edge it tumbles off onto the floor, coming to rest lying naturally on the surface. As soon as it stops moving, or after eight seconds at most, physics switches off so it costs nothing. The server simulates the fall and its movement replicates, so everyone sees it land in the same place. This needs the item's mesh to have collision; a mesh without it is simply put on the ground below. Dropped items are shown at the item's **pickup scale**, so a small object doesn't appear as big as its mesh's default size. A pickup's interaction card is placed just above wherever it ended up. Pickups don't block characters or the camera, so things on the floor never get in anyone's way.

There are two ways to fill a pickup from code, both server only:
- **Initialize pickup:** from a class and a quantity.
- **Initialize pickup from item:** from an existing item, copying it exactly. This is what dropping uses, so a dropped pouch keeps its contents.

---

## 5. The inventory component

### What it is

**VaesenInventoryComponent** can be added to **any actor**: the player, a chest, a corpse, a cart.

It's **slot based**. Every stack takes its item's **slot size** in slots, no matter how many items are in the stack; items with slot size zero take none. The stacks together may take up to the inventory's **capacity**. A capacity of **minus one means unlimited**, which is the default for chests.

Its settings:
- a **display name**;
- a **capacity** in slots, or minus one for unlimited;
- **accessible by others**: whether other players may use it, within an **access distance** of 3.5 m;
- **starting items**, added on the server when play starts;
- the **pickup class** used when items are dropped.

It keeps its stacks in a list of **positions**, so the player can arrange them. A stack that's moved away or used up leaves a **gap**, which the next new stack fills; gaps at the end are dropped. Positions aren't capacity: capacity is counted in slot sizes. The position list is replicated, and the items in it are replicated subobjects of the owning actor. Everyone receives every inventory's contents for now.

### Reading

You can:
- get all slots;
- get the item in a slot;
- find which slot an item is in;
- get the number of **used slots** (the slot sizes added up), the number of **free slots** (minus one when unlimited), and the total weight (information only);
- ask whether a new stack of some item **has room**;
- count how many of a kind of item it holds, pouches included;
- check whether it has a certain amount of an item.

**Can be accessed by** tells whether a given player may use it: yes if they own it, otherwise only if others are allowed and the player is within the access distance.

### Adding

**Try add item** (from an existing item) and **try add item of class** (from a class and quantity) run on the server. They return a result: how many were meant to go in, how many did, whether it was none, some or all, and an error text if not everything fit.

The order of filling is:
1. **Pouches:** into any pouch that accepts the item, topping up its stacks first and then new stacks while the pouch has room.
2. **Existing stacks:** topping up stacks of the same item in the inventory.
3. **New stacks:** new stacks, each up to the stack size, as long as the inventory has room for the item's slot size. Items with slot size zero always fit, and so does anything in an unlimited inventory.

When it runs out of room, the error is "Inventory full".

### Removing

**Remove item** takes some or all of one stack, wherever it is, including inside a pouch. **Remove items of class** takes a total amount of a kind across stacks, starting with pouch contents. That's meant for crafting and quests later.

### Moving

**Move item** moves an amount, or the whole stack, from one slot to a slot in the same or another inventory:
- **Onto an empty position:** it moves there (rearranging never needs room), or splits off the amount as a new stack, which needs room for its slot size. Positions just past the last stack count as empty.
- **Onto the same kind of item:** it merges as far as the stack allows.
- **Onto a pouch that accepts it:** it goes inside the pouch.
- **Onto a different item:** whole stacks swap places. Between two inventories, each side must have room once it gives up the other stack's slots.
- **To "anywhere":** it goes wherever it fits in the target, using the same filling order as adding.

Between inventories, the target's capacity is respected.

### Using and dropping

**Use item** calls the item's Use for the player. A consumable that's consumed on use then loses one from its stack.

**Drop item** spawns a falling pickup in front of the player with a copy of the item (see Pickups), and removes it from the inventory. Items that can't be dropped stay. The **drop sound** (Project Settings → Vaesen → Audio → Items) is played to the player who drops something.

### How players change inventories

A player's machine never changes an inventory directly. Everything goes through **the player's own inventory component**:
- **request move**;
- **request use**;
- **request drop**.

These are server RPCs. The player's own component is the one their connection owns, which is what allows the RPC.

The server checks that the player may access both inventories involved, then does the change. That's how a player can take things out of a chest: the request goes through their own inventory and names the chest's inventory.

### Opening a container

A chest calls **open container for owner** on the searching player's inventory, on the server. That sends a client RPC to that player, whose controller opens the inventory screen with the container beside it.

### The loot container

**VaesenLootContainer** is a ready-made chest actor:
- a mesh;
- an interaction component saying "Search", with a half-second hold;
- an inventory with **unlimited** capacity, accessible to anyone nearby.

When searched, it opens its inventory for that player. Any other actor can do the same: add an inventory component and an interaction component whose OnInteract opens the inventory for the player.

### The player's inventory

The player character has an inventory of **24 slots**, called "Inventory", which only they can open.

---

## 6. Pouches

A **VaesenPouchItem** is an item that holds other items in **slots of its own**, like a herb pouch, a sack or a cartridge box. In the inventory it takes its own slot size, normally one slot.

What it accepts is set by **accepted item tags** (for example `Item.Herb`) and **accepted classes**:
- An item is accepted if it has any of the tags or is any of the classes.
- If neither is set, the pouch accepts anything, which makes it a plain sack.
- A pouch never accepts another pouch.
- **Pouch slots** is its capacity inside, counted the same way as an inventory: each stack takes its item's slot size, and items with slot size zero always fit. Each stack goes up to its item's stack size. The test herb pouch has four slots, so with one-slot herbs that's forty herbs.

The behaviour:
- **Picking things up:** whenever something is added to an inventory, it **goes into a pouch that accepts it first**. So as long as the herb pouch has room, picked herbs don't take extra inventory slots. Only when the pouch is full do they start taking slots.
- **Dropping onto a pouch:** in the inventory screen, dropping an item onto a pouch puts it inside, if the pouch accepts it.
- **Emptying:** right-clicking a pouch, which is its "use" action, labelled "Empty", tips its contents out into the inventory's slots, as far as they fit. Whatever doesn't fit stays in the pouch.
- **Weight:** the pouch's weight is its own plus its contents' (information only, like all weight).
- **Counting and removing:** counting items of a kind, and removing items of a kind, look inside pouches.
- **Dropping the pouch:** a dropped pouch keeps its contents. The pickup holds a copy of the pouch with copies of everything inside, and picking it up again brings it all back.

Under the hood:
- **Contents:** the pouch keeps a replicated list of the stacks inside.
- **Changes:** only the inventory changes the contents, on the server. It registers each item inside as a replicated subobject of the owning actor, so clients see the contents.
- **Copies:** copying a pouch (DuplicateItem) copies each item inside for the new owner, so two pouches never share items.

In the UI:
- the pouch's cell shows how many of its slots are used, like "2/4", instead of a stack count;
- its tooltip lists what's inside, one line per stack, with the total weight.

---

## 7. Equipment

### Slots

What a character wears and holds is kept by the **equipment component** (`VaesenEquipmentComponent`), which every character has, players and NPCs alike. It has one place per **equipment slot**, and an item **only ever goes into its own slot**: boots into Feet, a coat into Chest.

The armor slots follow The Witcher 3, plus a head and a back:
- **Head:** hats, hoods, helmets.
- **Chest:** coats, shirts and armour for the body. **This covers the arms too.**
- **Hands:** gloves.
- **Legs:** trousers, skirts.
- **Feet:** boots and shoes.
- **Back:** a cloak, worn over the body rather than replacing a part.
- **Full body:** one suit for the whole body. **NPCs only**; players always dress piece by piece.

The other slots:
- **Main hand** and **off hand:** weapons and tools. A two-handed weapon takes both hands; putting something in the off hand takes a two-handed weapon off.
- **Talisman:** a charm.
- **Bag:** a satchel or backpack.

### The body

The character's modular body matches the armor slots: the mannequin is split into **head, torso (with the arms), hands, legs and feet**, all following the torso's animation. Each of those parts also has an empty **worn-over layer** on top, for armor worn over it, and there's an empty **back** layer that shows a cloak.

The equipment component sets the look on every machine:
- each body part shows the character's own mesh, unless armor that **replaces** it is worn there, in which case it shows the armor;
- armor **worn over** a part shows on that part's worn-over layer, with the part itself still drawn underneath;
- a cloak shows on the back;
- full-body armor replaces the torso with its mesh and hides the head, hands, legs and feet and their worn-over layers (a cloak stays on);
- weapons, tools, talismans and bags attach their equipped meshes.

In first person the player's own head, and anything worn over it, is hidden from their camera but still casts its shadow.

### Sounds

There's an **equip sound**, played to the player when something goes on, and an **equip rejected sound**, played when something can't: dragging an item onto the wrong slot, or the server refusing because there's no room for what it would replace. Each equippable item can have its own of both, so chainmail can rattle while a hat makes no sound; otherwise the **defaults in Project Settings → Vaesen → Audio → Items** are used. Only the player themself hears these.

### Equipping and taking off

Everything happens on the server, and players ask through **request** functions on their own equipment component, which the server checks:
- **Equip from an inventory:** the item leaves the inventory and goes on. Whatever was in its slot (and the other hand, for two-handed weapons) goes back into that inventory, and only if it all fits; otherwise nothing happens.
- **Take off:** the item goes into an inventory, the character's own unless another is named, if there's room.
- **Drop:** a worn item can be dropped straight to the ground, as a falling pickup.
- **Equip new:** puts on a fresh item of a kind, from nowhere. This is for NPC outfits; the component also has a list of **starting equipment** put on when play starts.

Whether full-body armor is allowed is a setting on the component; it's off on players.

## 8. Quick slots

Potions, food and other things you use often can't go in equipment (they aren't worn), so they have **quick slots**: shortcuts, like the potion slots in The Witcher 3.

- **What a slot holds:** a **kind** of item, not the item itself. The items stay in the inventory (pouches included); the slot shows how many you have in total.
- **Using:** keys **1 to 8**, or clicking the slot in the inventory screen, use one of them, from wherever in the inventory it is. When you run out, the slot stays assigned but greys out, and fills again when you pick more up.
- **What fits:** anything usable that isn't worn: potions, food, remedies, letters. Not clothing, weapons or tools (they go in equipment), and not pouches.
- **How many:** four to start with. A belt or bandolier worn in the **Bag** slot can add more (its **extra quick slots**), up to eight.
- **The quick bar:** the slots are shown at the bottom of the screen during play, and in the inventory screen under the panels. There you **drag an item onto a slot** to put that kind there (a kind of item is in one slot at most), and **right-click** a slot to clear it.

The quick slot component keeps the assignments on the server; only the owning player receives them. A radial menu (hold a key and pick with the mouse) could be added later on top of the same slots.

## 9. The inventory screen

### Opening and closing

**I** opens the inventory screen, through an input action called `IA_Inventory`. The player controller creates it from the widget set in Project Settings → Vaesen → Inventory Screen Widget.

While it's open:
- the mouse cursor is free;
- the view doesn't turn;
- the player can still walk.

**I or Escape** close it.

The screen shows the player's **equipment** on the left, as a paper doll around a live picture of their character, and their **inventory** beside it. When a chest is opened, the chest's inventory appears too, on the right. If the player walks out of reach, or the chest disappears, the chest side closes by itself.

### Building blocks

The screen is made of seven widget classes, each a C++ class with a Widget Blueprint made from it. Each Blueprint only needs controls with specific names; the C++ does the work.
- **Inventory screen** (`WBP_InventoryScreen`): the full-screen root, lightly darkened, with the equipment panel, the player's panel, the container's panel and a line of hints. Dropping an inventory item anywhere outside the panels drops it on the ground.
- **Equipment panel** (`WBP_EquipmentPanel`): a paper doll like The Witcher 3's. In the middle, in a dark frame, is a **live picture of the player's character** in an idle pose, wearing what's equipped. **Dragging on it turns the character.** The armor slots run down the left (head, chest, hands, legs, feet) and the others down the right (back, main hand, off hand, talisman, bag). Dropping an item on the character or the panel puts it on in its own slot.
  - The picture comes from a **character preview** (`VaesenCharacterPreview`): a copy of the character's body, every part and worn-over layer plus attached weapons and tools, standing in a small studio of its own far above the map, with a warm key light, a cool fill light and a rim light, filmed by a camera onto a texture. It's created only on the player's own machine while the inventory is open, and removed when it closes. Its meshes and lights use their own lighting channel, so the level's sun, sky and fog don't touch it. It updates whenever something is put on or taken off. A Blueprint of it can tune the lights, framing and exposure.
- **Drop box** (`WBP_DropZone`): a box labelled "Drop" under the player's inventory, as in The Witcher 3. Drag an item onto it, from the inventory or off the body, to drop it on the ground. It plays the drop sound.
- **Equipment slot** (`WBP_EquipmentSlot`): one slot, with its name in the corner and the worn item's thumbnail or name. It lights up when hovered, and turns red while dragging something that doesn't belong in it.
- **Inventory panel** (`WBP_InventoryPanel`): one inventory as a grid, six columns wide.
  - **Cells:** one cell per stack, then as many empty cells as there are free slots. An unlimited inventory always shows a spare empty row.
  - **Header:** the inventory's name and its used and total slots, or "used / unlimited".
  - **Weight:** the total weight in kilograms, for information.
  - **Drops:** dropping an item on the panel itself, rather than on a cell, puts it wherever it fits.
- **Inventory slot** (`WBP_InventorySlot`): one 64-pixel slot.
  - **Content:** the item's thumbnail, or its name if it has no thumbnail, and its quantity at the bottom right (pouches show used/total).
  - **Hover:** it lights up and shows the tooltip.
- **Item tooltip** (`WBP_ItemTooltip`):
  - name;
  - category (weapon, food, pouch…);
  - description;
  - stats: weight (and the stack's weight), how many slots it takes when that isn't one ("Takes no space" for zero), value, stack size, what right-click does, and a pouch's contents.

### Controls

- **Drag** a slot onto another slot: move, merge, swap, or put inside a pouch.
- **Ctrl and drag:** take half the stack.
- **Shift and click:** send the item straight to the other open inventory, chest to player or player to chest.
- **Right-click:** use the item. Eat it, read it, empty a pouch, or put on clothing and weapons.
- **Drag onto an equipment slot:** put it on, if it belongs in that slot.
- **Drag off an equipment slot** onto an inventory, or **right-click** it: take it off.
- **Drag onto the Drop box, or outside the panels:** drop it on the ground.

Every one of these is a request to the server through the player's own inventory or equipment, so the screen just shows what the server says.

---

## 10. Test assets

These were created by the project's asset-building commandlet.

**`BP_Pickup`** (in `Vaesen/Blueprints/Interaction`) is the one pickup for every item. Place it in the level, select it, and choose its **Item Template** and **Quantity** in the Details panel: the mesh changes to the item's pickup mesh straight away, the pickup takes the item's pickup size (resize it freely afterwards), and the card shows the item. An item without a mesh, or no item at all, leaves the pickup without a mesh.

Items to choose, in `Vaesen/Blueprints/Items`:
- `BP_Item_LampOil`: lamp oil, a plain item that stacks to ten.
- `BP_Item_HerbPouch`: a herb pouch with four slots that accepts `Item.Herb`.
- `Herbs/BP_Item_Yarrow`: yarrow, a herb tagged `Item.Herb`.
- `Clothing/BP_Item_LeatherBoots`, `BP_Item_WorkGloves` and `BP_Item_WoolCloak`: clothing for the feet, hands and back. They have no meshes of their own yet, so the body doesn't change when they're worn, but they fill their equipment slots.
- `Food/BP_Item_RyeBread`: rye bread, a consumable ("Eat") to try the quick slots with.

Also in `Vaesen/Blueprints/Interaction`:
- `BP_LootChest`: a crate with unlimited space and seven lamp oil inside. Hold E to search it.
- `BP_InteractableTest`: the earlier test crate, which just logs when interacted with.

A good test: place pickups of the herb pouch, yarrow, boots and bread. Pick up the herb pouch, then the yarrow. Open the inventory. The yarrow should be inside the pouch, not in a slot of its own; the pouch shows "1/4" and its tooltip lists the yarrow. Right-click the pouch to empty it. Then pick up the boots and right-click them: they go into the Feet slot. Try dragging the gloves onto the Head slot; it turns red and refuses.

---

## 11. Where to find things

C++ lives in `VaesenGame/Source/VaesenGame`:
- **`Interaction/`:** the interaction component and the interactor component.
- **`Items/`:**
  - the base item;
  - every kind of item;
  - the pouch;
  - the item enums and effect struct;
  - the pickup.
- **`Inventory/`:** the inventory component, the equipment component, the quick slot component and the loot container.
- **`UI/Inventory/` and `UI/Interaction/`:** the widget classes.
- **The characters:** every character has the modular body and the equipment; the player character adds the interactor and the inventory, and binds E (interact) and I (inventory).
- **The player controller:** opens and closes the inventory screen.

The **commandlet** in the editor module (`VaesenCreateWidgetsCommandlet`) builds the widget Blueprints, input actions and test assets. It only creates what's missing, so running it again is safe.

---

## 12. Not done yet

- **Clothing meshes.** The slots and the swapping work, but there are no clothing meshes yet; the test clothing keeps the plain body.
- **Bags adding slots.** A worn bag doesn't enlarge the inventory yet.
- **Armor doing anything.** Protection is data only, like consumable effects.
- **A hotbar** for quick use.
- **Feedback to the player** when something doesn't fit. Today it only goes to the log.
- **Big items drawn bigger.** An item taking three slots is still one cell in the grid; the panel just shows fewer free cells.
- **Encumbrance.** Weight is shown but limits nothing; if it should slow the player down, that's still to come.
- **Who receives which inventory.** Every client gets every inventory's contents. A player's own inventory should later go only to that player.
- **Consumable effects.** Effects are data only; nothing applies them yet. That needs a health and condition system.
- **Opening a pouch as its own panel**, to drag single items out of it. Today you fill it by dropping onto it and empty it with right-click.
- **Thumbnails** for the test items. They show their names until icons are set.
- **Saving inventories** between sessions, through the API.
