// Editor regression tests that drive the real arena code (src/arena.cpp)
// through its input handlers, without a window. Built with ASan/UBSan as
// arena_test_asan, so memory errors abort the run. List scenarios with
// `arena_test_asan --list` and run one with `arena_test_asan <name>`;
// test/test_arena.py runs each in turn.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../src/arena.hpp"
#include "../src/interval.h"

extern "C" {
#include "../src/graph.h"
#include "../src/xml.h"
}

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

static bool is_joint_in_list(design *d, joint *j) {
  for (joint *c = d->joints.head; c != nullptr; c = c->next) {
    if (c == j)
      return true;
  }
  return false;
}

// Pointers are compared by value only, so checking them before the original is
// freed never reads the stale memory a bad copy would hold. NULL is allowed:
// wheel inner joints are NULL when the radius is 20 or less.
static int check_joint(design *d, joint *j, const char *what) {
  if (j == nullptr || is_joint_in_list(d, j))
    return 0;
  printf("%s is not one of the copy's joints\n", what);
  return 1;
}

// An attach node must be in the attach list of its joint and point back at
// its block. Only checked once the joint itself has been accepted.
static int check_attach(design *d, block *owner, joint *j, attach_node *n,
                        const char *what) {
  if (j == nullptr || !is_joint_in_list(d, j))
    return 0;
  for (attach_node *c = j->att.head; c != nullptr; c = c->next) {
    if (c == n && n->block == owner)
      return 0;
  }
  printf("%s is not in its joint's attach list\n", what);
  return 1;
}

// number of joint and attach pointers in the design's shapes that are not
// the design's own
static int check_copy_shapes(design *d) {
  int bad = 0;
  for (block *b = d->design_blocks.head; b != nullptr; b = b->next) {
    switch (b->shape.type) {
    case SHAPE_BOX:
      bad += check_joint(d, b->shape.box.center, "box centre");
      for (int i = 0; i < 4; i++)
        bad += check_joint(d, b->shape.box.corners[i], "box corner");
      break;
    case SHAPE_ROD:
      bad += check_joint(d, b->shape.rod.from, "rod endpoint");
      bad += check_joint(d, b->shape.rod.to, "rod endpoint");
      bad += check_attach(d, b, b->shape.rod.from, b->shape.rod.from_att,
                          "rod start attach node");
      bad += check_attach(d, b, b->shape.rod.to, b->shape.rod.to_att,
                          "rod end attach node");
      break;
    case SHAPE_WHEEL:
      bad += check_joint(d, b->shape.wheel.center, "wheel centre");
      bad += check_attach(d, b, b->shape.wheel.center,
                          b->shape.wheel.center_att, "wheel attach node");
      for (int i = 0; i < 4; i++) {
        bad += check_joint(d, b->shape.wheel.spokes[i], "wheel spoke");
        bad += check_joint(d, b->shape.wheel.inner[i], "wheel inner joint");
      }
      break;
    default:
      break;
    }
  }
  return bad;
}

static bool has_shape(design *d, enum shape_type t) {
  for (block *b = d->design_blocks.head; b != nullptr; b = b->next) {
    if (b->shape.type == t)
      return true;
  }
  return false;
}

// a wheel of radius 30 (so it has inner joints), a free rod and a goal box
static const char SHAPES_XML[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><retrieveLevel><level>"
    "<levelBlocks></levelBlocks><playerBlocks>"
    "<ClockwiseWheel><rotation>0</rotation>"
    "<position><x>0</x><y>0</y></position>"
    "<width>60</width><height>60</height></ClockwiseWheel>"
    "<SolidRod><rotation>0</rotation>"
    "<position><x>200</x><y>0</y></position>"
    "<width>100</width><height>8</height></SolidRod>"
    "<JointedDynamicRectangle><rotation>0</rotation>"
    "<position><x>-200</x><y>0</y></position>"
    "<width>60</width><height>60</height>"
    "<goalBlock>true</goalBlock></JointedDynamicRectangle>"
    "</playerBlocks>"
    "<start><position><x>0</x><y>0</y></position>"
    "<width>2000</width><height>2000</height></start>"
    "<end><position><x>5000</x><y>5000</y></position>"
    "<width>10</width><height>10</height></end>"
    "</level><levelId>1</levelId></retrieveLevel>";

// heap-allocated, so it can be freed separately from any arena
static design *load_shapes_design() {
  std::vector<char> xml(SHAPES_XML, SHAPES_XML + sizeof(SHAPES_XML) - 1);
  struct xml_level level;
  design *d = (design *)calloc(1, sizeof(design));
  xml_parse(xml.data(), (int)xml.size(), &level);
  convert_xml(&level, d);
  xml_free(&level);
  return d;
}

struct body_state {
  bool present;
  double x, y, rot;
};

static const int SIM_TICKS = 300;

// steps a world built from the design and returns each design block's body
// state, read before the world is freed
static std::vector<body_state> simulate(design *d) {
  b2World *world = gen_world(d);
  for (int i = 0; i < SIM_TICKS; i++)
    step(world);
  std::vector<body_state> states;
  for (block *b = d->design_blocks.head; b != nullptr; b = b->next) {
    body_state s = {b->body != nullptr, 0, 0, 0};
    if (b->body) {
      s.x = b->body->m_position.x;
      s.y = b->body->m_position.y;
      s.rot = b->body->m_rotation;
    }
    states.push_back(s);
  }
  free_world(world, d);
  return states;
}

static bool same_states(const std::vector<body_state> &a,
                        const std::vector<body_state> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (!a[i].present || !b[i].present || a[i].x != b[i].x ||
        a[i].y != b[i].y || a[i].rot != b[i].rot)
      return false;
  }
  return true;
}

// #199: clean_copy_design is a deep copy. Every joint and attach pointer in a
// copied shape must belong to the copy, so the copy must keep simulating the
// same way after the original is freed. Rods, wheels and goal boxes are the
// shape types that hold such pointers.
static int clean_copy_rod_joints(arena *) {
  design *orig = load_shapes_design();
  design *copy = clean_copy_design(orig);
  int res = 0;
  if (!has_shape(copy, SHAPE_ROD) || !has_shape(copy, SHAPE_WHEEL) ||
      !has_shape(copy, SHAPE_BOX)) {
    printf("expected the copy to contain a rod, a wheel and a goal box\n");
    res = 1;
  } else if (check_copy_shapes(copy) != 0) {
    res = 1;
  } else {
    std::vector<body_state> before = simulate(orig);
    free_design(orig);
    orig = nullptr;
    std::vector<body_state> after = simulate(copy);
    if (!same_states(before, after)) {
      printf("copy does not simulate the same as the original\n");
      res = 1;
    }
  }
  if (orig)
    free_design(orig);
  free_design(copy);
  return res;
}

struct scenario {
  const char *name;
  int (*fn)(arena *);
};

static const scenario SCENARIOS[] = {
    {"rod_after_delete", rod_after_delete},
    {"wheel_after_delete", wheel_after_delete},
    {"move_after_delete", move_after_delete},
    {"clean_copy_rod_joints", clean_copy_rod_joints},
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
