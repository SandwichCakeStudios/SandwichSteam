---
title: Steam Authentication
nav_order: 4.5
---

# Steam Authentication
{: .no_toc }

Let Steam check every player who joins your hosted game: the player really is who they say, **owns your game**, and has **no VAC or publisher ban**. Unreal does the check; Sandwich Steam turns it on and tells you the result.

1. TOC
{:toc}

## Turn it on

1. Open **Project Settings > Plugins > Sandwich Steam - Auth** and tick **Enable Steam Authentication**. (The same switch is on the Steam Dashboard's **Advanced** page, card *Steam authentication*.)
2. In the Steam Dashboard click **Configure Steam...** and apply. It adds one line to `DefaultEngine.ini`.
3. **Restart the editor**, then package or launch again.

That is all. The **Validate Project** check and the Advanced page card tell you if the setting and the ini line do not match.

**Host and players need the same setting.** If one build has it and the other does not, the player never gets in (the connection keeps retrying). Ship one build and this never happens.

## What happens to a player who fails?

Choose the **Failure Policy** next to the switch:

| Policy | What happens |
|---|---|
| **Kick** (default) | Unreal kicks the player. You still get the result event with the reason. |
| **Custom** | Nobody is kicked. The event **On Steam Auth Failed** fires and your game decides, for example with the **Kick Steam Player** node. |

## Blueprint nodes

The events live on the **Steam Auth Subsystem** (*Get Game Instance Subsystem > Steam Auth Subsystem*, then *Bind Event to On Steam Auth Result*). They fire **on the host only**, a moment after the player logged in.

| Node | What it does |
|---|---|
| **On Steam Auth Result** | Steam answered for a joining player: *Success* and the reason (*OK*, *No Ownership*, *VAC Banned*...) |
| **On Steam Auth Failed** | *Custom* policy only: a player failed and was **not** kicked |
| **Get Steam Auth Status** | *Not Checked*, *Pending*, *Passed* or *Failed* |
| **Is Player Steam Authenticated** | True when Steam accepted the player. The host's own players always count |
| **Kick Steam Player** | Kicks a player with a reason (host only) |
| **Is Steam Auth Active** | True when the check is really running |

Want a "verified" badge on other players' screens? Clients do not get the results. Store the result in a **replicated** variable on your Player State from the host's *On Steam Auth Result*.

## Good to know

- **It is not anti-cheat.** VAC only reports bans that already exist.
- **Testing needs two PCs and two Steam accounts.** To see a failure you need your own App ID: everybody owns the test app Spacewar (480).
- **Without Steam** (Play In Editor, LAN) nothing is checked and players still connect.
- **Not the same as the Web API ticket** (*Get Steam Web API Ticket*). That one is for logging in to your **own** server.
- Dedicated servers are not supported yet.
- Console: `Steam.Auth.Dump` prints the setting, the policy and every player's status.
