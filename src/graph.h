#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct b2Body b2Body;
struct b2Body;

typedef struct b2World b2World;
struct b2World;
struct color {
  float a;
  float r;
  float g;
  float b;
};

struct attach_node {
  struct attach_node *prev;
  struct attach_node *next;
  struct block *block;
};

struct attach_node *new_attach_node(struct block *block);

struct attach_list {
  struct attach_node *head;
  struct attach_node *tail;
};

void append_attach_node(struct attach_list *list, struct attach_node *node);
void remove_attach_node(struct attach_list *list, struct attach_node *node);

/* One element of a joint stack. See README §Joints and §Joint Stack.
 * Note: struct joint maps to a single point; the user-visible "joint"
 * is a joint stack (all joints connected by joint edges at the same location). */
struct joint {
  struct joint *prev;
  struct joint *next;
  struct block *gen; /* generating block: the block whose position drives this joint's position. NULL for free joints (wheel centres, rod endpoints). Will be removed when blocks own their joints. */
  double x, y;
  struct attach_list att; /* blocks that use this joint as a non-gen attachment point (e.g. rods, wheel body for spokes) */
  bool in_drag_set; /* true while this joint is in the affected set of an in-progress move operation; see README §Move Operation State */
  uint64_t _checksum_uid; /* TODO: document purpose */
};

struct joint *new_joint(struct block *gen, double x, double y);

struct joint_list {
  struct joint *head;
  struct joint *tail;
};

void append_joint(struct joint_list *list, struct joint *joint);
void remove_joint(struct joint_list *list, struct joint *joint);

struct rect {
  double x, y;
  double w, h;
  double angle;
};

struct circ {
  double x, y;
  double radius;
};

struct box {
  double x, y;
  double w, h;
  double angle;

  struct joint *center;
  struct joint *corners[4];
};

struct rod {
  struct joint *from; /* left-side joint (see README §Joints) */
  struct attach_node *from_att;
  struct joint *to; /* right-side joint */
  struct attach_node *to_att;
  double width; /* cross-section thickness (not length): 4.0 for water rods, 8.0 for solid rods */
};

struct wheel {
  struct joint *center; /* centre joint; gen=NULL (free joint) — the wheel's positional anchor */
  struct attach_node *center_att;
  double radius;
  double angle;
  int spin; /* 0 = no spin, 5 = clockwise, -5 = counter-clockwise */

  struct joint *spokes[4];
};

enum shape_type {
  SHAPE_RECT,
  SHAPE_CIRC,
  SHAPE_BOX,    /* goal rectangle — see README §Blocks */
  SHAPE_ROD,
  SHAPE_WHEEL,
};

struct shape {
  enum shape_type type;
  union {
    struct rect rect;
    struct circ circ;
    struct box box;
    struct rod rod;
    struct wheel wheel;
  };
};

#define ENV_COLLISION_BIT (1 << 0)
#define SOLID_COLLISION_BIT (1 << 1)
#define WATER_COLLISION_BIT (1 << 2)

#define ENV_COLLISION_MASK                                                     \
  (ENV_COLLISION_BIT | SOLID_COLLISION_BIT | WATER_COLLISION_BIT)
#define SOLID_COLLISION_MASK (ENV_COLLISION_BIT | SOLID_COLLISION_BIT)
#define WATER_COLLISION_MASK (ENV_COLLISION_BIT)

struct material {
  double density;
  double friction;
  double restitution;
  double linear_damping;
  double angular_damping;
  uint32_t collision_bit;
  uint32_t collision_mask;
};

extern struct material static_env_material;
extern struct material dynamic_env_material;
extern struct material solid_material;
extern struct material solid_rod_material;
extern struct material water_rod_material;

// mostly redundant
#define FCSIM_STAT_RECT 0
#define FCSIM_STAT_CIRCLE 1
#define FCSIM_DYN_RECT 2
#define FCSIM_DYN_CIRCLE 3
#define FCSIM_GOAL_RECT 4
#define FCSIM_GOAL_CIRCLE 5
#define FCSIM_CW_GOAL_CIRCLE 6
#define FCSIM_CCW_GOAL_CIRCLE 7
#define FCSIM_WHEEL 8
#define FCSIM_CW_WHEEL 9
#define FCSIM_CCW_WHEEL 10
#define FCSIM_ROD 11
#define FCSIM_SOLID_ROD 12
#define FCSIM_JOINT 13
#define FCSIM_BUILD_AREA 14
#define FCSIM_GOAL_AREA 15
#define FCSIM_SKY 16
#define FCSIM_UI_BUTTON 17
#define FCSIM_GOAL_RECT_OVERTURNED 18
#define FCSIM_NUM_TYPES 19
#define FCSIM_NUM_PALETTES 3

extern uint32_t piece_color_table[FCSIM_NUM_TYPES][2 * FCSIM_NUM_PALETTES];
extern uint32_t piece_color_palette_offset;

// does the host user prefer dark mode?
int is_dark_mode();

/* See README §Blocks. */
struct block {
  struct block *prev;
  struct block *next;
  struct shape shape;
  struct material *material;
  bool goal;
  bool overlap; /* true if this block currently intersects another block (set by mark_overlaps) */
  bool in_drag_set; /* true while this block is in the affected set of an in-progress move operation; see README §Move Operation State */
  int uid; /* unique ID; see README §Blocks. NOTE: not initialised for interactively created blocks — TODO known bug, may violate ordering invariant if merge_design is called before export reassigns UIDs */
  uint8_t type_id;
  b2Body *body;
};

void get_color_by_type(int, int, struct color *);
void get_color_by_block(struct block *, int, struct color *);

struct block_list {
  struct block *head;
  struct block *tail;
};

void append_block(struct block_list *list, struct block *block);
void remove_block(struct block_list *list, struct block *block);

/* See README §Areas. */
struct area {
  double x, y;
  double w, h;
  double expand; // added to both w and h, but only for geometry checks
  /* TODO: document what expand means */
};

/* See README §Designs. */
struct design {
  struct joint_list joints;
  struct block_list level_blocks;
  struct block_list design_blocks; /* all design blocks (both player blocks and goal blocks, distinguished by block->goal); corresponds to <playerBlocks> in XML */
  struct area build_area;
  struct area goal_area;
  int level_id; /* TODO: document purpose */
  int modcount;        /* cache invalidation counter; see README §Designs (Modcount) */
  int expect_checksum; /* TODO: document purpose */
  int actual_checksum; /* TODO: document purpose */
};

enum shell_type {
  SHELL_CIRC,
  SHELL_RECT,
};

struct shell_circ {
  double radius;
};

struct shell_rect {
  double w, h;
};

struct shell {
  enum shell_type type;
  union {
    struct shell_circ circ;
    struct shell_rect rect;
  };
  double x, y;
  double angle;
};

struct xml_level;

void convert_xml(struct xml_level *xml_level, struct design *design);
void free_design(struct design *design);

// merge additional design pieces from `src` into `dst` according to the
// conservative policy used by arena_init when multiple XML loads occur.
// See comments in arena.c for behaviour; only rods/wheels that are not goals
// are imported, their IDs offset, and all other data in `src` discarded.
void merge_design(struct design *dst, struct design *src);

b2World *gen_world(struct design *design);
void free_world(b2World *world, struct design *design);

void step(struct b2World *world);
void get_shell(struct shell *shell, struct shape *shape);
int get_block_joints(struct block *block, struct joint **res);
bool share_block(struct design *design, struct joint *j1, struct joint *j2);

// deep copy, except all b2body are set to nullptr
struct design *clean_copy_design(struct design *);

#endif
