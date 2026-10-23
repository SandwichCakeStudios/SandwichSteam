---
title: DLC
parent: Content & Saves
nav_order: 2
---

# DLC
{: .no_toc }

Check whether the player owns a DLC and has it installed, install or remove it, show download progress, and open its store page.

> **Status:** works with the test app. **Not yet tested** with a real DLC App ID.

1. TOC
{:toc}

## Before you begin

- **Your DLC exists in Steamworks** with its own App ID. Spacewar (480) has no DLC, so you need your own App ID to test.
- The [Quick Start](quick-start.html) is done: App ID set, an App Definition assigned.

## Add the DLC to the App Definition

1. Add a tag per DLC under `Steam.DLC`, for example `Steam.DLC.SoundtrackPack`.
2. Add a row to **DLC** in your App Definition:

| Field | What it does |
|---|---|
| **App Id** | The DLC's App ID from Steamworks |
| **Tag** | The tag you pick in the nodes |
| **Editor Name** | Only for editor lists. Players see Steam's name |

The App Definition reports missing or duplicate App IDs and duplicate tags.

## Owned and installed

| Node | True when |
|---|---|
| **Is Steam DLC Owned** | The player bought (or was given) the DLC |
| **Is Steam DLC Installed** | The DLC's content is downloaded |

Check **both** before you load DLC content: a player can own a DLC that is not downloaded yet.

<!-- SHOT 73: Blueprint: Is Steam DLC Owned (Steam.DLC.SoundtrackPack) AND Is Steam DLC Installed -> Branch -> unlock the content -->

Steam answers from memory, so the checks are cheap and always current. Still, check when it matters (menu opens, level loads), not every frame.

## Install, remove and progress

| Node | What it does |
|---|---|
| **Install Steam DLC** | Asks Steam to download it. Fails with `Steam.Error.NotOwner` when the player does not own it |
| **Uninstall Steam DLC** | Asks Steam to remove it |
| **Get Steam DLC Progress** | *Downloading*, *Bytes Downloaded*, *Bytes Total* |
| **Get Steam DLC Progress Fraction** | The same as 0 to 1, for a progress bar |

**On DLC Installed** (*App Id*, *Tag*) on the **Steam DLC Subsystem** fires when a download finished. Enable the content there.

## Store page

**Open Steam DLC Store Page** opens the DLC in the Steam overlay, already in the cart. Use it for a "Buy" button next to locked content.

## All DLC

**List Steam DLC** returns every DLC Steam knows for your game: *App Id*, *Name*, *Tag* (when there is a row), *Available*, *Owned*, *Installed*. Use it for a DLC menu.

## By App ID

Every node has a **... By App ID** twin, for example **Is Steam DLC Owned By App ID**. It needs no App Definition row. See [Tags or names](core-concepts.html#tags-or-names).

## Console commands

| Command | What it does |
|---|---|
| `Steam.DLC.Dump` | Your rows with owned / installed, and everything Steam lists |

The test panel (`Steam.Test.Toggle`) has a **DLC** page with install, uninstall and store buttons.

## Good to know

- An unknown tag (not in the App Definition) returns false for the checks and fails Install, Uninstall and Store with `Steam.Error.InvalidArgument`.
- **Client only.**
