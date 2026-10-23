---
title: Achievements
parent: Progression
nav_order: 2
---

# Achievements
{: .no_toc }

Unlock Steam achievements, show progress toasts on the way, and build an achievement list with names, descriptions, icons and how many players got each one.

> **Status:** tested in a Standalone game. Achievements need the [Stats](stats.html) feature: turning Stats off turns Achievements off too.

1. TOC
{:toc}

## Before you begin

- The [Quick Start](quick-start.html) is done. It unlocks your first achievement on Spacewar (480) in 10 minutes.
- **The achievements exist in Steamworks.** Create them on the [Steamworks partner site](https://partner.steamgames.com/) under *Stats & Achievements* and **publish** the changes. Names, descriptions and icons come from there.

## Add the achievements to the App Definition

### Import from Steam

Steam can list the achievements of your game, so you do not have to type them:

1. Press **Play > Standalone Game**, open the console (**~**) and enter `Steam.Stats.ExportSchema`. (Or click **Export schema for the editor importer** in the `Steam.Debug.Show` panel.) Close the game.
2. On the dashboard's **App Definition** page, click **Import from Steam...**.
3. Answer **Yes** to create the tags. You get one tag per achievement: `Steam.Achievement.<ApiName>`.
4. **Save** the App Definition.

Importing again is safe. New achievements are added, your tags and progress settings stay, and rows Steam no longer reports are kept (and listed in the log), never deleted.

### By hand

Add a tag under `Steam.Achievement`, then a row to **Achievements**:

| Field | What it does |
|---|---|
| **Api Name** | The achievement's API name, exactly as in Steamworks |
| **Tag** | The tag you pick in the nodes |
| **Progress Stat** | Optional. The API name of the stat that counts toward it, see *Progress toasts* |
| **Progress Max** | The value of that stat at which the achievement unlocks |
| **Hidden** | Mirrors Steamworks, for your own overview. Steam decides what players see |
| **Editor Display Name**, **Editor Icon** | Only for editor lists. Players see Steam's name and icon |

## Unlock an achievement

**Unlock Steam Achievement** with the achievement's tag. Steam shows its pop-up and the unlock is uploaded right away.

<!-- SHOT 20 is used in the Quick Start: Event BeginPlay -> Unlock Steam Achievement -->

- **Unlocking twice is fine.** An already unlocked achievement succeeds without doing anything, so you do not have to check first.
- **Early is fine.** An unlock before Steam sent the player's stats is kept and happens when they arrive.
- **Is Steam Achievement Unlocked** tells you whether the player has it.

## Progress toasts

For an achievement like *Win 10 games*, Steam can show "3 / 10" on the way. Set it up once and it happens by itself:

1. Create a stat that counts the wins, both in Steamworks and in your App Definition ([Stats](stats.html)).
2. In the achievement's row, set **Progress Stat** to the stat's API name and **Progress Max** to `10`.
3. In your game, only change the stat: **Add Steam Stat Int** (*Steam.Stat.Wins*, 1).

The plugin shows the toast when the stat goes up, at most once per 10 % step (setting *Progress Notify Step Percent*), never at 0 and never at the maximum.

> **Unlocking is still your job.** In Steamworks you can let Steam unlock the achievement when the stat reaches the maximum (set the progress stat there too). Otherwise call **Unlock Steam Achievement** yourself when the count is reached.

**Indicate Steam Achievement Progress** shows a toast by hand, for progress you do not keep in a stat.

## Achievement list

To show an achievement screen in your game:

| Node | What it gives you |
|---|---|
| **Get All Steam Achievements** | Every achievement with *Display Name*, *Description*, *Hidden*, *Unlocked*, *Unlock Time* and *Progress Current* / *Progress Max*. Empty until the stats are ready |
| **Get Steam Achievement Info** | The same for one achievement |
| **Get Steam Achievement Icon** (async) | The icon as a texture: **On Icon Loaded**. Steam gives the locked or the unlocked icon, matching the player's state |
| **Request Steam Achievement Percentages** (async), then **Get Steam Achievement Global Percent** | How many percent of all players unlocked it |

Names and descriptions come from Steam in the player's language. A hidden achievement has no description until it is unlocked.

<!-- SHOT 64: An in-game achievement list: Wait For Steam Stats -> Get All Steam Achievements -> For Each -> list entry with Get Steam Achievement Icon -->

Wait for the stats first (**Wait For Steam Stats**, see [Stats](stats.html#stats-load-when-the-game-starts)), otherwise the list is empty.

## Events

On the **Steam Achievements Subsystem**:

| Event | When |
|---|---|
| **On Achievement Unlocked** (*Achievement Tag*, *Api Name*) | An achievement was unlocked |
| **On Achievement Progress** (*Achievement Tag*, *Api Name*, *Current*, *Max*) | Steam showed a progress toast |

## By name

Every node has a **... By Name** twin that takes the API name, for example **Unlock Steam Achievement By Name**. It needs no App Definition row, but without a row there are no automatic progress toasts and the *Tag* in events stays empty. **Indicate Steam Achievement Progress By Name** needs the *Max Value* pin. See [Tags or names](core-concepts.html#tags-or-names).

## Testing: lock it again

An unlocked achievement stays unlocked on your account. To test again:

- **Clear Steam Achievement** (Development builds only) or `Steam.Achievements.Clear <ApiName>` locks one again.
- `Steam.Stats.ResetAll 1` resets every stat **and** locks every achievement. Cannot be undone.

## Settings

**Project Settings > Plugins > Sandwich Steam - Achievements**

| Setting | Default | What it does |
|---|---|---|
| Auto Indicate Progress From Stats | on | Show progress toasts when a *Progress Stat* changes |
| Progress Notify Step Percent | 10 | At most one toast per step of this size (1 to 50) |

## Console commands

| Command | What it does |
|---|---|
| `Steam.Achievements.Dump` | Every achievement and whether it is unlocked |
| `Steam.Achievements.Unlock <ApiName>` | Unlocks one |
| `Steam.Achievements.Clear <ApiName>` | Locks one again |

The test panel (`Steam.Test.Toggle`, **Stats** page) lists the achievements with buttons to unlock and clear them.

## Good to know

- **Offline:** while the Steam servers are unreachable the feature pauses and calls fail with `Steam.Error.Offline`. Unlock it again when the feature is back (**On Feature Active Changed** on the subsystem).
- **The App Definition is optional** for achievements: the *By Name* nodes work without it. Tags are still recommended.
- Do not mix these nodes with Unreal's own Online Subsystem achievement nodes for the same achievements.
