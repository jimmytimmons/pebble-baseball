# pebble-baseball — Design Spec

**Date:** 2026-05-31  
**Platform:** Pebble Time 2 (Emery) — Alloy framework  
**Status:** Approved, ready for implementation planning

---

## Overview

A Pebble Time 2 watchface that displays live baseball scores for up to three user-configured teams. Teams auto-cycle on a configurable interval. Data is fetched directly from the official stats API via the phone proxy — no backend required.

---

## Architecture

```
Pebble Time 2 (Alloy watchface)
  └── fetch() calls
        └── @moddable/proxy (Bluetooth)
              └── PebbleKit JS on phone (HTTPS)
                    └── statsapi.mlb.com
```

- The watch has no direct internet access. All HTTP calls go through `@moddable/proxy`, which proxies them over Bluetooth to PebbleKit JS running on the paired phone.
- No backend server. No authentication. The stats API is free and has no documented rate limits.
- Clay generates a native-looking settings page in the Pebble phone app from a JSON definition.

---

## Project Structure

```
pebble-baseball/
├── src/
│   ├── index.js          # Watchface entry point — orchestrates timers, wires proxy to display
│   ├── scores.js         # All API calls + GameState normalisation (only file that references MLB)
│   ├── display.js        # Piu UI components — renders GameState, no fetch logic
│   └── settings.js       # localStorage read/write helpers with defaults
├── config/
│   └── clay-config.json  # Clay UI definition
└── package.json          # Alloy manifest
```

`scores.js` is the only file permitted to reference the upstream API by name (in comments). All other files consume `GameState` objects.

---

## Timers

Three independent timers run concurrently after the proxy signals ready:

| Timer | Interval | Purpose |
|---|---|---|
| **Cycle** | User-set, default 15s | Advances which team slot is displayed. Never waits on a network call. |
| **Poll** | User-set, 30s floor, default 60s | Fetches linescore for each *live* game only. Off-day / scheduled / final slots are skipped. |
| **Schedule** | Every 5 min (fixed) | Re-fetches `/schedule` for all tracked teams to detect state transitions: scheduled → live, live → final, postponements. |

---

## API Endpoints

All calls go through the phone proxy. No API key required.

### 1. Schedule + hydrate (schedule timer, and initial load)
```
GET https://statsapi.mlb.com/api/v1/schedule
  ?sportId=1
  &teamId={id}
  &date={YYYY-MM-DD}
  &hydrate=linescore
```
Returns game status, `gamePk`, score, inning, outs, bases, and ball/strike count in one call. ~10–15 KB per team.

### 2. Linescore (poll timer, live games only)
```
GET https://statsapi.mlb.com/api/v1/game/{gamePk}/linescore
```
Returns score, inning, half, outs, balls, strikes, and `offense.first/second/third` for base runners. ~5–10 KB. Fast refresh endpoint.

### 3. Next game lookup (once per off-day team, at launch)
```
GET https://statsapi.mlb.com/api/v1/schedule
  ?sportId=1
  &teamId={id}
  &startDate={tomorrow}
  &endDate={today+14}
```
Returns next scheduled game date, time, and opponent. ~5 KB. Not polled — called once when the day's schedule shows no game.

---

## GameState Data Model

`scores.js` normalises all API responses into this shape before returning. `display.js` never reads raw API JSON.

```js
{
  // Identity
  teamId:       147,         // numeric MLB team ID
  teamAbbrev:   "NYY",
  teamName:     "Yankees",

  // One of: "live" | "scheduled" | "final" | "off_day" | "postponed" | "error"
  // Note: "no connection" is not a status — it is a display-layer overlay applied
  // when a fetch fails and lastUpdated is stale. The last valid status is preserved.
  status:       "live",

  // live + final fields (null otherwise)
  opponent:     "BOS",
  isHome:       false,       // true if tracked team is the home team
  myScore:      4,           // tracked team's score
  theirScore:   2,
  inning:       7,
  inningHalf:   "Bot",       // "Top" | "Bot"
  outs:         2,
  balls:        3,
  strikes:      1,
  first:        true,        // runner on base (true/false)
  second:       false,
  third:        true,

  // scheduled fields (null otherwise)
  gameTime:     "7:05 PM",
  gameDate:     "Today",     // "Today" | "Mon Jun 2" etc.

  // off_day fields (null otherwise)
  nextOpponent: "BOS",
  nextDate:     "Thu Jun 4",
  nextTime:     "7:05 PM ET",

  // meta
  gamePk:       717465,      // used by poll timer for linescore calls
  lastUpdated:  1748700000,  // unix timestamp — drives stale indicator
}
```

`isHome` + `myScore`/`theirScore` mean `display.js` always renders the tracked team on the left without re-deriving which side is which. Fields irrelevant to the current `status` are `null`.

---

## UI States

`display.js` branches on `GameState.status`. Each team slot independently holds one state.

### 1. Live
Score (large, both teams), inning + half + outs, bases diamond (amber = runner on base, grey = empty), ball/strike/pitch count. Cycle dot indicator shows position in the 3-team rotation.

### 2. Scheduled
Team full name, opponent abbreviation, today's start time. No linescore polling. Schedule timer detects the transition to live.

### 3. Final
Score dimmed, FINAL badge, win attribution (e.g. "NYY wins · F/9"). No polling. Persists until next day's schedule loads.

### 4. Off Day
Team full name, "next game" label, next opponent, date, and start time (fetched once at launch via the date-range schedule call).

### 5. No Connection *(display overlay, not a GameState status)*
When a fetch fails, the last valid `GameState` is preserved. `display.js` detects staleness via `lastUpdated` and overlays: last known data at reduced opacity, red ⚠ banner, time-since-last-update label. Retries silently on next poll tick.

### 6. Postponed
Team name, **PPD** badge, reschedule date if available from API. All polling stops for the slot.

---

## Clay Config Fields

```json
[
  { "type": "heading", "defaultValue": "Baseball Scores" },

  { "type": "section", "label": "Teams to track",
    "items": [
      { "type": "select", "messageKey": "team1", "label": "Team 1",
        "defaultValue": "0", "options": [ {"label":"None","value":"0"}, ...all 30 teams as {label, value}... ] },
      { "type": "select", "messageKey": "team2", "label": "Team 2",
        "defaultValue": "0",  "options": [ {"label":"None","value":"0"}, ...30 teams... ] },
      { "type": "select", "messageKey": "team3", "label": "Team 3",
        "defaultValue": "0",  "options": [ {"label":"None","value":"0"}, ...30 teams... ] }
    ]
  },

  { "type": "section", "label": "Display",
    "items": [
      { "type": "range", "messageKey": "cycleInterval",  "label": "Cycle interval",
        "defaultValue": 15, "min": 5,  "max": 60,  "step": 5  },
      { "type": "range", "messageKey": "pollInterval",   "label": "Refresh interval",
        "defaultValue": 60, "min": 30, "max": 300, "step": 30 }
    ]
  }
]
```

Settings are written to `localStorage` on the watch when Clay config is saved. Defaults apply on first launch before any settings have been saved.

---

## Settings Persistence

| Key | Default | Storage |
|---|---|---|
| `team1` | `0` (None — settings prompt shown on first launch) | localStorage |
| `team2` | `0` (None) | localStorage |
| `team3` | `0` (None) | localStorage |
| `cycleInterval` | `15` | localStorage |
| `pollInterval` | `60` | localStorage |

`localStorage` in Alloy is backed by Pebble flash storage — survives reboots and battery swaps. Total footprint ~100 bytes, well under the 4 KB persistent storage limit.

If `team1` is `0` (unconfigured) at startup, skip all timers and show a single "Open settings to pick a team" screen.

---

## Error Handling

| Case | Behaviour |
|---|---|
| Network failure / API error | Keep last `GameState` in memory, set `lastUpdated`. Render stale data + ⚠ banner. Retry on next poll tick. |
| Proxy not ready at launch | Wait for `connected` event before starting any fetch or timer. Show "Connecting…" per slot. |
| First launch, no cached data | Show "Loading…" per configured slot until first fetch resolves. |
| Unexpected / malformed API response | `scores.js` validates required fields. Returns `status: "error"` if critical fields are absent. Display shows team name + "Data unavailable". |
| Game postponed | Returns `status: "postponed"`. Show PPD badge. Stop polling for that slot. |
| All slots unconfigured | Skip timers, show settings prompt. |

---

## Network Call Analysis

Two-tier polling: schedule check every 5 min + linescore at user poll interval (live games only).

| Scenario | Poll interval | Calls/min | Data/min | Per 3hr game |
|---|---|---|---|---|
| 1 team, off day | — | 0.2 | ~2 KB | N/A |
| 1 team, scheduled | — | 0.2 | ~2 KB | N/A |
| 1 team, 1 live game | 60s | 1.2 | ~10 KB | ~1.8 MB |
| 1 team, 1 live game | 30s | 2.2 | ~18 KB | ~3.2 MB |
| 3 teams, all off day | — | 0.6 | ~6 KB | N/A |
| 3 teams, 1 live game | 60s | 1.6 | ~14 KB | ~2.5 MB |
| 3 teams, 2 live games | 60s | 2.6 | ~24 KB | ~4.3 MB |
| 3 teams, 3 live games | 60s | 3.6 | ~34 KB | ~6.1 MB |
| **3 teams, 3 live games** | **30s** | **6.6** | **~62 KB** | **~11.2 MB** |

Worst case (~11 MB over 3 hours) is well within normal mobile data usage. Linescore calls for multiple live games are staggered, not simultaneous.

---

## Development Environment

**Recommended starting point: CloudPebble**
- Browser IDE at `cloudpebble.net` (hosted by Rebble)
- Built-in Emery emulator — test all UI states without a phone
- Requires a free Rebble account at rebble.io (original Pebble servers are offline)
- Deploy to device via Pebble phone app over Bluetooth

**Alternative: Local Alloy SDK**
- `pebble` CLI + Alloy toolchain
- `pebble build && pebble install --emulator emery`
- Works offline, integrates with VS Code
- Same Rebble account needed for device installs

---

## Testing Approach

| Area | Method |
|---|---|
| UI states | Stub `scores.js` to return each hardcoded `GameState`. Cycle through all 6 states in emulator. |
| Layout | Verify all states render without text clipping, including long names ("Minnesota Twins"). |
| Network | Real device + phone. Confirm: proxy connects, schedule call succeeds, poll fires at correct interval, stale banner appears when phone leaves Bluetooth range. |
| Clay config | Save settings from phone app → confirm watch receives updated team IDs and intervals without a watchface restart. |
| Cycle timer | Confirm cycle advances independently of fetch state — no visible stutter when a network call is in flight. |
