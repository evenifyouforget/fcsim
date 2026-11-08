// Editor regression tests that drive the real arena code (src/arena.cpp)
// through its input handlers, without a window. Built with ASan/UBSan as
// arena_test_asan, so memory errors abort the run. List scenarios with
// `arena_test_asan --list` and run one with `arena_test_asan <name>`;
// test/test_arena.py runs each in turn.

#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/arena.hpp"
#include "../src/interval.h"

// The front ends provide these (src/timing.cpp natively, JS on the web); the
// tests have no event loop, as in src/run_single_design.cpp.
extern "C" {
int set_interval(void (*func)(void *arg), int delay, void *arg) { return 0; }
void clear_interval(int id) {}
double time_precise_ms() { return 0; }
}

// editor internals not exposed in arena.h
void mouse_down_tool(struct arena *arena, float x, float y);
void pixel_to_world(struct view *view, int x, int y, float *x_world,
                    float *y_world);

// X11 key codes, as in arena_key_down_event
enum { KEY_R = 27, KEY_M = 58, KEY_U = 30, KEY_CTRL = 37 };

static const char LEVEL_XML[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><retrieveLevel><level>"
    "<levelBlocks></levelBlocks><playerBlocks></playerBlocks>"
    "<start><position><x>0</x><y>0</y></position>"
    "<width>2000</width><height>2000</height></start>"
    "<end><position><x>5000</x><y>5000</y></position>"
    "<width>10</width><height>10</height></end>"
    "</level><levelId>1</levelId></retrieveLevel>";

static void init(arena *a) {
  std::vector<char> xml(LEVEL_XML, LEVEL_XML + strlen(LEVEL_XML));
  arena_init(a, 800, 800, xml.data(), (int)xml.size());
}

static void press(arena *a, int key) {
  arena_key_down_event(a, key);
  arena_key_up_event(a, key);
}

// arena_mouse_move_event updates cursor_x/y after handling the move, so send
// it twice to leave the cursor at (x, y) as a real pointer would.
static void move(arena *a, int x, int y) {
  arena_mouse_move_event(a, x, y);
  arena_mouse_move_event(a, x, y);
}

// arena_mouse_button_down_event also hit-tests the UI buttons, which need the
// graphics state, so call the tool handler it dispatches to directly.
static void mouse_down(arena *a) {
  float x_world, y_world;
  pixel_to_world(&a->view, a->cursor_x, a->cursor_y, &x_world, &y_world);
  mouse_down_tool(a, x_world, y_world);
}

static void mouse_up(arena *a) { arena_mouse_button_up_event(a, 1); }

// what block_graphics_reset does to the editor state on each drawn frame
static void frame(arena *a) { arena_hit_test_update(a); }

// Draw a rod from (400, 400) to (500, 400), hover its free end so a frame
// caches that joint as a hit-test candidate, then ctrl+click to delete it.
// No frame is drawn after the delete, so the cache is stale (#148).
static void draw_hover_delete_rod(arena *a) {
  init(a);
  press(a, KEY_R);
  move(a, 400, 400);
  frame(a);
  mouse_down(a);
  move(a, 500, 400);
  mouse_up(a);
  frame(a);
  move(a, 500, 400);
  frame(a);
  arena_key_down_event(a, KEY_CTRL);
  mouse_down(a);
  mouse_up(a);
  arena_key_up_event(a, KEY_CTRL);
}

static int check_no_blocks(arena *a) {
  if (a->design.design_blocks.head != nullptr) {
    printf("expected the rod to be deleted\n");
    return 1;
  }
  return 0;
}

// #148: a mouse move before the next frame set hover_joint from the stale
// cache, and the next rod click attached the new rod to the freed joint.
static int rod_after_delete(arena *a) {
  draw_hover_delete_rod(a);
  move(a, 501, 400);
  if (a->hover_joint != nullptr) {
    printf("hover_joint should be NULL after its rod was deleted\n");
    return 1;
  }
  mouse_down(a);
  mouse_up(a);
  return 0;
}

// #148, same with the wheel tool
static int wheel_after_delete(arena *a) {
  draw_hover_delete_rod(a);
  press(a, KEY_U);
  move(a, 501, 400);
  mouse_down(a);
  mouse_up(a);
  return 0;
}

// #148: mouse_down_move read the stale cache directly when the click landed
// before the next frame.
static int move_after_delete(arena *a) {
  draw_hover_delete_rod(a);
  press(a, KEY_M);
  if (check_no_blocks(a))
    return 1;
  mouse_down(a);
  mouse_up(a);
  return 0;
}

struct scenario {
  const char *name;
  int (*fn)(arena *);
};

static const scenario SCENARIOS[] = {
    {"rod_after_delete", rod_after_delete},
    {"wheel_after_delete", wheel_after_delete},
    {"move_after_delete", move_after_delete},
};

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list") == 0) {
    for (const scenario &s : SCENARIOS)
      printf("%s\n", s.name);
    return 0;
  }
  // arena_init keeps global state across calls, so each scenario runs in its
  // own process
  if (argc != 2) {
    printf("usage: %s --list | <scenario>\n", argv[0]);
    return 2;
  }
  for (const scenario &s : SCENARIOS) {
    if (strcmp(argv[1], s.name) != 0)
      continue;
    static arena a;
    int res = s.fn(&a);
    printf("%s  %s\n", res ? "FAIL" : "PASS", s.name);
    return res ? 1 : 0;
  }
  printf("unknown scenario %s\n", argv[1]);
  return 2;
}
