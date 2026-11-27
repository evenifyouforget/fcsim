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
#include "../src/arena.h"
#include "../src/design_file.h"
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

// ---- #179: local save/load round trip ----------------------------------

// A wheel (id 1, radius 30), a goal box (id 3), a rod (id 2) from the wheel
// centre to the box (a block can only be connected to earlier ones), a second
// rod (id 4) from the first rod's far end to a point that is not a joint, and a
// static level block. The rods are not axis-aligned, so the XML
// centre/length/angle form is lossy for them.
static const char JOINTED_XML[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><retrieveLevel><level>"
    "<levelBlocks><StaticRectangle><rotation>0.3</rotation>"
    "<position><x>-500</x><y>100</y></position>"
    "<width>200</width><height>40</height></StaticRectangle></levelBlocks>"
    "<playerBlocks>"
    "<ClockwiseWheel id=\"1\"><rotation>0.1</rotation>"
    "<position><x>0</x><y>0</y></position>"
    "<width>60</width><height>60</height></ClockwiseWheel>"
    "<JointedDynamicRectangle id=\"3\"><rotation>0.2</rotation>"
    "<position><x>75.7</x><y>63.9</y></position>"
    "<width>60</width><height>60</height>"
    "<goalBlock>true</goalBlock></JointedDynamicRectangle>"
    "<SolidRod id=\"2\"><rotation>0.7</rotation>"
    "<position><x>38.3</x><y>32.1</y></position>"
    "<width>100.1</width><height>8</height>"
    "<joints><jointedTo>1</jointedTo><jointedTo>3</jointedTo></joints>"
    "</SolidRod>"
    "<HollowRod id=\"4\"><rotation>-1.1</rotation>"
    "<position><x>120.3</x><y>40.2</y></position>"
    "<width>88.8</width><height>4</height>"
    "<joints><jointedTo>3</jointedTo></joints></HollowRod>"
    "</playerBlocks>"
    "<start><position><x>0</x><y>0</y></position>"
    "<width>2000</width><height>2000</height></start>"
    "<end><position><x>5000</x><y>5000</y></position>"
    "<width>10</width><height>10</height></end>"
    "</level><levelId>7</levelId></retrieveLevel>";

static design *load_xml_design(const char *xml_text, size_t n) {
  std::vector<char> xml(xml_text, xml_text + n);
  struct xml_level level;
  design *d = (design *)calloc(1, sizeof(design));
  xml_parse(xml.data(), (int)xml.size(), &level);
  convert_xml(&level, d);
  xml_free(&level);
  return d;
}

static std::vector<uint8_t> encode(design *d) {
  int len = 0;
  uint8_t *buf = design_file_encode(d, &len);
  std::vector<uint8_t> bytes(buf, buf + len);
  free(buf);
  return bytes;
}

// nullptr if the file is rejected
static design *decode(const std::vector<uint8_t> &bytes) {
  struct xml_level level;
  if (design_file_decode(bytes.data(), (int)bytes.size(), &level) != 0) {
    xml_free(&level);
    return nullptr;
  }
  design *d = (design *)calloc(1, sizeof(design));
  convert_xml(&level, d);
  xml_free(&level);
  return d;
}

// Structural comparison, in list order. Doubles are compared as bit patterns
// so -0.0 and NaN payloads count. Joints are compared by their index in the
// design's joint list, so sharing between blocks has to match too. Returns
// the number of differences; each one is printed.
static bool quiet_diffs = false;

struct design_diff {
  int count = 0;
  void report(const char *what, int block) {
    if (count < 10 && !quiet_diffs)
      printf("differs: %s (block %d)\n", what, block);
    count++;
  }
  void dbl(double a, double b, const char *what, int block) {
    if (memcmp(&a, &b, sizeof a) != 0)
      report(what, block);
  }
};

static int joint_index(design *d, joint *j) {
  if (j == nullptr)
    return -1;
  int i = 0;
  for (joint *c = d->joints.head; c != nullptr; c = c->next, i++) {
    if (c == j)
      return i;
  }
  return -2; // not one of the design's joints
}

static void diff_joint_ref(design_diff &df, design *a, joint *ja, design *b,
                           joint *jb, const char *what, int block) {
  if (joint_index(a, ja) != joint_index(b, jb))
    df.report(what, block);
}

static void diff_areas(design_diff &df, const area &a, const area &b,
                       const char *what) {
  df.dbl(a.x, b.x, what, -1);
  df.dbl(a.y, b.y, what, -1);
  df.dbl(a.w, b.w, what, -1);
  df.dbl(a.h, b.h, what, -1);
  df.dbl(a.expand, b.expand, what, -1);
}

static void diff_block_list(design_diff &df, design *da, block *a, design *db,
                            block *b) {
  int i = 0;
  for (; a != nullptr && b != nullptr; a = a->next, b = b->next, i++) {
    if (a->shape.type != b->shape.type || a->type_id != b->type_id ||
        a->goal != b->goal || a->material != b->material) {
      df.report("block kind", i);
      continue;
    }
    const shape &sa = a->shape, &sb = b->shape;
    switch (sa.type) {
    case SHAPE_RECT:
      df.dbl(sa.rect.x, sb.rect.x, "rect x", i);
      df.dbl(sa.rect.y, sb.rect.y, "rect y", i);
      df.dbl(sa.rect.w, sb.rect.w, "rect w", i);
      df.dbl(sa.rect.h, sb.rect.h, "rect h", i);
      df.dbl(sa.rect.angle, sb.rect.angle, "rect angle", i);
      break;
    case SHAPE_CIRC:
      df.dbl(sa.circ.x, sb.circ.x, "circ x", i);
      df.dbl(sa.circ.y, sb.circ.y, "circ y", i);
      df.dbl(sa.circ.radius, sb.circ.radius, "circ radius", i);
      break;
    case SHAPE_BOX:
      df.dbl(sa.box.x, sb.box.x, "box x", i);
      df.dbl(sa.box.y, sb.box.y, "box y", i);
      df.dbl(sa.box.w, sb.box.w, "box w", i);
      df.dbl(sa.box.h, sb.box.h, "box h", i);
      df.dbl(sa.box.angle, sb.box.angle, "box angle", i);
      diff_joint_ref(df, da, sa.box.center, db, sb.box.center, "box centre", i);
      for (int k = 0; k < 4; k++)
        diff_joint_ref(df, da, sa.box.corners[k], db, sb.box.corners[k],
                       "box corner", i);
      break;
    case SHAPE_ROD:
      df.dbl(sa.rod.width, sb.rod.width, "rod width", i);
      diff_joint_ref(df, da, sa.rod.from, db, sb.rod.from, "rod from", i);
      diff_joint_ref(df, da, sa.rod.to, db, sb.rod.to, "rod to", i);
      break;
    case SHAPE_WHEEL:
      df.dbl(sa.wheel.radius, sb.wheel.radius, "wheel radius", i);
      df.dbl(sa.wheel.angle, sb.wheel.angle, "wheel angle", i);
      if (sa.wheel.spin != sb.wheel.spin)
        df.report("wheel spin", i);
      diff_joint_ref(df, da, sa.wheel.center, db, sb.wheel.center,
                     "wheel centre", i);
      for (int k = 0; k < 4; k++) {
        diff_joint_ref(df, da, sa.wheel.spokes[k], db, sb.wheel.spokes[k],
                       "wheel spoke", i);
        diff_joint_ref(df, da, sa.wheel.inner[k], db, sb.wheel.inner[k],
                       "wheel inner joint", i);
      }
      break;
    }
  }
  if (a != nullptr || b != nullptr)
    df.report("block count", i);
}

static int diff_designs(design *a, design *b) {
  design_diff df;
  diff_block_list(df, a, a->level_blocks.head, b, b->level_blocks.head);
  diff_block_list(df, a, a->design_blocks.head, b, b->design_blocks.head);
  diff_areas(df, a->build_area, b->build_area, "build area");
  diff_areas(df, a->goal_area, b->goal_area, "goal area");
  if (a->level_id != b->level_id)
    df.report("level id", -1);
  joint *ja = a->joints.head, *jb = b->joints.head;
  int i = 0;
  for (; ja != nullptr && jb != nullptr; ja = ja->next, jb = jb->next, i++) {
    df.dbl(ja->x, jb->x, "joint x", i);
    df.dbl(ja->y, jb->y, "joint y", i);
    if ((ja->gen == nullptr) != (jb->gen == nullptr))
      df.report("joint owner", i);
    int na = 0, nb = 0;
    for (attach_node *n = ja->att.head; n != nullptr; n = n->next)
      na++;
    for (attach_node *n = jb->att.head; n != nullptr; n = n->next)
      nb++;
    if (na != nb)
      df.report("joint attachments", i);
  }
  if (ja != nullptr || jb != nullptr)
    df.report("joint count", i);
  if (recalculate_design_checksum(a) != recalculate_design_checksum(b))
    df.report("checksum", -1);
  return df.count;
}

// loadXML (1) -> design (2) -> file (3) -> design (4); (2) must equal (4)
// exactly, and saving (4) must give the same bytes as saving (2).
static int check_round_trip(design *d2) {
  std::vector<uint8_t> file3 = encode(d2);
  design *d4 = decode(file3);
  if (d4 == nullptr) {
    printf("the file the encoder wrote was rejected by the decoder\n");
    return 1;
  }
  int res = 0;
  if (diff_designs(d2, d4) != 0)
    res = 1;
  if (encode(d4) != file3) {
    printf("saving the loaded design gave different bytes\n");
    res = 1;
  }
  design *copy = clean_copy_design(d2);
  if (encode(copy) != file3) {
    printf("saving a clean copy gave different bytes\n");
    res = 1;
  }
  free_design(copy);
  free_design(d4);
  return res;
}

static int file_round_trip_shapes(arena *) {
  design *d = load_shapes_design();
  int res = check_round_trip(d);
  free_design(d);
  return res;
}

static int file_round_trip_jointed(arena *) {
  design *d = load_xml_design(JOINTED_XML, sizeof(JOINTED_XML) - 1);
  int res = 0;
  if (!has_shape(d, SHAPE_ROD) || !has_shape(d, SHAPE_WHEEL) ||
      !has_shape(d, SHAPE_BOX)) {
    printf("expected a rod, a wheel and a goal box\n");
    res = 1;
  }
  res |= check_round_trip(d);
  free_design(d);
  return res;
}

// Designs built with the editor never had block uids assigned, so the file
// must not depend on them.
static int file_round_trip_editor(arena *a) {
  init(a);
  press(a, KEY_R);
  move(a, 400, 400);
  mouse_down(a);
  move(a, 500, 430);
  mouse_up(a);
  press(a, KEY_R);
  move(a, 500, 430);
  frame(a);
  mouse_down(a);
  move(a, 600, 380);
  mouse_up(a);
  press(a, KEY_U);
  move(a, 600, 380);
  frame(a);
  mouse_down(a);
  mouse_up(a);
  if (a->design.design_blocks.head == nullptr) {
    printf("expected the editor to have drawn blocks\n");
    return 1;
  }
  return check_round_trip(&a->design);
}

// The comparator has to be able to fail, or the tests above prove nothing.
static int comparator_detects_differences(arena *) {
  design *a = load_xml_design(JOINTED_XML, sizeof(JOINTED_XML) - 1);
  design *b = clean_copy_design(a);
  int res = 0;
  quiet_diffs = true; // the differences below are expected
  if (diff_designs(a, b) != 0) {
    printf("a clean copy should compare equal\n");
    res = 1;
  }
  // flip the lowest bit of a joint coordinate
  uint64_t bits;
  memcpy(&bits, &b->joints.head->x, sizeof bits);
  bits ^= 1;
  memcpy(&b->joints.head->x, &bits, sizeof bits);
  if (diff_designs(a, b) == 0) {
    printf("a one-bit change in a joint went unnoticed\n");
    res = 1;
  }
  free_design(b);
  b = clean_copy_design(a);
  b->goal_area.expand += 1;
  if (diff_designs(a, b) == 0) {
    printf("a change in the goal area's expand went unnoticed\n");
    res = 1;
  }
  free_design(b);
  b = clean_copy_design(a);
  for (block *k = b->design_blocks.head; k != nullptr; k = k->next) {
    if (k->shape.type != SHAPE_ROD)
      continue;
    joint *saved = k->shape.rod.to;
    k->shape.rod.to = k->shape.rod.from; // repoint one rod end
    if (diff_designs(a, b) == 0) {
      printf("a repointed rod end went unnoticed\n");
      res = 1;
    }
    k->shape.rod.to = saved; // so free_design sees a consistent design
    break;
  }
  free_design(b);
  free_design(a);
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
    {"file_round_trip_shapes", file_round_trip_shapes},
    {"file_round_trip_jointed", file_round_trip_jointed},
    {"file_round_trip_editor", file_round_trip_editor},
    {"comparator_detects_differences", comparator_detects_differences},
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
