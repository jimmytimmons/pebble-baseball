# pebble-baseball Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an Alloy watchface for Pebble Time 2 that auto-cycles through up to 3 user-configured teams showing live baseball scores, bases, and pitch count.

**Architecture:** A self-contained Alloy project. Watch code lives under `src/embeddedjs/`, split into modules (`main.js`, `scores.js`, `display.js`, `settings.js`) declared in `src/embeddedjs/manifest.json`. Phone code lives in `src/pkjs/index.js` and wires the network proxy plus Clay config. `scores.js` fetches and normalises MLB Stats API responses into a `GameState` object; `display.js` renders that object using Piu; `main.js` orchestrates three `setInterval` timers, the `@moddable/pebbleproxy` network bridge, and the Clay settings `Message`.

**Tech Stack:** Alloy (Moddable SDK, XS engine, ES2025), Piu UI framework, `@moddable/pebbleproxy` for HTTP via the phone, Clay for the settings UI, MLB Stats API (free, no auth).

---

## ⚠️ Reality-check corrections applied (vs. the original draft)

This plan was revised after verifying every framework assumption against the official repebble docs and the `moddable-OpenSource/pebble-examples` repo. The original draft contained several APIs that do not exist as written. Corrections, each grounded in a verified source:

| # | Original (wrong) | Corrected (verified) | Source |
|---|---|---|---|
| 1 | Flat `src/index.js`, `src/scores.js`, … | Watch code under `src/embeddedjs/`; phone code in `src/pkjs/`; each project has `src/c/mdbl.c`, `wscript`, and `src/embeddedjs/manifest.json` | Alloy guide; every `hello*` example |
| 2 | Multi-file split assumed to "just work" | Extra modules must be listed in `src/embeddedjs/manifest.json` under `modules`; imports use bare specifiers (`import {x} from "scores"`) | `hellomodule` example |
| 3 | Dependency `@moddable/proxy` | `@moddable/pebbleproxy` (install via `pebble package install @moddable/pebbleproxy`) | Networking guide |
| 4 | `new Proxy()` + `proxy.addEventListener("connected")` | Global `watch`; `watch.addEventListener("connected", …)` / `watch.connected.pebblekit`. `fetch()` is a real global returning a standard `Response` | Networking guide; `hellofetch` |
| 5 | Clay handled on the watch via a `"settings"` proxy event | Clay runs in **PKJS** (`new Clay(config)`); config reaches the watch as an AppMessage read via the `Message` class (`import Message from "pebble/message"`) | App Configuration guide; App Messages guide; `hellomessage` |
| 6 | `Timer.repeat(...)` with no import; no way to reconfigure intervals | Global `setInterval(cb, ms)` / `clearInterval(id)` (verified — no import) — store the ids so settings changes hot-reload without `System.restart()` (which is not relied on) | `hellotimer` example |
| 7 | Clock recomputed only on cycle/fetch (would freeze between updates) | Subscribe `watch.addEventListener("minutechange", …)` to refresh the display every minute | Watchfaces guide |
| 8 | `trace(...)` for debug output | `console.log(...)` | Alloy guide; all examples |
| 9 | `hydrate=linescore` | `hydrate=team,linescore` — team `abbreviation`/`shortName` are **absent** without the `team` hydrate, and the whole display keys off abbreviations | Verified against `statsapi.mlb.com` response |
| 10 | `isoDate` via `toISOString()` (UTC) | Local-date formatter — UTC rolls the date forward for evening US games and shows a false off-day | Logic review |
| 11 | `normalizeLinescore` set `inningHalf` to `"Top"` when absent | Preserve `existing.inningHalf` when the payload omits it | Logic review |
| 12 | `fetchAllSchedules` rendered every team at slot `0` | Render only when `i === currentSlot` so initial load doesn't fight the cycle timer | Logic review |

**Still flagged for on-device confirmation (could not be fully verified from docs):**
- **Clay package/require name.** The repebble *App Configuration* guide shows `require('@rebble/clay')`; the historical pebble-dev package is `pebble-clay` (`require('pebble-clay')`). This plan uses `pebble-clay`/`require('pebble-clay')` — **confirm the exact name** via the App Configuration guide / `pebble package install` output before relying on it.
- **Piu watchface mount.** The verified `piu-watchface` example uses `Application.template(...)` then `export default new WatchfaceApp({}, {})`. This plan keeps the imperative `new Application(null, {...})` (also valid Piu) but makes it the **entry module's default export** to match how the launcher picks up the root Application. Verify the watchface renders at all in Step-5/Step-8 emulator checks before building further.
- **`screen.width`/`screen.height`** for Piu sizing — confirm in the emulator (Poco exposes `render.width/height`; Piu Applications size to the display, and `screen` is a documented global).

---

## File Map

| File | Role |
|---|---|
| `package.json` | Alloy manifest — `targetPlatforms:["emery"]`, `watchapp.watchface`, `capabilities:["configurable"]`, `enableMultiJS:true`, `messageKeys`, dependencies (`@moddable/pebbleproxy`, `pebble-clay`) |
| `wscript` | Build script (generated by the scaffold — leave unmodified) |
| `src/c/mdbl.c` | C entry point that launches the embedded JS (generated — leave unmodified) |
| `src/embeddedjs/manifest.json` | Declares Piu + net + timer includes and the four watch JS modules |
| `src/embeddedjs/main.js` | Entry — `watch` readiness, three `setInterval` timers, state array, Clay `Message`, `minutechange`, wires scores → display. Default-exports the Piu Application. |
| `src/embeddedjs/scores.js` | All API calls + `GameState` normalisation. Only file that may reference the upstream API in comments. |
| `src/embeddedjs/display.js` | Piu layout — renders any `GameState`. No fetch logic. |
| `src/embeddedjs/settings.js` | `localStorage` read/write with typed defaults. |
| `src/embeddedjs/scores.stub.js` | Hardcoded `GameState` array covering all 6 states. Swap into `main.js` imports for emulator UI testing. |
| `src/pkjs/index.js` | Phone side — registers `@moddable/pebbleproxy` and initialises Clay. |
| `src/pkjs/clay-config.json` | Clay UI definition — 3 team dropdowns, 2 sliders. Required by `pkjs/index.js`. |

---

## Reference (verified URLs)

- Alloy guide: https://developer.repebble.com/guides/alloy/
- Networking (proxy + `fetch`): https://developer.repebble.com/guides/alloy/networking/
- App Messages (`Message` class): https://developer.repebble.com/guides/alloy/app-messages/
- Storage (`localStorage`): https://developer.repebble.com/guides/alloy/storage/
- Watchfaces (Piu + `minutechange`): https://developer.repebble.com/guides/alloy/watchfaces/
- App Configuration (Clay): https://developer.repebble.com/guides/user-interfaces/app-configuration/
- Examples repo: https://github.com/moddable-OpenSource/pebble-examples
  — study `hellotimer`, `hellofetch`, `hellomessage`, `hellolocalstorage`, `hellopiu-text`, `hellomodule`, `hellowatchface` for the exact patterns this plan uses.

---

## Task 1: Project scaffold

**Files:** `package.json`, `wscript`, `src/c/mdbl.c`, `src/embeddedjs/main.js`, `src/embeddedjs/manifest.json`, `src/pkjs/index.js`

- [ ] **Step 1: Create a Rebble account**

  Sign up at https://rebble.io (free). Required for CloudPebble and for sideloading to a device — the original Pebble servers are offline.

- [ ] **Step 2: Create the Alloy project**

  Either **CloudPebble** (https://cloudpebble.net → Create Project → type **Alloy**, template **Watchface**, platform **Emery**) or the **local SDK** (`pebble new-project --alloy pebble-baseball`). Both generate the canonical scaffold:

  ```
  pebble-baseball/
  ├── package.json
  ├── wscript
  └── src/
      ├── c/mdbl.c                 # leave unmodified
      ├── embeddedjs/
      │   ├── main.js
      │   └── manifest.json
      └── pkjs/
          └── index.js
  ```

- [ ] **Step 3: Add dependencies**

  ```bash
  pebble package install @moddable/pebbleproxy
  pebble package install pebble-clay   # confirm exact name — see "Still flagged" note above
  ```

  These add to `package.json` `dependencies`. Do not hand-edit the dependency versions.

- [ ] **Step 4: Set the watchface manifest keys in `package.json`**

  Ensure the `pebble` block contains (merge with what the scaffold generated; keep the generated `uuid`):

  ```json
  {
    "pebble": {
      "displayName": "Baseball Scores",
      "projectType": "moddable",
      "sdkVersion": "3",
      "targetPlatforms": ["emery"],
      "enableMultiJS": true,
      "watchapp": { "watchface": true },
      "capabilities": ["configurable"],
      "messageKeys": ["team1", "team2", "team3", "cycleInterval", "pollInterval"],
      "resources": { "media": [] }
    }
  }
  ```

  > `projectType: "moddable"` marks this as an Alloy/Moddable app (verified in the `hellomodule` example's `package.json`). Keep the scaffold-generated `uuid`.

  - `enableMultiJS: true` is required for the multi-module split (Task 2+).
  - `capabilities: ["configurable"]` makes the phone app show the settings gear.
  - `messageKeys` must list every Clay setting key, or Clay cannot deliver them.

- [ ] **Step 5: Write `src/embeddedjs/manifest.json`**

  Declares the framework include and the four JS modules. Bare-specifier imports (`import {x} from "scores"`) resolve through the `modules` map. Note the extension convention from the `hellomodule` example: the entry is `"./main"` (no extension), additional modules carry `.js`.

  ```json
  {
    "include": [
      "$(MODDABLE)/examples/manifest_mod.json",
      "$(MODDABLE)/examples/manifest_typings.json"
    ],
    "modules": {
      "*": [
        "./main",
        "./settings.js",
        "./scores.js",
        "./display.js"
      ]
    }
  }
  ```

  > **Verified against the raw example manifests:** every Alloy example (`hellotimer`, `hellofetch`, `hellomessage`, `hellolocalstorage`, `hellopiu-text`, `hellowatchface`, `hellomodule`) includes exactly `manifest_mod.json` + `manifest_typings.json` and nothing else. That single `manifest_mod.json` catch-all provides Piu, `fetch`, the `Message` class, `localStorage`, and the global `setInterval`/`clearInterval` — so no per-feature includes are needed. (`scores.stub.js` is only swapped in during display testing; add it to `modules` temporarily if you import it.)

- [ ] **Step 6: Write `src/pkjs/index.js` (proxy + Clay)**

  ```js
  // src/pkjs/index.js — runs on the phone (PebbleKit JS).
  // Wires the Moddable network proxy and the Clay config page.
  var moddableProxy = require("@moddable/pebbleproxy");
  var Clay = require("pebble-clay");               // confirm name — see "Still flagged"
  var clayConfig = require("./clay-config.json");  // added in Task 7
  var clay = new Clay(clayConfig);                 // auto-handles showConfiguration + webviewclosed

  Pebble.addEventListener("ready", function (e) {
    moddableProxy.readyReceived(e);
  });

  // The proxy and our own messages share the AppMessage channel.
  // appMessageReceived() returns true when the proxy consumed the message.
  Pebble.addEventListener("appmessage", function (e) {
    var handled = moddableProxy.appMessageReceived(e);
    if (handled) return;
    // Non-proxy inbound messages would be handled here (none expected from the watch in v1).
  });
  ```

  > Clay (with default `autoHandleEvents`) sends the saved settings to the watch via AppMessage keyed by the `messageKeys`, where `main.js`'s `Message` reads them (Task 8). The `clay-config.json` is added in Task 7; until then this file will fail to `require` it — create a `[]` placeholder if you want Step 7 of this task to pass first.

- [ ] **Step 7: Verify the emulator launches**

  Run in the Emery emulator. It should open with a blank/default watchface and **no console errors**. If the `clay-config.json` require fails, add a temporary `config/clay-config.json` placeholder (`[]`) or comment the Clay lines until Task 7.

- [ ] **Step 8: Commit**

  ```bash
  cd /Users/jimmytimmons/Projects/pebble-baseball
  git add -A
  git commit -m "chore: scaffold Alloy watchface (embeddedjs/pkjs, proxy + clay wiring)"
  ```

---

## Task 2: settings.js

**Files:** Create `src/embeddedjs/settings.js`

- [ ] **Step 1: Write settings.js**

  ```js
  // src/embeddedjs/settings.js
  // Persistent settings in localStorage (backed by Pebble flash — survives reboots).
  const DEFAULTS = {
    team1: 0,
    team2: 0,
    team3: 0,
    cycleInterval: 15,
    pollInterval: 60,
  };

  function readNum(key, def) {
    const raw = localStorage.getItem(key);     // null when unset
    const n = raw == null ? def : parseInt(raw, 10);
    return Number.isFinite(n) ? n : def;
  }

  export function readSettings() {
    return {
      team1:         readNum("team1", DEFAULTS.team1),
      team2:         readNum("team2", DEFAULTS.team2),
      team3:         readNum("team3", DEFAULTS.team3),
      cycleInterval: readNum("cycleInterval", DEFAULTS.cycleInterval),
      // enforce the 30s floor from the spec
      pollInterval:  Math.max(30, readNum("pollInterval", DEFAULTS.pollInterval)),
    };
  }

  export function writeSettings(incoming) {
    for (const [key, value] of Object.entries(incoming)) {
      localStorage.setItem(key, String(value));
    }
  }

  export function teamIds(settings) {
    return [settings.team1, settings.team2, settings.team3].filter(id => id > 0);
  }
  ```

- [ ] **Step 2: Verify defaults in the emulator**

  Temporarily in `main.js`:

  ```js
  import { readSettings } from "settings";
  console.log(`settings: ${JSON.stringify(readSettings())}`);
  ```

  Expected console output:
  ```
  settings: {"team1":0,"team2":0,"team3":0,"cycleInterval":15,"pollInterval":60}
  ```

  Remove the line after verifying.

- [ ] **Step 3: Commit**

  ```bash
  git add src/embeddedjs/settings.js src/embeddedjs/main.js
  git commit -m "feat: add settings localStorage helpers"
  ```

---

## Task 3: scores.js — normalisation functions

**Files:** Create `src/embeddedjs/scores.js` (normalisation only; fetch functions in Task 4)

The normalisation functions are pure (no Pebble APIs), so they are unit-testable. Step 5 below adds a Node harness in addition to the emulator trace.

- [ ] **Step 1: Write the helpers**

  ```js
  // src/embeddedjs/scores.js
  // MLB Stats API — https://statsapi.mlb.com
  // This is the only file that references the upstream API by name.

  function formatTime(isoString) {
    const d = new Date(isoString);
    let h = d.getHours(), m = d.getMinutes();
    const ampm = h >= 12 ? "PM" : "AM";
    h = h % 12 || 12;
    return `${h}:${m.toString().padStart(2, "0")} ${ampm}`;
  }

  function formatDate(isoString) {
    const d = new Date(isoString);
    const today = new Date();
    if (d.toDateString() === today.toDateString()) return "Today";
    const days   = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"];
    const months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
    return `${days[d.getDay()]} ${months[d.getMonth()]} ${d.getDate()}`;
  }

  // "Bottom" -> "Bot", "Top" -> "Top", anything else -> null (so the field stays null
  // for scheduled games and is *preserved* by the linescore path when absent).
  function normHalf(raw) {
    if (raw === "Bottom") return "Bot";
    if (raw === "Top")    return "Top";
    return null;
  }

  function mapStatus(detailedState) {
    if (!detailedState) return "error";
    const s = detailedState.toLowerCase();
    if (s.includes("progress") || s.includes("live")) return "live";
    if (s.includes("final") || s.includes("game over") || s.includes("completed")) return "final";
    if (s.includes("postponed") || s.includes("suspended")) return "postponed";
    return "scheduled"; // "Scheduled", "Pre-Game", "Warmup", "Delayed", etc.
  }
  ```

- [ ] **Step 2: Write normalizeScheduleResponse**

  Note `hydrate=team,linescore` is what makes `abbreviation`/`shortName` present (verified — they are absent with `hydrate=linescore` alone).

  ```js
  // src/embeddedjs/scores.js (continued)

  export function normalizeScheduleResponse(data, teamId) {
    const game = data?.dates?.[0]?.games?.[0];
    if (!game) return null;  // no game today -> caller produces off_day

    const isHome    = game.teams.home.team.id === teamId;
    const myTeam    = isHome ? game.teams.home : game.teams.away;
    const theirTeam = isHome ? game.teams.away : game.teams.home;
    const ls        = game.linescore;
    const status    = mapStatus(game.status?.detailedState);

    return {
      teamId,
      teamAbbrev:   myTeam.team.abbreviation,
      teamName:     myTeam.team.shortName ?? myTeam.team.name,
      status,
      opponent:     theirTeam.team.abbreviation,
      isHome,
      myScore:      myTeam.score    ?? 0,
      theirScore:   theirTeam.score ?? 0,
      inning:       ls?.currentInning ?? null,
      inningHalf:   normHalf(ls?.inningHalf),
      outs:         ls?.outs    ?? null,
      balls:        ls?.balls   ?? null,
      strikes:      ls?.strikes ?? null,
      first:        !!ls?.offense?.first,
      second:       !!ls?.offense?.second,
      third:        !!ls?.offense?.third,
      gameTime:     status === "scheduled" ? formatTime(game.gameDate) : null,
      gameDate:     status === "scheduled" ? "Today" : null,
      nextOpponent: null,
      nextDate:     null,
      nextTime:     null,
      gamePk:       game.gamePk,
      lastUpdated:  Date.now(),
    };
  }
  ```

- [ ] **Step 3: Write normalizeLinescore**

  Reads the standalone `/game/{pk}/linescore` shape: `teams.home/away.runs`, top-level `currentInning/inningHalf/outs/balls/strikes`, and top-level `offense.first/second/third` (verified). Does **not** change `status` (only the schedule timer may), and **preserves** `inningHalf` when the payload omits it.

  ```js
  // src/embeddedjs/scores.js (continued)

  export function normalizeLinescore(data, existing) {
    if (!data || !existing) return existing;
    const half = normHalf(data.inningHalf);
    return {
      ...existing,
      myScore:    existing.isHome ? (data.teams?.home?.runs ?? existing.myScore)
                                  : (data.teams?.away?.runs ?? existing.myScore),
      theirScore: existing.isHome ? (data.teams?.away?.runs ?? existing.theirScore)
                                  : (data.teams?.home?.runs ?? existing.theirScore),
      inning:     data.currentInning ?? existing.inning,
      inningHalf: half ?? existing.inningHalf,          // preserve when absent
      outs:       data.outs    ?? existing.outs,
      balls:      data.balls   ?? existing.balls,
      strikes:    data.strikes ?? existing.strikes,
      first:      !!data.offense?.first,
      second:     !!data.offense?.second,
      third:      !!data.offense?.third,
      lastUpdated: Date.now(),
    };
  }
  ```

- [ ] **Step 4: Write normalizeNextGame**

  ```js
  // src/embeddedjs/scores.js (continued)

  export function normalizeNextGame(data, teamId) {
    for (const date of data?.dates ?? []) {
      for (const game of date.games ?? []) {
        const isHome    = game.teams.home.team.id === teamId;
        const theirTeam = isHome ? game.teams.away : game.teams.home;
        return {
          nextOpponent: theirTeam.team.abbreviation,
          nextDate:     formatDate(game.gameDate),
          nextTime:     formatTime(game.gameDate),
        };
      }
    }
    return null;
  }
  ```

- [ ] **Step 5: Unit-test the pure functions in Node, then trace in the emulator**

  These functions touch no Pebble API, so test them with the local Node runtime (already available). Create `test/scores.test.mjs` that imports the normalisers and asserts against a fixture:

  ```js
  // test/scores.test.mjs   (run: node test/scores.test.mjs)
  import assert from "node:assert";
  import { normalizeScheduleResponse, normalizeLinescore } from "../src/embeddedjs/scores.js";

  const liveFixture = {
    dates: [{ games: [{
      gamePk: 717465, gameDate: "2026-05-31T23:10:00Z",
      status: { detailedState: "In Progress" },
      teams: {
        away: { team: { id: 147, abbreviation: "NYY", shortName: "Yankees" }, score: 4 },
        home: { team: { id: 111, abbreviation: "BOS", shortName: "Red Sox" }, score: 2 },
      },
      linescore: { currentInning: 7, inningHalf: "Bottom", outs: 2, balls: 3, strikes: 1,
        offense: { first: { id: 1 }, third: { id: 2 } } },
    }]}],
  };

  const gs = normalizeScheduleResponse(liveFixture, 147);
  assert.equal(gs.status, "live");
  assert.equal(gs.myScore, 4);
  assert.equal(gs.theirScore, 2);
  assert.equal(gs.inning, 7);
  assert.equal(gs.inningHalf, "Bot");
  assert.deepEqual([gs.first, gs.second, gs.third], [true, false, true]);

  // inningHalf preserved when the linescore payload omits it
  const u = normalizeLinescore({ teams:{home:{runs:3},away:{runs:5}}, currentInning:8 }, gs);
  assert.equal(u.inningHalf, "Bot");
  assert.equal(u.theirScore, 3); // away team is "mine" (isHome=false), home runs are theirs
  console.log("scores normalisation: all assertions passed");
  ```

  > `scores.js` uses `export` (ES modules) and no Pebble globals in these functions, so Node can import it directly. If Node complains about the bare `?.`/`??`, the installed Node 22 supports them natively. The fetch functions added in Task 4 reference the global `fetch` — keep them below the normalisers; Node won't execute them during this import.

  Run: `node test/scores.test.mjs` → expect `scores normalisation: all assertions passed`.

  Optionally also `console.log` the same in the emulator to confirm parity on-device.

- [ ] **Step 6: Commit**

  ```bash
  git add src/embeddedjs/scores.js test/scores.test.mjs
  git commit -m "feat: add scores normalisation + Node unit tests"
  ```

---

## Task 4: scores.js — fetch functions + scores.stub.js

**Files:** Modify `src/embeddedjs/scores.js`; create `src/embeddedjs/scores.stub.js`

- [ ] **Step 1: Add fetch functions to scores.js**

  `fetch` is a real global in embedded JS (routes through the phone proxy) and returns a standard `Response`. Note the **local** date formatter (UTC would roll evening US games to the next day).

  ```js
  // src/embeddedjs/scores.js (continued — after the normalisers)

  const SCHEDULE = "https://statsapi.mlb.com/api/v1/schedule";
  const GAME     = "https://statsapi.mlb.com/api/v1/game";

  // Local calendar date (NOT UTC) — matches how a user perceives "today".
  function isoDate(d) {
    const y = d.getFullYear();
    const m = String(d.getMonth() + 1).padStart(2, "0");
    const day = String(d.getDate()).padStart(2, "0");
    return `${y}-${m}-${day}`;
  }

  export async function fetchTodayGame(teamId) {
    const today = isoDate(new Date());
    const res   = await fetch(`${SCHEDULE}?sportId=1&teamId=${teamId}&date=${today}&hydrate=team,linescore`);
    const data  = await res.json();
    const state = normalizeScheduleResponse(data, teamId);
    if (!state) {
      // No game today — look up the next game in the next 14 days.
      const tomorrow = isoDate(new Date(Date.now() +  1 * 86400000));
      const end      = isoDate(new Date(Date.now() + 14 * 86400000));
      const nextRes  = await fetch(`${SCHEDULE}?sportId=1&teamId=${teamId}&startDate=${tomorrow}&endDate=${end}&hydrate=team`);
      const nextData = await nextRes.json();
      const next     = normalizeNextGame(nextData, teamId);
      return {
        teamId, teamAbbrev: null, teamName: null,
        status: "off_day",
        opponent: null, isHome: null, myScore: null, theirScore: null,
        inning: null, inningHalf: null, outs: null, balls: null, strikes: null,
        first: false, second: false, third: false,
        gameTime: null, gameDate: null,
        nextOpponent: next?.nextOpponent ?? null,
        nextDate:     next?.nextDate     ?? null,
        nextTime:     next?.nextTime     ?? null,
        gamePk: null, lastUpdated: Date.now(),
      };
    }
    return state;
  }

  export async function fetchLiveGame(gamePk, existing) {
    const res  = await fetch(`${GAME}/${gamePk}/linescore`);
    const data = await res.json();
    return normalizeLinescore(data, existing);
  }

  export async function refreshSchedule(teamId, existing) {
    const today = isoDate(new Date());
    const res   = await fetch(`${SCHEDULE}?sportId=1&teamId=${teamId}&date=${today}&hydrate=team,linescore`);
    const data  = await res.json();
    const state = normalizeScheduleResponse(data, teamId);
    if (!state) return existing; // schedule call returned no game -> keep prior state
    return state;
  }
  ```

- [ ] **Step 2: Create scores.stub.js**

  Swap into `main.js` imports during display development to exercise all six UI states without a phone or live game.

  ```js
  // src/embeddedjs/scores.stub.js
  // Swap "scores" -> "scores.stub" in main.js imports to test all UI states in the emulator.

  const STUBS = [
    { teamId:147, teamAbbrev:"NYY", teamName:"Yankees", status:"live",
      opponent:"BOS", isHome:false, myScore:4, theirScore:2,
      inning:7, inningHalf:"Bot", outs:2, balls:3, strikes:1,
      first:true, second:false, third:true,
      gameTime:null, gameDate:null, nextOpponent:null, nextDate:null, nextTime:null,
      gamePk:717465, lastUpdated:Date.now() },
    { teamId:119, teamAbbrev:"LAD", teamName:"Dodgers", status:"scheduled",
      opponent:"SF", isHome:true, myScore:0, theirScore:0,
      inning:null, inningHalf:null, outs:null, balls:null, strikes:null,
      first:false, second:false, third:false,
      gameTime:"7:10 PM", gameDate:"Today",
      nextOpponent:null, nextDate:null, nextTime:null,
      gamePk:717466, lastUpdated:Date.now() },
    { teamId:112, teamAbbrev:"CHC", teamName:"Cubs", status:"final",
      opponent:"STL", isHome:false, myScore:3, theirScore:5,
      inning:9, inningHalf:"Bot", outs:3, balls:null, strikes:null,
      first:false, second:false, third:false,
      gameTime:null, gameDate:null, nextOpponent:null, nextDate:null, nextTime:null,
      gamePk:717467, lastUpdated:Date.now() },
    { teamId:117, teamAbbrev:"HOU", teamName:"Astros", status:"off_day",
      opponent:null, isHome:null, myScore:null, theirScore:null,
      inning:null, inningHalf:null, outs:null, balls:null, strikes:null,
      first:false, second:false, third:false,
      gameTime:null, gameDate:null,
      nextOpponent:"TEX", nextDate:"Thu Jun 4", nextTime:"8:10 PM",
      gamePk:null, lastUpdated:Date.now() },
    { teamId:138, teamAbbrev:"STL", teamName:"Cardinals", status:"postponed",
      opponent:"MIL", isHome:true, myScore:null, theirScore:null,
      inning:null, inningHalf:null, outs:null, balls:null, strikes:null,
      first:false, second:false, third:false,
      gameTime:null, gameDate:null, nextOpponent:null, nextDate:"Jun 5", nextTime:"2:15 PM",
      gamePk:null, lastUpdated:Date.now() },
    { teamId:137, teamAbbrev:"SF", teamName:"Giants", status:"error",
      opponent:null, isHome:null, myScore:null, theirScore:null,
      inning:null, inningHalf:null, outs:null, balls:null, strikes:null,
      first:false, second:false, third:false,
      gameTime:null, gameDate:null, nextOpponent:null, nextDate:null, nextTime:null,
      gamePk:null, lastUpdated:Date.now() - 6 * 60 * 1000 }, // 6 min stale -> no-connection overlay
  ];

  export { STUBS };

  let stubIndex = 0;
  export async function fetchTodayGame(teamId) {
    const stub = STUBS[stubIndex % STUBS.length];
    stubIndex++;
    return stub;
  }
  export async function fetchLiveGame(gamePk, existing) { return existing; }
  export async function refreshSchedule(teamId, existing) { return existing; }
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add src/embeddedjs/scores.js src/embeddedjs/scores.stub.js
  git commit -m "feat: add scores fetch functions (hydrate=team,linescore; local date) and test stub"
  ```

---

## Task 5: display.js — live state + bases diamond

**Files:** Create `src/embeddedjs/display.js`

Builds the most complex state (live) first; the rest follow the same pattern in Task 6. Confirm Emery dimensions via `screen.width`/`screen.height` in the emulator. Built-in fonts: `"Gothic 14"`, `"Gothic 18"`, `"Gothic 24 Bold"`, `"Gothic 28 Bold"`, `"Bitham 42 Bold"`.

- [ ] **Step 1: Scaffold and common styles**

  ```js
  // src/embeddedjs/display.js
  import { Application, Container, Label, Content, Row, Column, Skin, Style } from "piu/MC";

  // ---- Colours (Emery is colour e-paper) ----
  const C_BG    = "#F5F0E8";
  const C_BLACK = "#111111";
  const C_MED   = "#444444";
  const C_DIM   = "#888888";
  const C_AMBER = "#E8A000";  // runner on base
  const C_EMPTY = "#CCCCCC";  // empty base
  const C_RED   = "#CC0000";  // stale banner

  // ---- Styles ----
  const sTime    = new Style({ font: "Gothic 18",      color: C_BLACK, horizontal: "center" });
  const sScore   = new Style({ font: "Gothic 28 Bold", color: C_BLACK, horizontal: "center" });
  const sScoreD  = new Style({ font: "Gothic 28 Bold", color: C_DIM,   horizontal: "center" });
  const sTeam    = new Style({ font: "Gothic 14",      color: C_MED,   horizontal: "center" });
  const sTeamD   = new Style({ font: "Gothic 14",      color: C_DIM,   horizontal: "center" });
  const sInfo    = new Style({ font: "Gothic 14",      color: C_MED,   horizontal: "center" });
  const sSmall   = new Style({ font: "Gothic 14",      color: C_DIM,   horizontal: "center" });
  const sBadge   = new Style({ font: "Gothic 14",      color: "white", horizontal: "center" });
  const sHeading = new Style({ font: "Gothic 18",      color: C_BLACK, horizontal: "center" });

  // ---- Skins ----
  const skinBg    = new Skin({ fill: C_BG });
  const skinRed   = new Skin({ fill: C_RED });
  const skinBlack = new Skin({ fill: C_BLACK });
  const skinDim   = new Skin({ fill: "#DDDDDD" });

  let application = null;   // the Piu Application (default-exported from main.js)
  let mainContainer = null;

  function formatWatchTime() {
    const d = new Date();
    let h = d.getHours(), m = d.getMinutes();
    const ampm = h >= 12 ? "PM" : "AM";
    h = h % 12 || 12;
    return `${h}:${m.toString().padStart(2, "0")} ${ampm}`;
  }
  ```

- [ ] **Step 2: Bases diamond helper**

  ```js
  // src/embeddedjs/display.js (continued)
  function base(x, y, sz, filled) {
    return new Content(null, { left: x, top: y, width: sz, height: sz,
      skin: new Skin({ fill: filled ? C_AMBER : C_EMPTY }) });
  }

  function buildBasesDiamond(gs) {
    const SZ = 8, cx = 14, cy = 10;
    return new Container(null, {
      width: 36, height: 28,
      contents: [
        base(cx,     0,      SZ, gs.second), // 2nd — top
        base(cx * 2, cy,     SZ, gs.first),  // 1st — right
        base(0,      cy,     SZ, gs.third),  // 3rd — left
        base(cx,     cy * 2, SZ, false),     // home — always grey
      ],
    });
  }
  ```

- [ ] **Step 3: Cycle-dots helper**

  ```js
  // src/embeddedjs/display.js (continued)
  function buildCycleDots(currentSlot, totalSlots) {
    const DOT = 6, GAP = 4, dots = [];
    for (let i = 0; i < totalSlots; i++) {
      dots.push(new Content(null, { width: DOT, height: DOT, left: i * (DOT + GAP),
        skin: new Skin({ fill: i === currentSlot ? C_BLACK : C_DIM }) }));
    }
    return new Container(null, { width: totalSlots * (DOT + GAP) - GAP, height: DOT, contents: dots });
  }
  ```

- [ ] **Step 4: buildLiveContent**

  ```js
  // src/embeddedjs/display.js (continued)
  function buildLiveContent(gs, slot, total) {
    return new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Row(null, { top: 4, width: screen.width, contents: [
          new Column(null, { width: screen.width * 0.4, contents: [
            new Label(null, { string: gs.teamAbbrev, style: sTeam }),
            new Label(null, { string: String(gs.myScore), style: sScore }),
          ]}),
          new Label(null, { string: "–", style: sScore }),
          new Column(null, { width: screen.width * 0.4, contents: [
            new Label(null, { string: gs.opponent, style: sTeam }),
            new Label(null, { string: String(gs.theirScore), style: sScore }),
          ]}),
        ]}),
        new Label(null, { top: 2,
          string: `${gs.inningHalf === "Bot" ? "▼" : "▲"} ${gs.inningHalf} ${gs.inning}  •  ${gs.outs} out${gs.outs !== 1 ? "s" : ""}`,
          style: sInfo }),
        buildBasesDiamond(gs),
        new Label(null, { top: 2, string: `B:${gs.balls}  S:${gs.strikes}`, style: sSmall }),
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }
  ```

- [ ] **Step 5: Mount and verify the live state**

  Add the public mount helper (used by all states):

  ```js
  // src/embeddedjs/display.js (continued)
  export function createWatchface() {
    mainContainer = new Container(null, { width: screen.width, height: screen.height, skin: skinBg });
    application = new Application(null, { skin: skinBg, contents: [mainContainer] });
    return application;   // main.js default-exports this so the launcher mounts it as root
  }
  ```

  Temporary test in `main.js` (entry module must default-export the Application):

  ```js
  import { createWatchface, updateDisplay } from "display";
  import { STUBS } from "scores.stub";
  const app = createWatchface();
  updateDisplay(STUBS[0], 0, 3);   // updateDisplay added in Task 6 — stub it returning live content for now
  export default app;
  ```

  Run in the Emery emulator. Verify: time, large score, inning/outs line, amber squares on 1st + 3rd, `B:3 S:1`, 3 dots with the first filled. Tune fonts/spacing to Emery's resolution. If nothing renders, check the "Piu watchface mount" note at the top before continuing.

- [ ] **Step 6: Commit**

  ```bash
  git add src/embeddedjs/display.js src/embeddedjs/main.js
  git commit -m "feat: add display live state + bases diamond"
  ```

---

## Task 6: display.js — remaining states + public API

**Files:** Modify `src/embeddedjs/display.js`

- [ ] **Step 1: buildScheduledContent**

  ```js
  function buildScheduledContent(gs, slot, total) {
    return new Column(null, { width: screen.width, height: screen.height, skin: skinBg, contents: [
      new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
      new Container(null, { top: 8, width: screen.width, height: 1, skin: skinDim }),
      new Label(null, { top: 6, string: gs.teamName ?? gs.teamAbbrev, style: sHeading }),
      new Container(null, { top: 6, width: screen.width, height: 1, skin: skinDim }),
      new Label(null, { top: 8, string: `vs ${gs.opponent}  •  ${gs.gameDate}`, style: sInfo }),
      new Label(null, { top: 2, string: gs.gameTime, style: sScore }),
      new Label(null, { top: 4, string: "game not yet started", style: sSmall }),
      new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
    ]});
  }
  ```

- [ ] **Step 2: buildFinalContent**

  ```js
  function buildFinalContent(gs, slot, total) {
    const won = gs.myScore > gs.theirScore;
    return new Column(null, { width: screen.width, height: screen.height, skin: skinBg, contents: [
      new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
      new Row(null, { top: 8, width: screen.width, contents: [
        new Column(null, { width: screen.width * 0.4, contents: [
          new Label(null, { string: gs.teamAbbrev, style: sTeamD }),
          new Label(null, { string: String(gs.myScore), style: sScoreD }),
        ]}),
        new Label(null, { string: "–", style: sScoreD }),
        new Column(null, { width: screen.width * 0.4, contents: [
          new Label(null, { string: gs.opponent, style: sTeamD }),
          new Label(null, { string: String(gs.theirScore), style: sScoreD }),
        ]}),
      ]}),
      new Container(null, { top: 8, height: 18, width: 60, skin: skinBlack,
        contents: [new Label(null, { string: "FINAL", style: sBadge })] }),
      new Label(null, { top: 4, string: `${won ? gs.teamAbbrev : gs.opponent} wins`, style: sSmall }),
      new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
    ]});
  }
  ```

- [ ] **Step 3: buildOffDayContent**

  ```js
  function buildOffDayContent(gs, slot, total) {
    return new Column(null, { width: screen.width, height: screen.height, skin: skinBg, contents: [
      new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
      new Container(null, { top: 8, width: screen.width, height: 1, skin: skinDim }),
      new Label(null, { top: 6, string: gs.teamName ?? gs.teamAbbrev ?? "—", style: sHeading }),
      new Container(null, { top: 6, width: screen.width, height: 1, skin: skinDim }),
      new Label(null, { top: 8, string: "next game", style: sSmall }),
      new Label(null, { top: 2, string: `vs ${gs.nextOpponent ?? "TBD"}`, style: sInfo }),
      new Label(null, { top: 2, string: gs.nextDate ?? "", style: sHeading }),
      new Label(null, { top: 2, string: gs.nextTime ?? "", style: sInfo }),
      new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
    ]});
  }
  ```

  > Note: `teamName`/`teamAbbrev` are `null` for off-day (the schedule call returned no game). If you want the team name shown on an off day, carry it forward from the last known `GameState` in `main.js` rather than from the off-day fetch.

- [ ] **Step 4: buildPostponedContent**

  ```js
  function buildPostponedContent(gs, slot, total) {
    return new Column(null, { width: screen.width, height: screen.height, skin: skinBg, contents: [
      new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
      new Label(null, { top: 12, string: gs.teamAbbrev ?? "—", style: sHeading }),
      new Container(null, { top: 8, height: 18, width: 46, skin: skinBlack,
        contents: [new Label(null, { string: "PPD", style: sBadge })] }),
      new Label(null, { top: 6, string: gs.nextDate ? `Rescheduled: ${gs.nextDate}` : "Rescheduled TBD", style: sSmall }),
      new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
    ]});
  }
  ```

- [ ] **Step 5: buildErrorContent**

  ```js
  function buildErrorContent(gs, slot, total) {
    return new Column(null, { width: screen.width, height: screen.height, skin: skinBg, contents: [
      new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
      new Label(null, { top: 12, string: gs.teamAbbrev ?? "—", style: sHeading }),
      new Label(null, { top: 8, string: "Data unavailable", style: sSmall }),
      new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
    ]});
  }
  ```

- [ ] **Step 6: buildConnectingContent + buildContent (with stale overlay)**

  ```js
  const STALE_THRESHOLD_MS = 5 * 60 * 1000;

  function buildConnectingContent(slot, total) {
    return new Column(null, { width: screen.width, height: screen.height, skin: skinBg, contents: [
      new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
      new Label(null, { top: 20, string: "Connecting…", style: sInfo }),
      new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
    ]});
  }

  function buildContent(gs, slot, total) {
    let inner;
    if (!gs) {
      inner = buildConnectingContent(slot, total);
    } else {
      switch (gs.status) {
        case "live":      inner = buildLiveContent(gs, slot, total); break;
        case "scheduled": inner = buildScheduledContent(gs, slot, total); break;
        case "final":     inner = buildFinalContent(gs, slot, total); break;
        case "off_day":   inner = buildOffDayContent(gs, slot, total); break;
        case "postponed": inner = buildPostponedContent(gs, slot, total); break;
        default:          inner = buildErrorContent(gs, slot, total); break;
      }
    }

    const isStale = gs && (Date.now() - gs.lastUpdated > STALE_THRESHOLD_MS);
    if (!isStale) return inner;

    const ageMin = Math.floor((Date.now() - gs.lastUpdated) / 60000);
    return new Container(null, { width: screen.width, height: screen.height, contents: [
      inner,
      new Container(null, { bottom: 18, width: screen.width, height: 18, skin: skinRed,
        contents: [new Label(null, { string: `⚠ last update: ${ageMin}m ago`, style: sBadge })] }),
    ]});
  }
  ```

- [ ] **Step 7: Public API**

  ```js
  export function updateDisplay(gs, slot, total) {
    if (!mainContainer) return;
    mainContainer.empty();
    mainContainer.add(buildContent(gs, slot, total));
  }

  export function renderUnconfigured() {
    if (!mainContainer) return;
    mainContainer.empty();
    mainContainer.add(new Column(null, { width: screen.width, height: screen.height, skin: skinBg, contents: [
      new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
      new Label(null, { top: 20, string: "Open settings", style: sInfo }),
      new Label(null, { top: 4, string: "to pick a team", style: sSmall }),
    ]}));
  }
  ```

  (`createWatchface` from Task 5 Step 5 stays as the single Application factory.)

- [ ] **Step 8: Verify all states via scores.stub.js**

  Temporary `main.js`:

  ```js
  import { createWatchface, updateDisplay } from "display";
  import { fetchTodayGame } from "scores.stub";
  const app = createWatchface();
  let i = 0;
  setInterval(() => { fetchTodayGame(i).then(gs => updateDisplay(gs, i % 3, 3)); i++; }, 3000);
  export default app;
  ```

  Run; watch each state cycle every 3s: live (score/bases/count), scheduled (name/time), final (dimmed + FINAL), off-day (next game), postponed (PPD), error/stale (⚠ banner — the stub is 6 min stale). Check long names ("Cardinals", "Diamondbacks") don't clip.

- [ ] **Step 9: Commit**

  ```bash
  git add src/embeddedjs/display.js src/embeddedjs/main.js
  git commit -m "feat: add all display states + no-connection overlay"
  ```

---

## Task 7: clay-config.json

**Files:** Create `src/pkjs/clay-config.json` (required by `src/pkjs/index.js`)

- [ ] **Step 1: Create the Clay config with all 30 teams**

  Lives in `src/pkjs/` so `pkjs/index.js` can `require("./clay-config.json")`. Values are official MLB team IDs. Replace `"__SAME_AS_TEAM1__"` in team2/team3 with the same options array as team1 (Clay does not support shared option references).

  ```json
  [
    { "type": "heading", "defaultValue": "Baseball Scores", "size": 1 },
    { "type": "section", "items": [
      { "type": "text", "defaultValue": "Teams to track" },
      { "type": "select", "messageKey": "team1", "label": "Team 1", "defaultValue": "0",
        "options": [
          { "label": "— None —",             "value": "0"   },
          { "label": "Arizona Diamondbacks", "value": "109" },
          { "label": "Athletics",            "value": "133" },
          { "label": "Atlanta Braves",       "value": "144" },
          { "label": "Baltimore Orioles",    "value": "110" },
          { "label": "Boston Red Sox",       "value": "111" },
          { "label": "Chicago Cubs",         "value": "112" },
          { "label": "Chicago White Sox",    "value": "145" },
          { "label": "Cincinnati Reds",      "value": "113" },
          { "label": "Cleveland Guardians",  "value": "114" },
          { "label": "Colorado Rockies",     "value": "115" },
          { "label": "Detroit Tigers",       "value": "116" },
          { "label": "Houston Astros",       "value": "117" },
          { "label": "Kansas City Royals",   "value": "118" },
          { "label": "Los Angeles Angels",   "value": "108" },
          { "label": "Los Angeles Dodgers",  "value": "119" },
          { "label": "Miami Marlins",        "value": "146" },
          { "label": "Milwaukee Brewers",    "value": "158" },
          { "label": "Minnesota Twins",      "value": "142" },
          { "label": "New York Mets",        "value": "121" },
          { "label": "New York Yankees",     "value": "147" },
          { "label": "Philadelphia Phillies","value": "143" },
          { "label": "Pittsburgh Pirates",   "value": "134" },
          { "label": "San Diego Padres",     "value": "135" },
          { "label": "San Francisco Giants", "value": "137" },
          { "label": "Seattle Mariners",     "value": "136" },
          { "label": "St. Louis Cardinals",  "value": "138" },
          { "label": "Tampa Bay Rays",       "value": "139" },
          { "label": "Texas Rangers",        "value": "140" },
          { "label": "Toronto Blue Jays",    "value": "141" },
          { "label": "Washington Nationals", "value": "120" }
        ] },
      { "type": "select", "messageKey": "team2", "label": "Team 2", "defaultValue": "0", "options": "__SAME_AS_TEAM1__" },
      { "type": "select", "messageKey": "team3", "label": "Team 3", "defaultValue": "0", "options": "__SAME_AS_TEAM1__" }
    ] },
    { "type": "section", "items": [
      { "type": "text", "defaultValue": "Display" },
      { "type": "range", "messageKey": "cycleInterval", "label": "Cycle interval (seconds)", "defaultValue": 15, "min": 5,  "max": 60,  "step": 5  },
      { "type": "range", "messageKey": "pollInterval",  "label": "Refresh interval (seconds)", "defaultValue": 60, "min": 30, "max": 300, "step": 30 }
    ] },
    { "type": "text", "defaultValue": "Refresh only applies to live games. Scheduled and off-day slots update every 5 min automatically." }
  ]
  ```

  > The Athletics relocated; team ID `133` is used here for the franchise (label "Athletics"). **Verify against** `https://statsapi.mlb.com/api/v1/teams?sportId=1` before release — if the ID changed, update it.

- [ ] **Step 2: Confirm package.json wiring (from Task 1 Step 4)**

  `capabilities` includes `"configurable"`, `enableMultiJS` is `true`, and `messageKeys` lists all five keys. Clay is initialised in `src/pkjs/index.js` (Task 1 Step 6). No separate `clay` key is needed in `package.json` — Clay is wired in PKJS code.

- [ ] **Step 3: Verify the config page opens on device**

  Install on the Pebble. Phone app → My Watchfaces → Baseball Scores → Settings (gear). Confirm 3 team dropdowns + 2 sliders render. Pick Team 1 and Save.

- [ ] **Step 4: Commit**

  ```bash
  git add src/pkjs/clay-config.json package.json
  git commit -m "feat: add Clay config (30 teams + display sliders) in pkjs"
  ```

---

## Task 8: main.js — orchestration

**Files:** Modify `src/embeddedjs/main.js`

- [ ] **Step 1: Write main.js**

  ```js
  // src/embeddedjs/main.js — watch entry point.
  // setInterval/clearInterval are globals (provided by manifest_mod.json) — no timer import.
  import Message from "pebble/message";
  import { readSettings, writeSettings, teamIds } from "settings";
  import { fetchTodayGame, fetchLiveGame, refreshSchedule } from "scores";
  import { createWatchface, updateDisplay, renderUnconfigured } from "display";

  let settings   = readSettings();
  let configured = teamIds(settings);
  let states     = [];
  let currentSlot = 0;
  let cycleTimer = null, pollTimer = null, scheduleTimer = null;
  let connectedBound = false;

  const app = createWatchface();   // Application instance — default-exported below

  // Keep the clock fresh every minute, independent of cycle/fetch.
  watch.addEventListener("minutechange", () => {
    if (configured.length) updateDisplay(states[currentSlot], currentSlot, configured.length);
  });

  // Receive Clay settings saved on the phone (AppMessage -> Message).
  const configMsg = new Message({
    keys: ["team1", "team2", "team3", "cycleInterval", "pollInterval"],
    onReadable() {
      const msg = this.read();
      const incoming = {};
      msg.forEach((value, key) => { incoming[key] = value; });
      writeSettings(incoming);
      applySettings();             // hot-reload — no System.restart needed
    },
  });

  if (configured.length === 0) renderUnconfigured();
  else start();

  function start() {
    states = new Array(configured.length).fill(null);
    currentSlot = 0;
    updateDisplay(null, currentSlot, configured.length); // "Connecting…"

    if (watch.connected && watch.connected.pebblekit) {
      fetchAllSchedules();
    } else if (!connectedBound) {
      watch.addEventListener("connected", () => fetchAllSchedules());
      connectedBound = true;
    }
    startTimers();
  }

  function startTimers() {
    stopTimers();
    cycleTimer = setInterval(() => {
      currentSlot = (currentSlot + 1) % configured.length;
      updateDisplay(states[currentSlot], currentSlot, configured.length);
    }, settings.cycleInterval * 1000);

    pollTimer = setInterval(() => {
      for (let i = 0; i < configured.length; i++) {
        const gs = states[i];
        if (gs && gs.status === "live" && gs.gamePk) {
          fetchLiveGame(gs.gamePk, gs)
            .then(u => { states[i] = u; if (i === currentSlot) updateDisplay(u, currentSlot, configured.length); })
            .catch(() => {}); // leave state; lastUpdated unchanged -> stale overlay appears
        }
      }
    }, settings.pollInterval * 1000);

    scheduleTimer = setInterval(() => {
      for (let i = 0; i < configured.length; i++) {
        refreshSchedule(configured[i], states[i])
          .then(u => { states[i] = u; if (i === currentSlot) updateDisplay(u, currentSlot, configured.length); })
          .catch(() => {});
      }
    }, 5 * 60 * 1000);
  }

  function stopTimers() {
    if (cycleTimer)    clearInterval(cycleTimer);
    if (pollTimer)     clearInterval(pollTimer);
    if (scheduleTimer) clearInterval(scheduleTimer);
    cycleTimer = pollTimer = scheduleTimer = null;
  }

  function applySettings() {
    settings   = readSettings();
    configured = teamIds(settings);
    if (configured.length === 0) { stopTimers(); renderUnconfigured(); return; }
    start();   // re-init states + timers + re-fetch with the new teams/intervals
  }

  async function fetchAllSchedules() {
    // Sequential (not Promise.all) to avoid bursting 3 requests at launch.
    for (let i = 0; i < configured.length; i++) {
      try { states[i] = await fetchTodayGame(configured[i]); }
      catch { /* leave null -> connecting/stale */ }
      if (i === currentSlot) updateDisplay(states[i], currentSlot, configured.length); // respect current slot
    }
  }

  export default app;   // the launcher mounts the entry module's default export
  ```

- [ ] **Step 2: Restore the real scores import**

  Ensure `main.js` imports from `"scores"`, not `"scores.stub"`. The stub is only for Task 5/6 display work.

- [ ] **Step 3: Verify timers + readiness in the emulator**

  Temporary traces inside `startTimers()`:

  ```js
  console.log(`timers: cycle=${settings.cycleInterval}s poll=${settings.pollInterval}s schedule=300s`);
  ```

  Run; confirm the line logs once and that changing settings re-logs it (proving hot-reload via `applySettings` → `startTimers` → `stopTimers`). Remove the trace after verifying.

- [ ] **Step 4: Commit**

  ```bash
  git add src/embeddedjs/main.js
  git commit -m "feat: orchestration — 3x setInterval, watch readiness, Clay Message, minutechange, hot-reload"
  ```

---

## Task 9: Real device integration test

**Files:** None — verification only.

- [ ] **Step 1: Build and install**

  CloudPebble: Run → Install on Pebble. Local SDK: `pebble build && pebble install --phone <phone-ip>`.

- [ ] **Step 2: Proxy connection** — Watchface loads, shows "Connecting…", resolves to the first team once `watch.connected.pebblekit` is true. Check the phone/CloudPebble console for fetch errors.

- [ ] **Step 3: Clay saves + hot-reload** — Settings → pick Team 1 → Save. The watch should pick up the new team **without restarting** (via the `Message` handler → `applySettings`). Reopen the watchface and confirm the choice persisted (localStorage).

- [ ] **Step 4: Cycle timer** — With 2+ teams, confirm auto-cycle at the configured interval with no stutter while a fetch is in flight.

- [ ] **Step 5: Stale overlay** — Disable phone connectivity (or leave BT range). Within 5 min the ⚠ banner shows the age; re-enable and confirm it clears on the next successful poll.

- [ ] **Step 6: Live game (in season)** — Track a team with a game in progress. Confirm score/inning/outs/bases/count update at the poll interval; drop the poll interval to 30s and confirm faster updates.

- [ ] **Step 7: Final commit + push**

  ```bash
  git add -A
  git commit -m "feat: complete pebble-baseball watchface v1"
  git push
  ```

---

## Self-review notes

- **Status integrity:** `normalizeLinescore` never changes `status` and preserves `inningHalf` when the linescore omits it — only `refreshSchedule` (schedule timer) flips game state, so a stale/partial linescore can't revert a finished game to "live" or flip the half-inning.
- **Cycle smoothness:** the cycle timer reads from `states[]` and never awaits a fetch; fetch results only repaint when `i === currentSlot`. Initial load (`fetchAllSchedules`) also respects `currentSlot`, so the screen no longer flickers through every team at launch.
- **Settings without restart:** the three `setInterval` ids are torn down (`clearInterval`) and rebuilt in `applySettings`, so changing teams or intervals applies live. `System.restart()` (unverified in Alloy) is deliberately not used.
- **Clock:** `minutechange` keeps the displayed time current even when no cycle/fetch fires.
- **Network shape:** `hydrate=team,linescore` is required for abbreviations; bases come from `linescore.offense.first/second/third`; the standalone linescore supplies `teams.home/away.runs` + count.
- **Verify-before-build flags:** the three items in the "Still flagged" box (Clay package name, Piu Application mount, `screen` dimensions) are the only assumptions not fully confirmed from docs — resolve each at its first emulator checkpoint (Tasks 1, 5).
