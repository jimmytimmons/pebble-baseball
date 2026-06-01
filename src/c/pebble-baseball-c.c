#include <pebble.h>

// Native C watchface. PKJS (phone) fetches each configured team's game and streams
// one GameState per slot over AppMessage; this side stores up to 3 and auto-cycles
// through them, rendering each state with full colour graphics.

typedef struct {
  char status[12];   // live | scheduled | final | off_day | postponed | error
  char team[8];
  char team_name[16];
  char opp[8];
  int  my_score, their_score;
  char inning_half[4];
  int  inning, outs, balls, strikes;
  bool first, second, third;
  char game_time[12];
  char game_date[16];
  char next_opp[8];
  char next_date[16];
  char next_time[12];
} GameState;

#define MAX_SLOTS 3

static Window    *s_window;
static Layer     *s_canvas;
static GameState  s_games[MAX_SLOTS];
static int        s_count = 1;     // configured teams
static int        s_slot = 0;      // currently displayed slot
static int        s_cycle_ms = 15000;
static bool       s_have_data = false;
static AppTimer  *s_cycle_timer = NULL;

static GColor c_bg, c_navy, c_amber, c_empty, c_text, c_dim, c_green, c_red;

// ---- drawing helpers ----

static void draw_centered(GContext *ctx, const char *s, const char *font_key,
                          int w, int y, int h, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, s, fonts_get_system_font(font_key),
                     GRect(0, y, w, h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_base(GContext *ctx, int cx, int cy, bool filled) {
  const int r = 8;
  GPoint pts[4] = { {cx, cy - r}, {cx + r, cy}, {cx, cy + r}, {cx - r, cy} };
  GPathInfo info = { .num_points = 4, .points = pts };
  GPath *p = gpath_create(&info);
  graphics_context_set_fill_color(ctx, filled ? c_amber : c_empty);
  gpath_draw_filled(ctx, p);
  graphics_context_set_stroke_color(ctx, c_dim);
  gpath_draw_outline(ctx, p);
  gpath_destroy(p);
}

static void draw_badge(GContext *ctx, const char *text, GColor bg, int w, int y) {
  const int bw = 96, bh = 30, x = (w - bw) / 2;
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, GRect(x, y, bw, bh), 4, GCornersAll);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(x, y + 4, bw, 24), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_header(GContext *ctx, int w) {
  graphics_context_set_fill_color(ctx, c_navy);
  graphics_fill_rect(ctx, GRect(0, 0, w, 30), 0, GCornerNone);
  char tbuf[8];
  clock_copy_time_string(tbuf, sizeof(tbuf));
  draw_centered(ctx, tbuf, FONT_KEY_GOTHIC_18_BOLD, w, 4, 24, GColorWhite);
}

static void draw_dots(GContext *ctx, int w, int y) {
  if (s_count <= 1) return;
  int gap = 14, startx = w / 2 - (s_count - 1) * gap / 2;
  for (int i = 0; i < s_count; i++) {
    graphics_context_set_fill_color(ctx, i == s_slot ? c_navy : c_dim);
    graphics_fill_circle(ctx, GPoint(startx + i * gap, y), 4);
  }
}

// ---- per-state ----

static void draw_live(GContext *ctx, int w, GameState *g, const char *score) {
  draw_centered(ctx, score, FONT_KEY_GOTHIC_28_BOLD, w, 36, 36, c_text);
  char line[24];
  snprintf(line, sizeof(line), "%s %d  -  %d out%s",
           g->inning_half, g->inning, g->outs, g->outs == 1 ? "" : "s");
  draw_centered(ctx, line, FONT_KEY_GOTHIC_18, w, 74, 24, c_dim);

  int cx = w / 2, cy = 130;
  draw_base(ctx, cx, cy - 20, g->second);
  draw_base(ctx, cx + 20, cy, g->first);
  draw_base(ctx, cx - 20, cy, g->third);
  draw_base(ctx, cx, cy + 20, false);

  char count[16];
  snprintf(count, sizeof(count), "B %d    S %d", g->balls, g->strikes);
  draw_centered(ctx, count, FONT_KEY_GOTHIC_18_BOLD, w, 162, 24, c_text);
}

static void draw_scheduled(GContext *ctx, int w, GameState *g) {
  draw_centered(ctx, g->team_name[0] ? g->team_name : g->team, FONT_KEY_GOTHIC_24_BOLD, w, 40, 30, c_text);
  char vs[16];
  snprintf(vs, sizeof(vs), "vs %s", g->opp);
  draw_centered(ctx, vs, FONT_KEY_GOTHIC_18, w, 78, 24, c_dim);
  draw_centered(ctx, g->game_time, FONT_KEY_GOTHIC_28_BOLD, w, 104, 36, c_text);
  draw_centered(ctx, g->game_date[0] ? g->game_date : "Today", FONT_KEY_GOTHIC_18, w, 146, 24, c_dim);
}

static void draw_final(GContext *ctx, int w, GameState *g, const char *score) {
  bool won = g->my_score > g->their_score;
  draw_centered(ctx, score, FONT_KEY_GOTHIC_28_BOLD, w, 42, 36, c_text);
  draw_badge(ctx, won ? "WIN" : "LOSS", won ? c_green : c_red, w, 92);
  draw_centered(ctx, "Final", FONT_KEY_GOTHIC_18, w, 132, 24, c_dim);
}

static void draw_off_day(GContext *ctx, int w, GameState *g) {
  draw_centered(ctx, g->team_name[0] ? g->team_name : "Off day", FONT_KEY_GOTHIC_24_BOLD, w, 38, 30, c_text);
  draw_centered(ctx, "Next game", FONT_KEY_GOTHIC_18, w, 74, 24, c_dim);
  char vs[16];
  snprintf(vs, sizeof(vs), "vs %s", g->next_opp[0] ? g->next_opp : "TBD");
  draw_centered(ctx, vs, FONT_KEY_GOTHIC_18, w, 98, 24, c_text);
  draw_centered(ctx, g->next_date, FONT_KEY_GOTHIC_24_BOLD, w, 122, 30, c_text);
  draw_centered(ctx, g->next_time, FONT_KEY_GOTHIC_18, w, 158, 24, c_dim);
}

static void draw_postponed(GContext *ctx, int w, GameState *g) {
  draw_centered(ctx, g->team[0] ? g->team : "--", FONT_KEY_GOTHIC_28_BOLD, w, 44, 36, c_text);
  draw_badge(ctx, "PPD", c_dim, w, 96);
  draw_centered(ctx, g->next_date[0] ? g->next_date : "Postponed", FONT_KEY_GOTHIC_18, w, 136, 24, c_dim);
}

static void draw_error(GContext *ctx, int w, GameState *g) {
  draw_centered(ctx, g->team[0] ? g->team : "--", FONT_KEY_GOTHIC_28_BOLD, w, 56, 36, c_text);
  draw_centered(ctx, "Data unavailable", FONT_KEY_GOTHIC_18, w, 100, 24, c_dim);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;

  graphics_context_set_fill_color(ctx, c_bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  draw_header(ctx, w);

  GameState *g = &s_games[s_slot];
  if (!s_have_data || g->status[0] == 0) {
    draw_centered(ctx, "Connecting...", FONT_KEY_GOTHIC_24_BOLD, w, 90, 28, c_dim);
    draw_dots(ctx, w, 204);
    return;
  }

  char score[24];
  snprintf(score, sizeof(score), "%s %d-%d %s", g->team, g->my_score, g->their_score, g->opp);

  if      (strcmp(g->status, "live") == 0)      draw_live(ctx, w, g, score);
  else if (strcmp(g->status, "scheduled") == 0) draw_scheduled(ctx, w, g);
  else if (strcmp(g->status, "final") == 0)     draw_final(ctx, w, g, score);
  else if (strcmp(g->status, "off_day") == 0)   draw_off_day(ctx, w, g);
  else if (strcmp(g->status, "postponed") == 0) draw_postponed(ctx, w, g);
  else                                          draw_error(ctx, w, g);

  draw_dots(ctx, w, 204);
}

// ---- cycling ----

static void cycle_cb(void *data) {
  if (s_count > 1) {
    s_slot = (s_slot + 1) % s_count;
    layer_mark_dirty(s_canvas);
  }
  s_cycle_timer = app_timer_register(s_cycle_ms, cycle_cb, NULL);
}

// ---- AppMessage ----

#define READ_STR(key, field) do { \
    Tuple *t = dict_find(iter, key); \
    if (t) { strncpy(field, t->value->cstring, sizeof(field) - 1); field[sizeof(field) - 1] = 0; } \
  } while (0)
#define READ_INT(key, field) do { \
    Tuple *t = dict_find(iter, key); \
    if (t) field = t->value->int32; \
  } while (0)

static void inbox_received(DictionaryIterator *iter, void *context) {
  int slot = 0, total = s_count, cycle = 0;
  READ_INT(MESSAGE_KEY_SLOT, slot);
  READ_INT(MESSAGE_KEY_TOTAL, total);
  READ_INT(MESSAGE_KEY_CYCLE, cycle);
  if (slot < 0 || slot >= MAX_SLOTS) return;
  if (total >= 1 && total <= MAX_SLOTS) s_count = total;
  if (cycle >= 3) s_cycle_ms = cycle * 1000;

  GameState *g = &s_games[slot];
  READ_STR(MESSAGE_KEY_STATUS, g->status);
  READ_STR(MESSAGE_KEY_TEAM, g->team);
  READ_STR(MESSAGE_KEY_TEAM_NAME, g->team_name);
  READ_STR(MESSAGE_KEY_OPP, g->opp);
  READ_STR(MESSAGE_KEY_INNING_HALF, g->inning_half);
  READ_STR(MESSAGE_KEY_GAME_TIME, g->game_time);
  READ_STR(MESSAGE_KEY_GAME_DATE, g->game_date);
  READ_STR(MESSAGE_KEY_NEXT_OPP, g->next_opp);
  READ_STR(MESSAGE_KEY_NEXT_DATE, g->next_date);
  READ_STR(MESSAGE_KEY_NEXT_TIME, g->next_time);
  READ_INT(MESSAGE_KEY_MY_SCORE, g->my_score);
  READ_INT(MESSAGE_KEY_THEIR_SCORE, g->their_score);
  READ_INT(MESSAGE_KEY_INNING, g->inning);
  READ_INT(MESSAGE_KEY_OUTS, g->outs);
  READ_INT(MESSAGE_KEY_BALLS, g->balls);
  READ_INT(MESSAGE_KEY_STRIKES, g->strikes);
  Tuple *bt = dict_find(iter, MESSAGE_KEY_BASES);
  if (bt) { int m = bt->value->int32; g->first = m & 1; g->second = m & 2; g->third = m & 4; }

  s_have_data = true;
  if (slot == s_slot) layer_mark_dirty(s_canvas);
}

static void request_update(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST, 1);
    app_message_outbox_send();
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
  request_update();   // refresh once a minute
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  c_bg    = GColorFromRGB(244, 241, 234);
  c_navy  = GColorFromRGB(22, 40, 74);
  c_amber = GColorOrange;
  c_empty = GColorFromRGB(201, 196, 184);
  c_text  = GColorBlack;
  c_dim   = GColorDarkGray;
  c_green = GColorFromRGB(30, 142, 62);
  c_red   = GColorFromRGB(192, 57, 43);

  app_message_register_inbox_received(inbox_received);
  app_message_open(512, 64);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  s_cycle_timer = app_timer_register(s_cycle_ms, cycle_cb, NULL);
  request_update();
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
