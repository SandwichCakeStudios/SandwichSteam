---
title: Cloud Saves
parent: Content & Saves
nav_order: 1
---

# Cloud Saves
{: .no_toc }

Save games that follow the player through the Steam Cloud. Every save is checked for damage, and when the PC's copy and the cloud copy differ, you decide which one wins.

> **Status:** tested with one account.

1. TOC
{:toc}

## Before you begin

- **Turn on Steam Cloud** for your App ID on the [Steamworks partner site](https://partner.steamgames.com/) and set a byte and file quota. Without it, cloud writes fail.
- **Use this or Steam Auto-Cloud for your save files, never both.** Auto-Cloud (also set up on the partner site) syncs files by folder. This feature writes its own files (`<Slot>.steamsave`). Two systems on the same files overwrite each other.
- No App Definition rows are needed.

## Save

1. Create your **Save Game** object as usual (*Create Save Game Object* with your Save Game class) and fill it.
2. **Save Game To Steam Cloud** with a *Slot* name and the save game.

<!-- SHOT 71: Blueprint: Create Save Game Object -> set variables -> Save Game To Steam Cloud (Slot "Profile1") -->

The result tells you where it landed: *Result*, *Saved Local* and *Saved Cloud*.

- The local copy is written first, safely: a crash never leaves half a file.
- **Cloud switched off** by the player (in Steam, for the account or the game): the save still succeeds, locally. *Saved Cloud* is false.
- **Cloud full:** the local copy is kept and the result fails with `Steam.Error.QuotaExceeded`.

Slot names: letters, digits, `_` and `-`, up to 100 characters.

## Load

**Load Game From Steam Cloud** with the *Slot*. It returns the **Save Game** (cast it to your class) and a result with an *Outcome*:

| Outcome | Meaning | What to do |
|---|---|---|
| **Loaded** | It worked. *From Cloud* says which copy was used | Cast and use the save game |
| **Not Found** | No save in this slot yet. Not an error | Start a new game |
| **Conflict** | Both copies are fine but different, and the policy is **Ask** | Let the player choose (below) |
| **Corrupt** | Both copies are damaged | Tell the player, start fresh |

A damaged copy is ignored and repaired from the good one. After a load, both copies are the same again.

<!-- SHOT 72: Blueprint: Load Game From Steam Cloud -> Switch on Outcome -> Loaded: Cast To MySaveGame -->

## When the two copies differ

This happens when the player played on two PCs, or offline. **Project Settings > Plugins > Sandwich Steam - Cloud > Conflict Policy** decides:

| Policy | Result |
|---|---|
| **Newest Wins** (default) | The copy saved last wins |
| **Prefer Local** | This PC's copy wins |
| **Prefer Cloud** | The cloud copy wins |
| **Ask** | The load returns **Conflict** and **On Cloud Conflict** fires with the save time and size of both copies |

With **Ask**: show both copies to the player, then call **Resolve Steam Cloud Conflict** with the *Slot* and *Use Cloud* (true = cloud copy, false = this PC's copy). It returns the chosen save game, like a load.

## Other nodes

| Node | What it does |
|---|---|
| **Delete Steam Cloud Slot** | Deletes both copies of a slot |
| **List Steam Cloud Slots** | Every slot in the cloud, with size and time. For a "Load game" menu |
| **Is Steam Cloud Enabled** | Cloud is on for this player and game |
| **Get Steam Cloud Quota** | Total and available bytes |

## Console commands

| Command | What it does |
|---|---|
| `Steam.Cloud.Dump` | Cloud state, quota and the slots |
| `Steam.Cloud.Roundtrip [Slot]` | Saves a test text and loads it back |
| `Steam.Cloud.Read [Slot]` | Loads a slot and logs the result |
| `Steam.Cloud.DeleteLocal [Slot]` | Deletes only the local copy. `Read` afterwards must bring it back from the cloud |
| `Steam.Cloud.Delete [Slot]` | Deletes both copies |

The test panel (`Steam.Test.Toggle`) has a **Cloud** page with the same steps.

## Good to know

- **Saving on quit is enough.** Saves are written right away and Steam uploads in the background, also after the game closed.
- Saves made with Unreal's own **Save Game To Slot** are separate files and are not slots of this feature.
- The local copy lives in `Saved/SaveGames/<Slot>.steamsave`.
- Saves are read and written in one piece. Keep them small; Steam's own limit is 100 MB per file.
- **Client only.**
