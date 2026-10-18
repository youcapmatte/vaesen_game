# Interaction, Items and Inventory in Vaesen

This describes how interaction, items, pickups, pouches and the inventory are built in the Vaesen project (Unreal Engine 5.5, C++ with Blueprint data), as of the branch `feature/interaction-outline`. It's written to be read aloud or talked through, so it explains in prose rather than code.

The game is multiplayer with a dedicated server. One rule runs through all of it: **the server decides, clients ask**. A player's machine shows things (cards, outlines, the inventory screen) and sends requests. The server checks every request, changes the game, and replication brings the result back to everyone.

---

## 1. The big picture

There are five layers, each built on the one before.

1. **Interaction.** The player looks at things. Something interactable shows a small card and a yellow outline, and the player can press or hold E to interact with it.
2. **Items.** An item is a plain data object (a UObject, not an actor) describing one kind of thing: lamp oil, bread, a revolver, a herb pouch. Kinds of item are Blueprint subclasses.
3. **Pickups.** A pickup is an actor in the world holding one item stack. Interacting with it picks it up.
4. **The inventory.** A component that can go on any actor: the player, a chest, a corpse. It holds item stacks in slots, with stacking, weight, moving, using and dropping.
5. **The inventory screen.** The player's survival-style UI for all of this: grids, drag and drop, tooltips, and looting containers side by side.

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

- **Equippable.** Goes into a slot: Head, Torso, Arms, Legs, Feet, Main hand, Off hand, Back or Talisman.
  - **Equip and Unequip:** handle how it looks on the character, so they're meant to run on every machine.
  - **Equipped mesh:** an optional static mesh attached to a bone while equipped, like a revolver in the right hand.
  - **Kinds of equippable:**
    - **Weapon:** melee, firearm or thrown; damage; range; two-handed. Ammo type and magazine size only show for firearms.
    - **Armor:** this covers clothes too. Its slot is one of the five body parts. While worn, it **replaces that body part's mesh** on the modular character with its own skeletal mesh, and unequipping restores the previous one. The armor mesh must use the mannequin skeleton.
    - **Tool:** lantern, spyglass, compass, lockpicks. Off hand by default, "Use" action, and an optional item it uses up, such as lamp oil for a lantern, with how long each lasts.
    - **Talisman:** a charm against vaesen, with the tags of what it wards against and a protection value.
    - **Container:** a satchel or backpack worn on the back, giving extra slots and carry weight. This isn't wired into the inventory yet.
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
- **Nothing fits:** nothing happens, and the reason ("Inventory full" or "Too heavy") goes to the log for now.

There are two ways to fill a pickup from code, both server only:
- **Initialize pickup:** from a class and a quantity.
- **Initialize pickup from item:** from an existing item, copying it exactly. This is what dropping uses, so a dropped pouch keeps its contents.

---

## 5. The inventory component

### What it is

**VaesenInventoryComponent** can be added to **any actor**: the player, a chest, a corpse, a cart.

Its settings:
- a **display name**;
- a **capacity** in slots;
- a **weight capacity** in kilograms (zero means no limit);
- **accessible by others**: whether other players may use it, within an **access distance** of 3.5 m;
- **starting items**, added on the server when play starts;
- the **pickup class** used when items are dropped.

It holds an array of **slots**, one entry per slot, each either an item stack or empty. The slot array is replicated, and the items in it are replicated subobjects of the owning actor. Everyone receives every inventory's contents for now.

### Reading

You can:
- get all slots;
- get the item in a slot;
- find which slot an item is in;
- get the current weight and the number of used slots;
- count how many of a kind of item it holds, pouches included;
- check whether it has a certain amount of an item.

**Can be accessed by** tells whether a given player may use it: yes if they own it, otherwise only if others are allowed and the player is within the access distance.

### Adding

**Try add item** (from an existing item) and **try add item of class** (from a class and quantity) run on the server. They return a result: how many were meant to go in, how many did, whether it was none, some or all, and an error text if not everything fit.

The order of filling is:
1. **Weight:** how many the weight limit allows. The weight per item includes anything inside it, so a full pouch weighs more than an empty one.
2. **Pouches:** into any pouch that accepts the item, topping up its stacks first and then its empty pouch slots.
3. **Existing stacks:** topping up stacks of the same item in the inventory's own slots.
4. **Empty slots:** new stacks in empty slots, each up to the stack size.

### Removing

**Remove item** takes some or all of one stack, wherever it is, including inside a pouch. **Remove items of class** takes a total amount of a kind across stacks, starting with pouch contents. That's meant for crafting and quests later.

### Moving

**Move item** moves an amount, or the whole stack, from one slot to a slot in the same or another inventory:
- **Onto an empty slot:** it moves there, or splits off the amount.
- **Onto the same kind of item:** it merges as far as the stack allows.
- **Onto a pouch that accepts it:** it goes inside the pouch.
- **Onto a different item:** whole stacks swap places, if both sides can take the weight.
- **To "anywhere":** it goes wherever it fits in the target, using the same filling order as adding.

Between inventories, the target's weight limit is respected.

### Using and dropping

**Use item** calls the item's Use for the player. A consumable that's consumed on use then loses one from its stack.

**Drop item** spawns a pickup in front of the player, on the ground, with a copy of the item, and removes it from the inventory. Items that can't be dropped stay.

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
- an inventory of 12 slots, accessible to anyone nearby.

When searched, it opens its inventory for that player. Any other actor can do the same: add an inventory component and an interaction component whose OnInteract opens the inventory for the player.

### The player's inventory

The player character has an inventory of **24 slots and 40 kg**, called "Inventory", which only they can open.

---

## 6. Pouches

A **VaesenPouchItem** is an item that holds other items in **slots of its own**, like a herb pouch, a sack or a cartridge box. It takes **one inventory slot**.

What it accepts is set by **accepted item tags** (for example `Item.Herb`) and **accepted classes**:
- An item is accepted if it has any of the tags or is any of the classes.
- If neither is set, the pouch accepts anything, which makes it a plain sack.
- A pouch never accepts another pouch.
- **Pouch slots** says how many stacks fit inside; each stack goes up to its item's stack size. The test herb pouch has four slots, so forty herbs.

The behaviour:
- **Picking things up:** whenever something is added to an inventory, it **goes into a pouch that accepts it first**. So as long as the herb pouch has room, picked herbs don't take extra inventory slots. Only when the pouch is full do they start taking slots.
- **Dropping onto a pouch:** in the inventory screen, dropping an item onto a pouch puts it inside, if the pouch accepts it.
- **Emptying:** right-clicking a pouch, which is its "use" action, labelled "Empty", tips its contents out into the inventory's slots, as far as they fit. Whatever doesn't fit stays in the pouch.
- **Weight:** the pouch's weight is its own plus its contents'.
- **Counting and removing:** counting items of a kind, and removing items of a kind, look inside pouches.
- **Dropping the pouch:** a dropped pouch keeps its contents. The pickup holds a copy of the pouch with copies of everything inside, and picking it up again brings it all back.

Under the hood:
- **Contents:** the pouch keeps a replicated array of its contents, one entry per pouch slot.
- **Changes:** only the inventory changes the contents, on the server. It registers each item inside as a replicated subobject of the owning actor, so clients see the contents.
- **Copies:** copying a pouch (DuplicateItem) copies each item inside for the new owner, so two pouches never share items.

In the UI:
- the pouch's slot shows how full it is, like "2/4", instead of a stack count;
- its tooltip lists what's inside, one line per stack, with the total weight.

---

## 7. The inventory screen

### Opening and closing

**Tab** opens the inventory screen, through an input action called `IA_Inventory`. The player controller creates it from the widget set in Project Settings → Vaesen → Inventory Screen Widget.

While it's open:
- the mouse cursor is free;
- the view doesn't turn;
- the player can still walk.

**Tab, I or Escape** close it.

When a chest is opened, the same screen opens with the chest's inventory beside the player's. If the player walks out of reach, or the chest disappears, the chest side closes by itself.

### Building blocks

The screen is made of four widget classes, each a C++ class with a Widget Blueprint made from it. Each Blueprint only needs controls with specific names; the C++ does the work.
- **Inventory screen** (`WBP_InventoryScreen`): the full-screen root, lightly darkened, with the player's panel, the container's panel and a line of hints. Dropping an item anywhere outside the panels drops it on the ground.
- **Inventory panel** (`WBP_InventoryPanel`): one inventory as a grid, six columns wide.
  - **Header:** the inventory's name and its used/total slots.
  - **Weight:** a weight bar and "12.5 / 40 kg", hidden when there's no weight limit.
  - **Drops:** dropping an item on the panel itself, rather than on a slot, puts it wherever it fits.
- **Inventory slot** (`WBP_InventorySlot`): one 64-pixel slot.
  - **Content:** the item's thumbnail, or its name if it has no thumbnail, and its quantity at the bottom right (pouches show used/total).
  - **Hover:** it lights up and shows the tooltip.
- **Item tooltip** (`WBP_ItemTooltip`):
  - name;
  - category (weapon, food, pouch…);
  - description;
  - stats: weight (and the stack's weight), value, stack size, what right-click does, and a pouch's contents.

### Controls

- **Drag** a slot onto another slot: move, merge, swap, or put inside a pouch.
- **Ctrl and drag:** take half the stack.
- **Shift and click:** send the item straight to the other open inventory, chest to player or player to chest.
- **Right-click:** use the item. Eat it, read it, or for a pouch, empty it.
- **Drag outside the panels:** drop it on the ground.

Every one of these is a request to the server through the player's own inventory, so the screen just shows what the server says.

---

## 8. Test assets

These were created by the project's asset-building commandlet and are ready to drop into a level:
- `BP_Pickup_LampOil`: five lamp oil. Lamp oil is a plain item that stacks to ten.
- `BP_LootChest`: a crate with seven lamp oil inside. Hold E to search it.
- `BP_Pickup_HerbPouch`: a herb pouch with four slots that accepts `Item.Herb`.
- `BP_Pickup_Yarrow`: three yarrow, a herb tagged `Item.Herb`.
- `BP_InteractableTest`: the earlier test crate, which just logs when interacted with.

A good test: pick up the herb pouch, then the yarrow. Open the inventory. The yarrow should be inside the pouch, not in a slot of its own; the pouch shows "1/4" and its tooltip lists the yarrow. Right-click the pouch to empty it.

---

## 9. Where to find things

C++ lives in `VaesenGame/Source/VaesenGame`:
- **`Interaction/`:** the interaction component and the interactor component.
- **`Items/`:**
  - the base item;
  - every kind of item;
  - the pouch;
  - the item enums and effect struct;
  - the pickup.
- **`Inventory/`:** the inventory component and the loot container.
- **`UI/Inventory/` and `UI/Interaction/`:** the widget classes.
- **The player character:** has the interactor and the inventory, and binds E and Tab.
- **The player controller:** opens and closes the inventory screen.

The **commandlet** in the editor module (`VaesenCreateWidgetsCommandlet`) builds the widget Blueprints, input actions and test assets. It only creates what's missing, so running it again is safe.

---

## 10. Not done yet

- **Equipment slots on the player.** Equip and unequip exist on items, but there's no equipment component or UI yet, and wearing a bag doesn't add slots yet.
- **A hotbar** for quick use.
- **Feedback to the player** when something doesn't fit. Today it only goes to the log.
- **Who receives which inventory.** Every client gets every inventory's contents. A player's own inventory should later go only to that player.
- **Consumable effects.** Effects are data only; nothing applies them yet. That needs a health and condition system.
- **Opening a pouch as its own panel**, to drag single items out of it. Today you fill it by dropping onto it and empty it with right-click.
- **Thumbnails** for the test items. They show their names until icons are set.
- **Saving inventories** between sessions, through the API.
