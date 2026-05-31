# pebble-baseball Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an Alloy watchface for Pebble Time 2 that auto-cycles through up to 3 user-configured teams showing live baseball scores, bases, and pitch count.

**Architecture:** A self-contained Alloy project with four source files (`index.js`, `scores.js`, `display.js`, `settings.js`) and a Clay config. `scores.js` fetches and normalises MLB Stats API responses into a `GameState` object; `display.js` renders that object using Piu; `index.js` orchestrates three independent timers and the `@moddable/proxy` network bridge.

**Tech Stack:** Alloy (Moddable SDK, ES2025), Piu UI framework, `@moddable/proxy` for HTTP via phone, Clay for settings UI, MLB Stats API (free, no auth).

---

## File Map

| File | Role |
|---|---|
| `package.json` | Alloy manifest — platform target (emery), watchface flag, proxy dependency |
| `src/index.js` | Entry point — proxy setup, three timers, state array, wires scores → display |
| `src/scores.js` | All API calls + `GameState` normalisation. Only file that may reference the upstream API in comments. |
| `src/display.js` | Piu layout — renders any `GameState` to screen. No fetch logic. |
| `src/settings.js` | `localStorage` read/write with typed defaults. |
| `src/scores.stub.js` | Hardcoded `GameState` array covering all 6 states. Swap in for `scores.js` imports in `index.js` during emulator testing. |
| `config/clay-config.json` | Clay UI definition — 3 team dropdowns, 2 sliders. |

---

## Reference

- Alloy guides: https://developer.repebble.com/guides/alloy/
- Networking (proxy setup): https://developer.repebble.com/guides/alloy/networking/
- App Configuration (Clay): https://developer.repebble.com/guides/user-interfaces/app-configuration/
- Storage: https://developer.repebble.com/guides/alloy/storage/
- Alloy watchface tutorial (follow this first): https://developer.repebble.com/tutorials/alloy-watchface-tutorial/part1/

---

## Task 1: Project scaffold

**Files:**
- Create: `package.json`
- Create: `src/index.js` (empty entry point)

- [ ] **Step 1: Create a Rebble account**

  Go to https://rebble.io and sign up for a free account. This is required to use CloudPebble and to sideload to the device.

- [ ] **Step 2: Open CloudPebble and create the project**

  Go to https://cloudpebble.net, log in with your Rebble account. Click "Create Project":
  - Name: `pebble-baseball`
  - Project type: **Alloy**
  - Template: **Watchface**
  - Platform: **Emery** (Pebble Time 2)

  CloudPebble generates the initial scaffold including `package.json` and a starter `src/index.js`.

- [ ] **Step 3: Add the proxy dependency**

  In CloudPebble's `package.json` editor, add `@moddable/proxy` to dependencies:

  ```json
  {
    "dependencies": {
      "@moddable/proxy": "*"
    }
  }
  ```

- [ ] **Step 4: Verify the emulator launches**

  Click **Run** in CloudPebble. The Emery emulator should open and show a blank watchface (or the default template). No errors in the console.

- [ ] **Step 5: Export project and sync with local git repo**

  In CloudPebble: Settings → Export. This downloads a `.zip`. Extract into `/Users/jimmytimmons/Projects/pebble-baseball/`, replacing the scaffold files.

  ```bash
  cd /Users/jimmytimmons/Projects/pebble-baseball
  git add -A
  git commit -m "chore: scaffold Alloy watchface project"
  git push
  ```

---

## Task 2: settings.js

**Files:**
- Create: `src/settings.js`

- [ ] **Step 1: Write settings.js**

  ```js
  // src/settings.js
  const DEFAULTS = {
    team1: 0,
    team2: 0,
    team3: 0,
    cycleInterval: 15,
    pollInterval: 60,
  };

  export function readSettings() {
    return {
      team1:         parseInt(localStorage.getItem("team1")         ?? DEFAULTS.team1),
      team2:         parseInt(localStorage.getItem("team2")         ?? DEFAULTS.team2),
      team3:         parseInt(localStorage.getItem("team3")         ?? DEFAULTS.team3),
      cycleInterval: parseInt(localStorage.getItem("cycleInterval") ?? DEFAULTS.cycleInterval),
      pollInterval:  Math.max(30, parseInt(localStorage.getItem("pollInterval") ?? DEFAULTS.pollInterval)),
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

- [ ] **Step 2: Verify defaults in emulator**

  In `src/index.js`, temporarily add:

  ```js
  import { readSettings } from "settings";
  const s = readSettings();
  trace(`settings: ${JSON.stringify(s)}\n`);
  ```

  Run in emulator. Console should log:
  ```
  settings: {"team1":0,"team2":0,"team3":0,"cycleInterval":15,"pollInterval":60}
  ```

  Remove the `trace` line after verifying.

- [ ] **Step 3: Commit**

  ```bash
  git add src/settings.js src/index.js
  git commit -m "feat: add settings localStorage helpers"
  ```

---

## Task 3: scores.js — normalisation functions

**Files:**
- Create: `src/scores.js` (normalisation functions only — fetch functions added in Task 4)

The normalisation functions are pure (no Pebble APIs). Verify them by tracing output with fixture data.

- [ ] **Step 1: Write the normalisation helpers**

  ```js
  // src/scores.js
  // MLB Stats API — https://statsapi.mlb.com
  // This is the only file that references the upstream API by name.

  // --- Helpers ---

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
    const days  = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"];
    const months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
    return `${days[d.getDay()]} ${months[d.getMonth()]} ${d.getDate()}`;
  }

  function mapStatus(detailedState) {
    if (!detailedState) return "error";
    const s = detailedState.toLowerCase();
    if (s.includes("progress") || s.includes("live")) return "live";
    if (s.includes("final") || s.includes("game over") || s.includes("completed")) return "final";
    if (s.includes("postponed") || s.includes("suspended")) return "postponed";
    return "scheduled"; // covers "Scheduled", "Pre-Game", "Warmup", "Delayed"
  }
  ```

- [ ] **Step 2: Write normalizeScheduleResponse**

  ```js
  // src/scores.js (continued)

  export function normalizeScheduleResponse(data, teamId) {
    const game = data?.dates?.[0]?.games?.[0];
    if (!game) return null;

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
      inningHalf:   ls?.inningHalf === "Bottom" ? "Bot" : "Top",
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

  ```js
  // src/scores.js (continued)

  // Updates only live-game fields. Does NOT change status — only the schedule
  // timer (which calls normalizeScheduleResponse) may change status.
  export function normalizeLinescore(data, existing) {
    if (!data || !existing) return existing;
    return {
      ...existing,
      myScore:    existing.isHome ? (data.teams?.home?.runs ?? existing.myScore)
                                  : (data.teams?.away?.runs ?? existing.myScore),
      theirScore: existing.isHome ? (data.teams?.away?.runs ?? existing.theirScore)
                                  : (data.teams?.home?.runs ?? existing.theirScore),
      inning:     data.currentInning ?? existing.inning,
      inningHalf: data.inningHalf === "Bottom" ? "Bot" : "Top",
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
  // src/scores.js (continued)

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

- [ ] **Step 5: Verify normalisation with fixture data in emulator**

  In `src/index.js`, temporarily add:

  ```js
  import { normalizeScheduleResponse, normalizeLinescore } from "scores";

  const liveFixture = {
    dates: [{ games: [{
      gamePk: 717465,
      gameDate: "2026-05-31T23:10:00Z",
      status: { detailedState: "In Progress" },
      teams: {
        away: { team: { id: 147, abbreviation: "NYY", shortName: "Yankees" }, score: 4 },
        home: { team: { id: 111, abbreviation: "BOS", shortName: "Red Sox" }, score: 2 },
      },
      linescore: {
        currentInning: 7, inningHalf: "Bottom", outs: 2, balls: 3, strikes: 1,
        offense: { first: { id: 1 }, third: { id: 2 } },
      },
    }]}],
  };

  const gs = normalizeScheduleResponse(liveFixture, 147);
  trace(`status=${gs.status} myScore=${gs.myScore} theirScore=${gs.theirScore}\n`);
  trace(`inning=${gs.inning} half=${gs.inningHalf} outs=${gs.outs}\n`);
  trace(`first=${gs.first} second=${gs.second} third=${gs.third}\n`);
  ```

  Run in emulator. Expected console output:
  ```
  status=live myScore=4 theirScore=2
  inning=7 half=Bot outs=2
  first=true second=false third=true
  ```

  Remove the fixture code after verifying.

- [ ] **Step 6: Commit**

  ```bash
  git add src/scores.js src/index.js
  git commit -m "feat: add scores normalisation functions"
  ```

---

## Task 4: scores.js — fetch functions + scores.stub.js

**Files:**
- Modify: `src/scores.js` (add fetch functions)
- Create: `src/scores.stub.js`

- [ ] **Step 1: Add fetch functions to scores.js**

  ```js
  // src/scores.js (continued — add after normalisation functions)

  const SCHEDULE = "https://statsapi.mlb.com/api/v1/schedule";
  const GAME     = "https://statsapi.mlb.com/api/v1/game";

  function isoDate(date) {
    return date.toISOString().split("T")[0];
  }

  export async function fetchTodayGame(teamId) {
    const today = isoDate(new Date());
    const res   = await fetch(`${SCHEDULE}?sportId=1&teamId=${teamId}&date=${today}&hydrate=linescore`);
    const data  = await res.json();
    const state = normalizeScheduleResponse(data, teamId);
    if (!state) {
      // No game today — look up next game
      const end     = isoDate(new Date(Date.now() + 14 * 86400000));
      const tomorrow = isoDate(new Date(Date.now() +  1 * 86400000));
      const nextRes  = await fetch(`${SCHEDULE}?sportId=1&teamId=${teamId}&startDate=${tomorrow}&endDate=${end}`);
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
    const res   = await fetch(`${SCHEDULE}?sportId=1&teamId=${teamId}&date=${today}&hydrate=linescore`);
    const data  = await res.json();
    // Preserve nextOpponent/nextDate/nextTime if we transition to off_day
    const state = normalizeScheduleResponse(data, teamId);
    if (!state) return existing; // no change if schedule call fails
    return state;
  }
  ```

- [ ] **Step 2: Create scores.stub.js**

  This is swapped in during emulator testing to exercise all UI states without a phone or live game.

  ```js
  // src/scores.stub.js
  // Swap this for "scores" in index.js imports to test all UI states in the emulator.

  const STUBS = [
    // State 1: live
    {
      teamId: 147, teamAbbrev: "NYY", teamName: "Yankees", status: "live",
      opponent: "BOS", isHome: false, myScore: 4, theirScore: 2,
      inning: 7, inningHalf: "Bot", outs: 2, balls: 3, strikes: 1,
      first: true, second: false, third: true,
      gameTime: null, gameDate: null, nextOpponent: null, nextDate: null, nextTime: null,
      gamePk: 717465, lastUpdated: Date.now(),
    },
    // State 2: scheduled
    {
      teamId: 119, teamAbbrev: "LAD", teamName: "Dodgers", status: "scheduled",
      opponent: "SF", isHome: true, myScore: 0, theirScore: 0,
      inning: null, inningHalf: null, outs: null, balls: null, strikes: null,
      first: false, second: false, third: false,
      gameTime: "7:10 PM", gameDate: "Today",
      nextOpponent: null, nextDate: null, nextTime: null,
      gamePk: 717466, lastUpdated: Date.now(),
    },
    // State 3: final
    {
      teamId: 112, teamAbbrev: "CHC", teamName: "Cubs", status: "final",
      opponent: "STL", isHome: false, myScore: 3, theirScore: 5,
      inning: 9, inningHalf: "Bot", outs: 3, balls: null, strikes: null,
      first: false, second: false, third: false,
      gameTime: null, gameDate: null, nextOpponent: null, nextDate: null, nextTime: null,
      gamePk: 717467, lastUpdated: Date.now(),
    },
    // State 4: off_day
    {
      teamId: 117, teamAbbrev: "HOU", teamName: "Astros", status: "off_day",
      opponent: null, isHome: null, myScore: null, theirScore: null,
      inning: null, inningHalf: null, outs: null, balls: null, strikes: null,
      first: false, second: false, third: false,
      gameTime: null, gameDate: null,
      nextOpponent: "TEX", nextDate: "Thu Jun 4", nextTime: "8:10 PM",
      gamePk: null, lastUpdated: Date.now(),
    },
    // State 5: postponed
    {
      teamId: 138, teamAbbrev: "STL", teamName: "Cardinals", status: "postponed",
      opponent: "MIL", isHome: true, myScore: null, theirScore: null,
      inning: null, inningHalf: null, outs: null, balls: null, strikes: null,
      first: false, second: false, third: false,
      gameTime: null, gameDate: null, nextOpponent: null, nextDate: "Jun 5", nextTime: "2:15 PM",
      gamePk: null, lastUpdated: Date.now(),
    },
    // State 6: error (bad API response)
    {
      teamId: 137, teamAbbrev: "SF", teamName: "Giants", status: "error",
      opponent: null, isHome: null, myScore: null, theirScore: null,
      inning: null, inningHalf: null, outs: null, balls: null, strikes: null,
      first: false, second: false, third: false,
      gameTime: null, gameDate: null, nextOpponent: null, nextDate: null, nextTime: null,
      gamePk: null, lastUpdated: Date.now() - 300000, // 5 min stale — triggers no-connection overlay
    },
  ];

  export { STUBS }; // exported so Task 5 Step 5 can import directly for one-shot display testing

  let stubIndex = 0;

  export async function fetchTodayGame(teamId) {
    const stub = STUBS[stubIndex % STUBS.length];
    stubIndex++;
    return stub;
  }

  export async function fetchLiveGame(gamePk, existing) {
    return existing; // return unchanged in stub
  }

  export async function refreshSchedule(teamId, existing) {
    return existing;
  }
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add src/scores.js src/scores.stub.js
  git commit -m "feat: add scores fetch functions and test stub"
  ```

---

## Task 5: display.js — live state + bases diamond

**Files:**
- Create: `src/display.js`

This task builds the most complex state (live). All other states follow the same pattern in Task 6.

Piu reference: https://developer.repebble.com/docs/  
Check `screen.width` and `screen.height` for the actual Emery display dimensions.

- [ ] **Step 1: Write the display scaffold and common styles**

  ```js
  // src/display.js
  import { Application, Container, Label, Content, Row, Column, Skin, Style, Port } from "piu/MC";

  // ---- Colours ----
  const C_BG      = "#F5F0E8";  // e-paper warm white
  const C_BLACK   = "#111111";
  const C_MED     = "#444444";
  const C_DIM     = "#888888";
  const C_AMBER   = "#E8A000";  // runner on base
  const C_EMPTY   = "#CCCCCC";  // empty base
  const C_RED     = "#CC0000";  // stale banner
  const C_HOME    = "#999999";  // home plate (always grey)

  // ---- Styles ----
  // Adjust font sizes after seeing them in the Emery emulator.
  // Built-in Pebble fonts: "Gothic 14", "Gothic 18", "Gothic 28 Bold", "Bitham 42 Bold"
  // Alloy also supports embedded custom fonts — see developer.repebble.com/guides/alloy/
  const sTime    = new Style({ font: "Gothic 18",      color: C_BLACK, horizontal: "center" });
  const sScore   = new Style({ font: "Gothic 28 Bold", color: C_BLACK, horizontal: "center" });
  const sTeam    = new Style({ font: "Gothic 14",      color: C_MED,   horizontal: "center" });
  const sInfo    = new Style({ font: "Gothic 14",      color: C_MED,   horizontal: "center" });
  const sSmall   = new Style({ font: "Gothic 14",      color: C_DIM,   horizontal: "center" });
  const sStale   = new Style({ font: "Gothic 14",      color: "white", horizontal: "center" });
  const sHeading = new Style({ font: "Gothic 18",      color: C_BLACK, horizontal: "center" });

  // ---- Skins ----
  const skinBg    = new Skin({ fill: C_BG });
  const skinRed   = new Skin({ fill: C_RED });
  const skinBlack = new Skin({ fill: C_BLACK });
  const skinDim   = new Skin({ fill: "#DDDDDD" });

  // ---- Module state ----
  let app = null;
  let mainContainer = null;
  ```

- [ ] **Step 2: Write the bases diamond helper**

  The diamond is four small squares arranged in a rotated-square pattern.
  Occupied bases are amber; empty bases are grey.

  ```js
  // src/display.js (continued)

  function buildBasesDiamond(gs) {
    // 36w × 28h container, bases at the 4 compass positions
    const SZ = 8; // square size in px
    const cx = 14, cy = 10; // center offsets

    function base(x, y, filled) {
      return new Content(null, {
        left: x, top: y, width: SZ, height: SZ,
        skin: new Skin({ fill: filled ? C_AMBER : C_EMPTY }),
      });
    }

    return new Container(null, {
      width: 36, height: 28,
      contents: [
        base(cx,      0,      gs.second), // 2nd — top
        base(cx * 2,  cy,     gs.first),  // 1st — right
        base(0,       cy,     gs.third),  // 3rd — left
        base(cx,      cy * 2, false),     // home — always grey
      ],
    });
  }
  ```

- [ ] **Step 3: Write the cycle dot indicator helper**

  ```js
  // src/display.js (continued)

  function buildCycleDots(currentSlot, totalSlots) {
    const DOT = 6;
    const GAP = 4;
    const dots = [];
    for (let i = 0; i < totalSlots; i++) {
      dots.push(new Content(null, {
        width: DOT, height: DOT,
        skin: new Skin({ fill: i === currentSlot ? C_BLACK : C_DIM }),
        left: i * (DOT + GAP),
      }));
    }
    return new Container(null, {
      width: totalSlots * (DOT + GAP) - GAP,
      height: DOT,
      contents: dots,
    });
  }
  ```

- [ ] **Step 4: Write buildLiveContent**

  ```js
  // src/display.js (continued)

  function buildLiveContent(gs, slot, total) {
    const scoreColor = gs.status === "final" ? C_DIM : C_BLACK;
    const scoreStyle = gs.status === "final"
      ? new Style({ font: "Gothic 28 Bold", color: C_DIM, horizontal: "center" })
      : sScore;

    return new Column(null, {
      width: screen.width, height: screen.height,
      skin: skinBg,
      contents: [
        // Time
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        // Teams row
        new Row(null, {
          top: 4, width: screen.width,
          contents: [
            new Column(null, { width: screen.width * 0.4,
              contents: [
                new Label(null, { string: gs.teamAbbrev,  style: sTeam }),
                new Label(null, { string: String(gs.myScore),    style: scoreStyle }),
              ]}),
            new Label(null, { string: "–", style: sScore }),
            new Column(null, { width: screen.width * 0.4,
              contents: [
                new Label(null, { string: gs.opponent,    style: sTeam }),
                new Label(null, { string: String(gs.theirScore), style: scoreStyle }),
              ]}),
          ],
        }),
        // Inning + outs
        new Label(null, {
          top: 2,
          string: `${gs.inningHalf === "Bot" ? "▼" : "▲"} ${gs.inningHalf} ${gs.inning}  •  ${gs.outs} out${gs.outs !== 1 ? "s" : ""}`,
          style: sInfo,
        }),
        // Bases diamond
        buildBasesDiamond(gs),
        // Count
        new Label(null, { top: 2, string: `B:${gs.balls}  S:${gs.strikes}`, style: sSmall }),
        // Cycle dots
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }

  function formatWatchTime() {
    const d = new Date();
    let h = d.getHours(), m = d.getMinutes();
    const ampm = h >= 12 ? "PM" : "AM";
    h = h % 12 || 12;
    return `${h}:${m.toString().padStart(2, "0")} ${ampm}`;
  }
  ```

- [ ] **Step 5: Wire to a stub, run in emulator, verify live state layout**

  In `src/index.js`, temporarily add:

  ```js
  import { createWatchface, updateDisplay } from "display";
  import { STUBS } from "scores.stub"; // import the stub array directly for testing

  const app = createWatchface();
  updateDisplay(STUBS[0], 0, 3); // slot 0 of 3
  ```

  Add to `src/display.js`:

  ```js
  export function createWatchface() {
    mainContainer = new Container(null, { width: screen.width, height: screen.height, skin: skinBg });
    app = new Application(null, { skin: skinBg, contents: [mainContainer] });
    return app;
  }
  ```

  Run in Emery emulator. Verify: time shows, score shows large, inning/outs shows, bases diamond has amber squares on 1st and 3rd, B:3 S:1 shows, 3 dots with first dot filled.

  Tweak font sizes and spacing until it looks right at Emery's resolution.

- [ ] **Step 6: Commit**

  ```bash
  git add src/display.js
  git commit -m "feat: add display.js live state and bases diamond"
  ```

---

## Task 6: display.js — remaining states + public API

**Files:**
- Modify: `src/display.js`

- [ ] **Step 1: Write buildScheduledContent**

  ```js
  // src/display.js (continued)

  function buildScheduledContent(gs, slot, total) {
    return new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Container(null, {
          top: 8, width: screen.width, height: 1, skin: skinDim,
        }),
        new Column(null, {
          top: 6,
          contents: [
            new Label(null, { string: gs.teamName ?? gs.teamAbbrev, style: sHeading }),
          ],
        }),
        new Container(null, {
          top: 6, width: screen.width, height: 1, skin: skinDim,
        }),
        new Column(null, {
          top: 8,
          contents: [
            new Label(null, { string: `vs ${gs.opponent}  •  ${gs.gameDate}`, style: sInfo }),
            new Label(null, { top: 2, string: gs.gameTime, style: sScore }),
          ],
        }),
        new Label(null, { top: 4, string: "game not yet started", style: sSmall }),
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }
  ```

- [ ] **Step 2: Write buildFinalContent**

  ```js
  // src/display.js (continued)

  function buildFinalContent(gs, slot, total) {
    const dimScore = new Style({ font: "Gothic 28 Bold", color: C_DIM, horizontal: "center" });
    const dimTeam  = new Style({ font: "Gothic 14",      color: C_DIM, horizontal: "center" });
    const won = gs.myScore > gs.theirScore;

    return new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Row(null, {
          top: 8, width: screen.width,
          contents: [
            new Column(null, { width: screen.width * 0.4, contents: [
              new Label(null, { string: gs.teamAbbrev,          style: dimTeam }),
              new Label(null, { string: String(gs.myScore),     style: dimScore }),
            ]}),
            new Label(null, { string: "–", style: dimScore }),
            new Column(null, { width: screen.width * 0.4, contents: [
              new Label(null, { string: gs.opponent,            style: dimTeam }),
              new Label(null, { string: String(gs.theirScore),  style: dimScore }),
            ]}),
          ],
        }),
        // FINAL badge
        new Container(null, {
          top: 8, height: 18, width: 60, skin: skinBlack,
          contents: [new Label(null, { string: "FINAL", style: sStale })],
        }),
        new Label(null, {
          top: 4,
          string: `${won ? gs.teamAbbrev : gs.opponent} wins`,
          style: sSmall,
        }),
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }
  ```

- [ ] **Step 3: Write buildOffDayContent**

  ```js
  // src/display.js (continued)

  function buildOffDayContent(gs, slot, total) {
    return new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Container(null, { top: 8, width: screen.width, height: 1, skin: skinDim }),
        new Label(null, { top: 6, string: gs.teamName ?? gs.teamAbbrev, style: sHeading }),
        new Container(null, { top: 6, width: screen.width, height: 1, skin: skinDim }),
        new Column(null, {
          top: 8,
          contents: [
            new Label(null, { string: "next game",           style: sSmall }),
            new Label(null, { top: 2, string: `vs ${gs.nextOpponent}`, style: sInfo }),
            new Label(null, { top: 2, string: gs.nextDate,   style: sHeading }),
            new Label(null, { top: 2, string: gs.nextTime,   style: sInfo }),
          ],
        }),
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }
  ```

- [ ] **Step 4: Write buildPostponedContent**

  ```js
  // src/display.js (continued)

  function buildPostponedContent(gs, slot, total) {
    return new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Label(null, { top: 12, string: gs.teamAbbrev, style: sHeading }),
        new Container(null, {
          top: 8, height: 18, width: 46, skin: skinBlack,
          contents: [new Label(null, { string: "PPD", style: sStale })],
        }),
        new Label(null, {
          top: 6,
          string: gs.nextDate ? `Rescheduled: ${gs.nextDate}` : "Rescheduled TBD",
          style: sSmall,
        }),
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }
  ```

- [ ] **Step 5: Write buildErrorContent**

  Shown when `scores.js` returns `status: "error"` (malformed API response).

  ```js
  // src/display.js (continued)

  function buildErrorContent(gs, slot, total) {
    return new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Label(null, { top: 12, string: gs.teamAbbrev ?? "—", style: sHeading }),
        new Label(null, { top: 8, string: "Data unavailable", style: sSmall }),
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }
  ```

- [ ] **Step 6: Write buildConnectingContent then buildContent**

  Define `buildConnectingContent` first — `buildContent` calls it, and Moddable's JS engine may not hoist function declarations.

  ```js
  // src/display.js (continued)

  const STALE_THRESHOLD_MS = 5 * 60 * 1000; // 5 minutes

  // Must be defined before buildContent, which calls it.
  function buildConnectingContent(slot, total) {
    return new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Label(null, { top: 20, string: "Connecting…", style: sInfo }),
        new Container(null, { bottom: 4, contents: [buildCycleDots(slot, total)] }),
      ],
    });
  }

  function buildContent(gs, slot, total) {
    // No-connection overlay: if data is stale, wrap the normal content
    const isStale = gs && (Date.now() - gs.lastUpdated > STALE_THRESHOLD_MS);

    let inner;
    if (!gs) {
      inner = buildConnectingContent(slot, total);
    } else {
      switch (gs.status) {
        case "live":       inner = buildLiveContent(gs, slot, total);       break;
        case "scheduled":  inner = buildScheduledContent(gs, slot, total);  break;
        case "final":      inner = buildFinalContent(gs, slot, total);      break;
        case "off_day":    inner = buildOffDayContent(gs, slot, total);     break;
        case "postponed":  inner = buildPostponedContent(gs, slot, total);  break;
        default:           inner = buildErrorContent(gs, slot, total);      break;
      }
    }

    if (!isStale) return inner;

    // Wrap in a Container with the stale overlay on top
    const ageMin = Math.floor((Date.now() - gs.lastUpdated) / 60000);
    return new Container(null, {
      width: screen.width, height: screen.height,
      contents: [
        inner,
        new Container(null, {
          bottom: 18, width: screen.width, height: 18, skin: skinRed,
          contents: [
            new Label(null, {
              string: `⚠ last update: ${ageMin}m ago`,
              style: sStale,
            }),
          ],
        }),
      ],
    });
  }
  ```

- [ ] **Step 7: Write the public API**

  ```js
  // src/display.js (continued)

  export function createWatchface() {
    mainContainer = new Container(null, {
      width: screen.width, height: screen.height, skin: skinBg,
    });
    app = new Application(null, { skin: skinBg, contents: [mainContainer] });
    return app;
  }

  export function updateDisplay(gs, slot, total) {
    if (!mainContainer) return;
    mainContainer.empty();
    mainContainer.add(buildContent(gs, slot, total));
  }

  export function renderUnconfigured() {
    if (!mainContainer) return;
    mainContainer.empty();
    mainContainer.add(new Column(null, {
      width: screen.width, height: screen.height, skin: skinBg,
      contents: [
        new Label(null, { top: 4, string: formatWatchTime(), style: sTime }),
        new Label(null, { top: 20, string: "Open settings", style: sInfo }),
        new Label(null, { top: 4, string: "to pick a team", style: sSmall }),
      ],
    }));
  }
  ```

- [ ] **Step 8: Verify all states in emulator using scores.stub.js**

  In `src/index.js`, temporarily cycle through all stubs:

  ```js
  import { createWatchface, updateDisplay } from "display";
  import { fetchTodayGame } from "scores.stub"; // swap to stub

  const app = createWatchface();
  let i = 0;

  Timer.repeat(() => {
    fetchTodayGame(i).then(gs => updateDisplay(gs, i % 3, 3));
    i++;
  }, 3000); // cycle every 3s to see all states quickly
  ```

  Run in emulator. Watch each state appear in turn:
  - Live: large score, bases, count
  - Scheduled: team name, start time
  - Final: dimmed score, FINAL badge
  - Off Day: team name, next game info
  - Postponed: PPD badge
  - Error/stale: ⚠ banner (note the stale stub has `lastUpdated` 5 min ago)

  Verify no text clipping on long team names ("Cardinals", "Diamondbacks"). Adjust spacing as needed.

- [ ] **Step 9: Commit**

  ```bash
  git add src/display.js
  git commit -m "feat: add all display states and no-connection overlay"
  ```

---

## Task 7: clay-config.json

**Files:**
- Create: `config/clay-config.json`

- [ ] **Step 1: Create clay-config.json with all 30 teams**

  Teams are sorted alphabetically within each league. Values are official MLB team IDs.

  ```json
  [
    {
      "type": "heading",
      "defaultValue": "Baseball Scores",
      "size": 1
    },
    {
      "type": "section",
      "items": [
        {
          "type": "text",
          "defaultValue": "Teams to track"
        },
        {
          "type": "select",
          "messageKey": "team1",
          "label": "Team 1",
          "defaultValue": "0",
          "options": [
            { "label": "— None —",                "value": "0"   },
            { "label": "Arizona Diamondbacks",    "value": "109" },
            { "label": "Atlanta Braves",          "value": "144" },
            { "label": "Baltimore Orioles",       "value": "110" },
            { "label": "Boston Red Sox",          "value": "111" },
            { "label": "Chicago Cubs",            "value": "112" },
            { "label": "Chicago White Sox",       "value": "145" },
            { "label": "Cincinnati Reds",         "value": "113" },
            { "label": "Cleveland Guardians",     "value": "114" },
            { "label": "Colorado Rockies",        "value": "115" },
            { "label": "Detroit Tigers",          "value": "116" },
            { "label": "Houston Astros",          "value": "117" },
            { "label": "Kansas City Royals",      "value": "118" },
            { "label": "Los Angeles Angels",      "value": "108" },
            { "label": "Los Angeles Dodgers",     "value": "119" },
            { "label": "Miami Marlins",           "value": "146" },
            { "label": "Milwaukee Brewers",       "value": "158" },
            { "label": "Minnesota Twins",         "value": "142" },
            { "label": "New York Mets",           "value": "121" },
            { "label": "New York Yankees",        "value": "147" },
            { "label": "Oakland Athletics",       "value": "133" },
            { "label": "Philadelphia Phillies",   "value": "143" },
            { "label": "Pittsburgh Pirates",      "value": "134" },
            { "label": "San Diego Padres",        "value": "135" },
            { "label": "San Francisco Giants",    "value": "137" },
            { "label": "Seattle Mariners",        "value": "136" },
            { "label": "St. Louis Cardinals",     "value": "138" },
            { "label": "Tampa Bay Rays",          "value": "139" },
            { "label": "Texas Rangers",           "value": "140" },
            { "label": "Toronto Blue Jays",       "value": "141" },
            { "label": "Washington Nationals",    "value": "120" }
          ]
        },
        {
          "type": "select",
          "messageKey": "team2",
          "label": "Team 2",
          "defaultValue": "0",
          "options": "__SAME_AS_TEAM1__"
        },
        {
          "type": "select",
          "messageKey": "team3",
          "label": "Team 3",
          "defaultValue": "0",
          "options": "__SAME_AS_TEAM1__"
        }
      ]
    },
    {
      "type": "section",
      "items": [
        {
          "type": "text",
          "defaultValue": "Display"
        },
        {
          "type": "range",
          "messageKey": "cycleInterval",
          "label": "Cycle interval (seconds)",
          "defaultValue": 15,
          "min": 5,
          "max": 60,
          "step": 5
        },
        {
          "type": "range",
          "messageKey": "pollInterval",
          "label": "Refresh interval (seconds)",
          "defaultValue": 60,
          "min": 30,
          "max": 300,
          "step": 30
        }
      ]
    },
    {
      "type": "text",
      "defaultValue": "Refresh only applies to live games. Scheduled and off-day slots update every 5 min automatically."
    }
  ]
  ```

  > **Note:** Replace `"__SAME_AS_TEAM1__"` in team2 and team3 with the same options array as team1. Clay doesn't support shared option references — you must duplicate the array.
  >
  > Also verify the Oakland A's current team ID against https://statsapi.mlb.com/api/v1/teams?sportId=1 — the franchise relocated and the ID may have changed.

- [ ] **Step 2: Register Clay in package.json**

  In `package.json`, ensure Clay is configured. Check the App Configuration guide at https://developer.repebble.com/guides/user-interfaces/app-configuration/ for the exact key — in most Alloy projects it looks like:

  ```json
  {
    "pebble": {
      "capabilities": ["configurable"]
    },
    "clay": {
      "config": "config/clay-config.json"
    }
  }
  ```

- [ ] **Step 3: Verify config page opens on device**

  Install the watchface on the real Pebble. In the Pebble phone app → My Watchfaces → Baseball Scores → Settings. Verify the page opens and shows 3 team dropdowns and 2 sliders. Select a team for Team 1 and Save.

- [ ] **Step 4: Commit**

  ```bash
  git add config/clay-config.json package.json
  git commit -m "feat: add Clay config with all 30 teams and display sliders"
  ```

---

## Task 8: index.js — orchestration

**Files:**
- Modify: `src/index.js`

- [ ] **Step 1: Write index.js**

  ```js
  // src/index.js
  import Proxy from "@moddable/proxy";
  import { readSettings, writeSettings, teamIds } from "settings";
  import { fetchTodayGame, fetchLiveGame, refreshSchedule } from "scores";
  import { createWatchface, updateDisplay, renderUnconfigured } from "display";

  // ---- Bootstrap ----
  const settings    = readSettings();
  const configured  = teamIds(settings);  // [teamId, ...] with id > 0

  const app = createWatchface();

  if (configured.length === 0) {
    renderUnconfigured();
  } else {
    startApp(settings, configured);
  }

  // ---- Listen for Clay settings updates ----
  // Clay sends a "settings" message via the proxy when the user saves config.
  // The proxy must be connected for this to arrive.
  // See: https://developer.repebble.com/guides/user-interfaces/app-configuration/
  //
  // NOTE: The Alloy API for restarting the watchface is not well-documented.
  // If `System.restart()` does not exist in the Alloy runtime, implement a
  // hot-swap instead: call writeSettings, then update the cycle/poll timers
  // and re-run fetchAllSchedules with the new teamIds. For v1 simplicity,
  // the restart approach is preferred — check developer.repebble.com/docs/
  // for the correct API.
  function onSettingsReceived(incoming) {
    writeSettings(incoming);
    if (typeof System !== "undefined" && typeof System.restart === "function") {
      System.restart();
    }
    // Fallback: re-fetch all schedules with new settings on next timer tick.
    // The cycle timer will pick up new teamIds on the next iteration.
  }

  // ---- Main app ----
  function startApp(settings, configured) {
    // State per slot: null until first fetch resolves
    const states = new Array(configured.length).fill(null);
    let currentSlot = 0;

    // Show connecting state immediately
    updateDisplay(null, currentSlot, configured.length);

    // ---- Proxy ----
    const proxy = new Proxy();

    proxy.addEventListener("connected", () => {
      // Initial load — fetch all teams in sequence (not parallel — avoid bursting)
      fetchAllSchedules(configured, states, settings, proxy);
    });

    proxy.addEventListener("settings", (event) => {
      onSettingsReceived(event.data, configured, states, settings, currentSlot);
    });

    // ---- Cycle timer ----
    Timer.repeat(() => {
      currentSlot = (currentSlot + 1) % configured.length;
      updateDisplay(states[currentSlot], currentSlot, configured.length);
    }, settings.cycleInterval * 1000);

    // ---- Poll timer (live games only) ----
    Timer.repeat(() => {
      for (let i = 0; i < configured.length; i++) {
        const gs = states[i];
        if (gs?.status === "live" && gs.gamePk) {
          fetchLiveGame(gs.gamePk, gs)
            .then(updated => {
              states[i] = updated;
              if (i === currentSlot) updateDisplay(updated, currentSlot, configured.length);
            })
            .catch(() => {
              // Leave state as-is; lastUpdated unchanged → stale overlay will appear
            });
        }
      }
    }, settings.pollInterval * 1000);

    // ---- Schedule timer (every 5 min) ----
    Timer.repeat(() => {
      for (let i = 0; i < configured.length; i++) {
        refreshSchedule(configured[i], states[i])
          .then(updated => {
            states[i] = updated;
            if (i === currentSlot) updateDisplay(updated, currentSlot, configured.length);
          })
          .catch(() => { /* leave stale */ });
      }
    }, 5 * 60 * 1000);
  }

  async function fetchAllSchedules(teamIdList, states, settings, proxy) {
    for (let i = 0; i < teamIdList.length; i++) {
      try {
        states[i] = await fetchTodayGame(teamIdList[i]);
      } catch {
        // leave null — shows connecting/stale state
      }
      updateDisplay(states[i], 0, teamIdList.length); // update display as each resolves
    }
  }
  ```

- [ ] **Step 2: Remove stub import, restore real scores import**

  Make sure `src/index.js` imports from `"scores"` not `"scores.stub"`. The stub is only used during display development.

- [ ] **Step 3: Verify timer logs in emulator**

  Add temporary traces to confirm all three timers fire:

  ```js
  // In startApp(), after Timer.repeat calls:
  trace(`Cycle timer: ${settings.cycleInterval}s\n`);
  trace(`Poll timer: ${settings.pollInterval}s\n`);
  trace(`Schedule timer: 300s\n`);
  ```

  Run in emulator. Console should show the three lines. Remove traces after verifying.

- [ ] **Step 4: Commit**

  ```bash
  git add src/index.js
  git commit -m "feat: add index.js orchestration — proxy, three timers, state management"
  git push
  ```

---

## Task 9: Real device integration test

**Files:** None — verification only.

- [ ] **Step 1: Build and install on device**

  In CloudPebble: Run → Install on Pebble. Or with local SDK:
  ```bash
  pebble build && pebble install --phone <phone-ip>
  ```

- [ ] **Step 2: Verify proxy connection**

  Open the Pebble phone app. Watch face should load, show "Connecting…" briefly, then resolve to the first team's state. Check CloudPebble console (or phone app console) for any fetch errors.

- [ ] **Step 3: Verify Clay config saves and applies**

  In Pebble phone app → Settings for watchface. Pick Team 1 = any team. Save. Verify watch shows that team. Confirm `localStorage` persists after watch face restart (close and reopen watchface).

- [ ] **Step 4: Verify cycle timer**

  With 2+ teams configured, watch should auto-cycle. Confirm cycle happens at the configured interval and does not stutter when a network call is in flight.

- [ ] **Step 5: Verify stale overlay**

  Disable WiFi and mobile data on the phone (or walk the watch out of Bluetooth range). Within 5 minutes, the ⚠ banner should appear with the age of the last update. Re-enable connection — the banner should disappear on the next successful poll.

- [ ] **Step 6: Verify with a live game (if in season)**

  Configure a team that has a game in progress. Confirm score, inning, outs, bases, and count update at the poll interval. Change poll interval via settings to 30s — verify it updates faster.

- [ ] **Step 7: Final commit and push**

  ```bash
  git add -A
  git commit -m "feat: complete pebble-baseball watchface v1"
  git push
  ```

---

## Self-review notes

- `normalizeLinescore` intentionally does not update `status` — only `refreshSchedule` (schedule timer) may change game state. This prevents a stale linescore from flipping a finished game back to "live".
- The cycle timer fires on a fixed `Timer.repeat` and never awaits fetch calls — the display update from a fetch goes through `states[i]` and only refreshes the screen if `i === currentSlot`. This prevents fetch latency from affecting cycle smoothness.
- `fetchAllSchedules` uses a sequential loop (not `Promise.all`) to avoid bursting three simultaneous requests on app launch.
- Clay settings arrive via a `"settings"` proxy event — `System.restart()` is the simplest way to apply new team/interval settings cleanly.
