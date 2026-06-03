# Baseball Scores

A watchface for the **Pebble Time 2** that keeps live MLB scores on your wrist. It
auto-cycles through up to three of your favorite teams, showing the live score, base
runners, the ball–strike–out count, and the inning — with sensible screens for games that
haven't started, finished games, and off-days.

> Unofficial fan app. Uses the free public [MLB Stats API](https://statsapi.mlb.com).
> Not affiliated with or endorsed by MLB.

## Features

- **Up to 3 teams**, auto-cycling on a configurable interval.
- **Live games** — score, bases diamond, and a `BALLS · STRIKES · OUTS` row with the inning.
- **Scheduled games** — opponent and start time ("Today" or the date).
- **Final games** — final score with a green **WIN** / red **LOSS** badge.
- **Off-days** — your team's next game: opponent, date, and time.
- **Postponed / no-data** states handled gracefully.
- Clock with day and date in the header, on every screen.

## Install

Once published, search **"Baseball Scores"** in the Pebble appstore (in the Pebble / Core
app, or at the web appstore) and tap install.

To sideload a development build:

```sh
pebble build
pebble install --phone <your-phone-ip>   # Developer Connection enabled in the app
```

## Settings

Open the watchface's settings in the phone app:

- **Teams to track** — pick 1–3 teams. Leave a slot on **"None"** to skip it (set only
  Team 1 to track a single team).
- **Team cycle interval** — how long each team is shown before rotating.

Tap **Save settings** to apply — the watch refreshes immediately.

## How it works

Split app: a native **C** watch side renders everything and never touches the network; a
**PebbleKit JS** phone side fetches and normalizes the MLB Stats API and streams one
compact game state per team to the watch over AppMessage.

| Path | Role |
|---|---|
| `src/c/pebble-baseball-c.c` | Watch render + team cycling + AppMessage inbox |
| `src/pkjs/index.js` | Phone fetch + normalize + AppMessage send queue + Clay |
| `src/pkjs/clay-config.json` | Settings UI (team pickers + cycle interval) |
| `package.json` | Metadata, target platform (`emery`), AppMessage `messageKeys` |

## Building

```sh
pebble build                       # -> build/pebble-baseball.pbw (emery)
pebble install --emulator emery    # boots qemu + installs
pebble screenshot --emulator emery out.png
```

Run `pebble clean` first after changing `messageKeys` in `package.json`.
