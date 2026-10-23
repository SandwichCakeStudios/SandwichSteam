---
title: Rich Presence
parent: Social
nav_order: 2
---

# Rich Presence
{: .no_toc }

Show friends what the player is doing, right in the Steam friends list: "Playing on Harbor", "In the main menu", "Party of 3". Rich presence also makes **Join Game** work.

> **Status:** tested with one account (the data was checked on a second one).

1. TOC
{:toc}

## Before you begin

- The [Quick Start](quick-start.html) is done: App ID set, an App Definition assigned.
- **Steam only shows translated text.** Your statuses become text in the friends list only after you upload a localization file to Steamworks for your App ID. This page shows how to generate it.
- **On Spacewar (480)** you cannot upload that file. Test with the raw `status` key instead (see *Testing on Spacewar*).

## Add the statuses to the App Definition

1. Add a tag per status under `Steam.Presence`, for example `Steam.Presence.InMatch`.
2. Add a row to **Presence** in your App Definition:

| Field | What it does |
|---|---|
| **Tag** | The tag you pick in the nodes |
| **Token** | The status's name for Steam. Starts with `#`, for example `#Status_InMatch` |
| **Extra Keys** | Values the text uses, for example `map` |
| **Localized Text** | What friends see, with the keys in braces: `Playing on {map}`. Localize it like any other text in your project |

## Upload the text to Steam

1. On the dashboard's **App Definition** page, click **Generate Rich Presence Localization...** (also on the App Definition asset itself).
2. It writes `richpresence_<AppId>.vdf` into the plugin's data folder, with one block per language you translated. Click **Show file**.
3. Upload that file on the [Steamworks partner site](https://partner.steamgames.com/) in your app's rich presence localization settings.

Generate and upload again whenever you add or change a status. Keys written in the text but missing from *Extra Keys* are reported in the Output Log.

## Set the status

**Set Steam Presence** with the status tag and its *Args*, for example `map` = `Harbor`.

<!-- SHOT 69: Blueprint: On match start -> Set Steam Presence (Steam.Presence.InMatch, Args: map = Harbor) -->

- Call it whenever the player's situation changes: main menu, in a match, map changed.
- Keys the previous status used and the new one does not are removed for you.
- Several changes in one frame are sent as one update, and a value Steam already has is not sent again, so calling it often is cheap.
- **Clear Steam Presence** removes everything, for example when the player goes back to the main menu and you want no status.

### Groups

**Set Steam Presence Group** with a *Group Id* and the *Size* tells Steam which friends play together. The friends list groups them ("Party of 3"). Use the same id for everyone in the party, for example the party lobby's Steam ID as text. An empty id removes the group.

### Join Game

**Set Steam Connect String** sets what Steam starts the game with when a friend clicks **Join Game**. With Sandwich Steam sessions or a party, set it to `+connect_lobby ` followed by **Get Joinable Steam Lobby Id**, after you created or joined the session or party. The [Sessions](sessions.html#invites-and-join-game) and [Party Lobby](party-lobby.html#invites-and-join-game) guides show where.

## Read a friend's status

| Node | What it does |
|---|---|
| **Get Steam Friend Presence Value** (*Friend*, *Key*) | One value, for example `steam_display` (the translated text) or your own key |
| **Get Steam Friend Presence** | All values of a friend |
| **Request Steam Friend Presence** | Asks Steam for a user's values, for players who are not friends |

**On Friend Presence Changed** on the **Steam Presence Subsystem** fires when a friend's values change.

## Other nodes

| Node | What it does |
|---|---|
| **Set Steam Presence By Token** | The same as Set Steam Presence, with the token instead of a tag. No App Definition row needed |
| **Set Steam Presence Value**, **Remove Steam Presence Value** | Any key you like. An empty value removes it |

## Testing on Spacewar

Spacewar has no localization file of yours, so the friends list stays empty for your statuses. To check that values arrive:

1. On machine A: `Steam.Presence.Raw status Hello` (the `status` key is plain text, no file needed).
2. On machine B: `Steam.Presence.Friend <A's SteamID64>` logs A's values.

## Console commands

| Command | What it does |
|---|---|
| `Steam.Presence.Dump` | The statuses and the keys set right now |
| `Steam.Presence.Set <Tag> [Key=Value ...]` | Sets a status |
| `Steam.Presence.Raw <Key> [Value]` | Sets or removes one key |
| `Steam.Presence.Clear` | Clears everything |
| `Steam.Presence.Friend <SteamID64>` | Logs a friend's values |

## Good to know

- **At most 30 keys** at once (Steam's limit). A change that would exceed it fails with `Steam.Error.QuotaExceeded` and changes nothing.
- A token that is not in the uploaded file shows as plain text, Steam does not check it.
- When Steam reconnects after being offline, the plugin sends the current status again.
- **Client only.**
