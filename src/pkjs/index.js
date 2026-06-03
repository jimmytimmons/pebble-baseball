// src/pkjs/index.js — phone side (PebbleKit JS).
// Reads team selection from Clay settings, fetches each team's game from the MLB
// Stats API, and streams a compact GameState per slot to the C watchface over
// AppMessage. Normalisation ported from the Alloy project's scores.js.
// MLB Stats API — https://statsapi.mlb.com (free, no auth).

var Clay = require("pebble-clay");
var clayConfig = require("./clay-config.json");
var clay = new Clay(clayConfig);

var SCHEDULE = "https://statsapi.mlb.com/api/v1/schedule";
var DEMO_TEAMS = [147, 111, 119]; // NYY, BOS, LAD — used until the user picks teams

// ---- settings ----

function getSettings() {
  var s = {};
  try { s = JSON.parse(localStorage.getItem("clay-settings")) || {}; } catch (e) {}
  var ids = [s.team1, s.team2, s.team3]
    .map(function (x) { return parseInt(x, 10) || 0; })
    .filter(function (x) { return x > 0; });
  return {
    teams: ids.length ? ids : DEMO_TEAMS,
    cycle: parseInt(s.cycleInterval, 10) || 15,
  };
}

// ---- formatting / normalisation ----

function isoDateLocal(d) {
  return d.getFullYear() + "-" + ("0" + (d.getMonth() + 1)).slice(-2) + "-" + ("0" + d.getDate()).slice(-2);
}

function formatTime(iso) {
  var d = new Date(iso), h = d.getHours(), m = d.getMinutes();
  var ap = h >= 12 ? "PM" : "AM";
  h = h % 12 || 12;
  return h + ":" + ("0" + m).slice(-2) + " " + ap;
}

function formatDate(iso) {
  var d = new Date(iso), today = new Date();
  if (d.toDateString() === today.toDateString()) return "Today";
  var days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
  var mon = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
  return days[d.getDay()] + " " + mon[d.getMonth()] + " " + d.getDate();
}

function mapStatus(detailedState) {
  if (!detailedState) return "error";
  var s = detailedState.toLowerCase();
  if (s.indexOf("progress") >= 0 || s.indexOf("live") >= 0) return "live";
  if (s.indexOf("final") >= 0 || s.indexOf("game over") >= 0 || s.indexOf("completed") >= 0) return "final";
  if (s.indexOf("postponed") >= 0 || s.indexOf("suspended") >= 0) return "postponed";
  return "scheduled";
}

function normHalf(raw) {
  return raw === "Bottom" ? "Bot" : raw === "Top" ? "Top" : "";
}

function normalize(data, teamId) {
  var game = data && data.dates && data.dates[0] && data.dates[0].games && data.dates[0].games[0];
  if (!game) return null;
  var isHome = game.teams.home.team.id === teamId;
  var mine = isHome ? game.teams.home : game.teams.away;
  var theirs = isHome ? game.teams.away : game.teams.home;
  var ls = game.linescore || {}, off = ls.offense || {};
  var status = mapStatus(game.status && game.status.detailedState);
  return {
    STATUS: status,
    TEAM: mine.team.abbreviation || "",
    TEAM_NAME: mine.team.teamName || mine.team.shortName || mine.team.name || "",
    OPP: theirs.team.abbreviation || "",
    OPP_NAME: theirs.team.teamName || theirs.team.shortName || theirs.team.name || "",
    MY_SCORE: mine.score || 0,
    THEIR_SCORE: theirs.score || 0,
    INNING_HALF: normHalf(ls.inningHalf),
    INNING: ls.currentInning || 0,
    OUTS: ls.outs || 0,
    BALLS: ls.balls || 0,
    STRIKES: ls.strikes || 0,
    BASES: (off.first ? 1 : 0) | (off.second ? 2 : 0) | (off.third ? 4 : 0),
    GAME_TIME: status === "scheduled" ? formatTime(game.gameDate) : "",
    GAME_DATE: status === "scheduled" ? "Today" : "",
  };
}

// ---- networking ----

function getJSON(url, ok, fail) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url);
  xhr.onload = function () { try { ok(JSON.parse(xhr.responseText)); } catch (e) { fail(e); } };
  xhr.onerror = function () { fail("network"); };
  xhr.timeout = 8000;
  xhr.send();
}

// ---- outbound queue (AppMessage is one-at-a-time) ----

var queue = [], sending = false;
function pump() {
  if (sending || queue.length === 0) return;
  sending = true;
  var dict = queue.shift();
  Pebble.sendAppMessage(dict,
    function () { sending = false; pump(); },
    function () { sending = false; setTimeout(pump, 400); });
}
function enqueue(dict) { queue.push(dict); pump(); }

function fetchNextGame(teamId, onDone) {
  var start = isoDateLocal(new Date(Date.now() + 86400000));
  var end = isoDateLocal(new Date(Date.now() + 14 * 86400000));
  getJSON(SCHEDULE + "?sportId=1&teamId=" + teamId + "&startDate=" + start + "&endDate=" + end + "&hydrate=team",
    function (data) {
      var dates = (data && data.dates) || [];
      for (var i = 0; i < dates.length; i++) {
        var g = dates[i].games && dates[i].games[0];
        if (g) {
          var isHome = g.teams.home.team.id === teamId;
          var theirs = isHome ? g.teams.away : g.teams.home;
          return onDone({
            NEXT_OPP: theirs.team.abbreviation || "",
            NEXT_OPP_NAME: theirs.team.teamName || theirs.team.shortName || theirs.team.name || "",
            NEXT_DATE: formatDate(g.gameDate),
            NEXT_TIME: formatTime(g.gameDate),
          });
        }
      }
      onDone(null);
    }, function () { onDone(null); });
}

function fetchTeam(teamId, slot, total, cycle) {
  var today = isoDateLocal(new Date());
  getJSON(SCHEDULE + "?sportId=1&teamId=" + teamId + "&date=" + today + "&hydrate=team,linescore",
    function (data) {
      var gs = normalize(data, teamId);
      if (gs) {
        gs.SLOT = slot; gs.TOTAL = total; gs.CYCLE = cycle;
        enqueue(gs);
        return;
      }
      fetchNextGame(teamId, function (next) {
        var msg = { STATUS: "off_day", TEAM: "", TEAM_NAME: "", OPP: "", SLOT: slot, TOTAL: total, CYCLE: cycle,
                    NEXT_OPP: "", NEXT_OPP_NAME: "", NEXT_DATE: "", NEXT_TIME: "" };
        if (next) { msg.NEXT_OPP = next.NEXT_OPP; msg.NEXT_OPP_NAME = next.NEXT_OPP_NAME; msg.NEXT_DATE = next.NEXT_DATE; msg.NEXT_TIME = next.NEXT_TIME; }
        enqueue(msg);
      });
    },
    function () { enqueue({ STATUS: "error", SLOT: slot, TOTAL: total, CYCLE: cycle }); });
}

function fetchAll() {
  var cfg = getSettings();
  var total = cfg.teams.length;
  console.log("fetching teams: " + JSON.stringify(cfg.teams));
  cfg.teams.forEach(function (teamId, i) { fetchTeam(teamId, i, total, cfg.cycle); });
}

Pebble.addEventListener("ready", function () { console.log("PKJS ready"); fetchAll(); });
Pebble.addEventListener("appmessage", function () { fetchAll(); });

// Clay (registered first, in `new Clay()`) persists the saved settings before this
// fires, so localStorage is fresh here — re-fetch so new teams take effect at once.
Pebble.addEventListener("webviewclosed", function (e) {
  if (e && e.response) { console.log("settings saved; refetching"); fetchAll(); }
});
