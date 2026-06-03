# pebble-baseball

A **native C watchface** for the **Pebble Time 2** (platform `emery`, 200×228, 64-color)
that auto-cycles through up to 3 user-chosen MLB teams showing live scores, bases, and
pitch count. Free MLB Stats API, no auth.

## Architecture (split app)

- **Watch side — C** (`src/c/pebble-baseball-c.c`): all rendering. Holds a `GameState[3]`
  array, auto-cycles slots on an `AppTimer`, and draws each of six states with color
  graphics (navy header band with large clock + day/date, bold score, amber bases
  **diamond** via `gpath`, a BALLS·STRIKES·OUTS stat row, WIN/LOSS badge). Receives data
  over AppMessage; never does networking.
- **Phone side — PKJS / JavaScript** (`src/pkjs/index.js`): fetches the MLB Stats API
  (`statsapi.mlb.com`), normalizes each team's game, and **streams one GameState per slot**
  to the watch via AppMessage (sequential send queue — AppMessage is one-at-a-time).
  Reads team selection from Clay settings (`src/pkjs/clay-config.json`, 30 teams).

Data flow: watch `request_update()` / minute tick → PKJS `fetchAll()` → per-team
`sendAppMessage({STATUS, TEAM, MY_SCORE, …, SLOT, TOTAL, CYCLE})` → C `inbox_received`
stores into `s_games[SLOT]` → `layer_mark_dirty`.

The AppMessage keys are declared in `package.json` `messageKeys` and must stay in sync
with both `inbox_received` (C) and the dicts PKJS sends.

## Why C, not Alloy/JS (do not re-litigate)

This started as an Alloy (Moddable/JS) project and **was abandoned**. The firmware
Moddable host loads JS as a *mod* into a shared XS machine whose `creation` is fixed at
`chunk: 8192` inside `static: 32768` — an **8 KB chunk pool, unchangeable from the mod**,
identical for watchface vs app (the `isWatchface` flag only blocks `pebble/button`). A
plain Piu watchface already sat ~0 bytes from that ceiling; any color/font/graphics blew
it. The PT2 itself has ~128 KB app RAM — the 8 KB was the JS sandbox, not the hardware.
Native C reaches the full RAM (~126 KB free in our build) and real graphics. **Don't
propose porting back to Alloy/Piu/Poco for this app.** Background in `plans/`.

## Build / run / test

```bash
pebble build                              # builds build/pebble-baseball.pbw (emery only)
pebble install --emulator emery           # boots qemu + installs
pebble screenshot --emulator emery out.png
pebble logs --emulator emery
```

- After changing `messageKeys` or deps, run `pebble clean` first (stale generated
  `message_keys.auto.*` causes `MESSAGE_KEY_*` build errors).
- PKJS fetch works in the emulator (pypkjs runs on the host Mac with real network).

### Emulator discipline (it is flaky — learned the hard way)

- **Fresh start after any confirmed failure** (app fault / wedged bridge): `pebble kill`,
  `kill -9` stray `qemu-pebble`/`pypkjs`, then one patient `pebble install`. Do **not**
  rapid-cycle installs onto a faulted watch — it corrupts `qemu_spi_flash.bin` and
  bootloops (fix: move that file aside under
  `~/Library/Application Support/Pebble SDK/<ver>/emery/`, reboot fresh).
- **Wrap every `pebble` CLI call in a hard timeout** (e.g. `perl -e 'alarm shift; exec @ARGV'`)
  so a wedged pypkjs bridge can't hang.
- **Don't run `pebble logs` concurrently with `pebble screenshot`** — they contend for the
  single debug bridge and the screenshot times out.

## File map

| Path | Role |
|---|---|
| `src/c/pebble-baseball-c.c` | Watch render + cycling + AppMessage inbox |
| `src/pkjs/index.js` | Phone fetch + normalize + AppMessage send queue + Clay |
| `src/pkjs/clay-config.json` | Settings UI (3 team dropdowns, 30 teams, interval sliders) |
| `package.json` | `targetPlatforms:["emery"]`, `watchface:true`, `configurable`, `messageKeys`, `pebble-clay` dep |
| `plans/` | Original design spec + implementation plan (Alloy-era; historical) |

## Status / TODO

Feature-complete vs spec and verified on the emulator with live data (cycling 3 teams,
all states). Remaining: on-device install + Clay settings exercised on the phone;
live-state seen with a real in-progress game; wire cycle/poll intervals from the Clay
sliders (currently `cycleInterval` flows; defaults NYY/BOS/LAD until teams are picked).
