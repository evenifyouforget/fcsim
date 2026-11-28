#include "design_file.h"

#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "xml.h"

/* See README §Design File for the format. */

#define MAGIC "FCDS"
#define VERSION 1
#define HEADER_SIZE (4 + 2 + 4 + 8 * 8 + 4)
#define BLOCK_SIZE (1 + 5 * 8 + 2 * 2 + 1)
#define NO_BLOCK 0xFFFF
#define MAX_TYPE_ID FCSIM_SOLID_ROD

/* ---- little-endian primitives ---- */

static void put_u8(uint8_t **p, uint8_t v) { *(*p)++ = v; }

static void put_u16(uint8_t **p, uint16_t v) {
  put_u8(p, v & 0xFF);
  put_u8(p, v >> 8);
}

static void put_u32(uint8_t **p, uint32_t v) {
  put_u16(p, v & 0xFFFF);
  put_u16(p, v >> 16);
}

static void put_f64(uint8_t **p, double v) {
  uint64_t u;
  memcpy(&u, &v, 8);
  put_u32(p, (uint32_t)u);
  put_u32(p, (uint32_t)(u >> 32));
}

static uint8_t get_u8(const uint8_t **p) { return *(*p)++; }

static uint16_t get_u16(const uint8_t **p) {
  uint16_t lo = get_u8(p);
  return lo | (uint16_t)(get_u8(p) << 8);
}

static uint32_t get_u32(const uint8_t **p) {
  uint32_t lo = get_u16(p);
  return lo | ((uint32_t)get_u16(p) << 16);
}

static double get_f64(const uint8_t **p) {
  uint64_t lo = get_u32(p);
  uint64_t u = lo | ((uint64_t)get_u32(p) << 32);
  double v;
  memcpy(&v, &u, 8);
  return v;
}

static bool is_finite(double v) { return v - v == 0.0; }

/* ---- encoding ---- */

static int block_count(struct block_list *list) {
  int n = 0;
  struct block *b;
  for (b = list->head; b; b = b->next)
    n++;
  return n;
}

/* file index of a design block: level blocks come first */
static int file_index(struct design *design, struct block *block) {
  int i = block_count(&design->level_blocks);
  struct block *b;
  for (b = design->design_blocks.head; b; b = b->next, i++)
    if (b == block)
      return i;
  return -1;
}

/* the block and joint index that `joint` of `block` is connected to, as in
 * export.c's append_jointed_to */
static void joint_ref(struct design *design, struct block *block,
                      struct joint *joint, uint16_t *ref_block,
                      uint8_t *ref_joint) {
  struct joint *joints[MAX_BLOCK_JOINTS];
  struct block *other;
  int n, i, idx;

  *ref_block = NO_BLOCK;
  *ref_joint = 0;
  if (!joint)
    return;
  other = joint->gen ? joint->gen : joint->att.head->block;
  if (other == block)
    return;
  idx = file_index(design, other);
  if (idx < 0)
    return;
  n = get_block_joints(other, joints);
  for (i = 0; i < n; i++) {
    if (joints[i] == joint) {
      *ref_block = (uint16_t)idx;
      *ref_joint = (uint8_t)i;
      return;
    }
  }
}

static void put_block(uint8_t **p, struct design *design, struct block *block) {
  struct joint *slots[2] = {NULL, NULL};
  uint16_t ref_block[2];
  uint8_t ref_joint[2];
  struct shell shell;
  double w, h;
  int i;

  /* except for rods, the same values as the XML export (_append_block) */
  get_shell(&shell, &block->shape);
  if (shell.type == SHELL_RECT) {
    w = shell.rect.w;
    h = shell.rect.h;
  } else if (block->shape.type == SHAPE_WHEEL) {
    w = h = shell.circ.radius * 2;
  } else {
    w = h = shell.circ.radius;
  }

  if (block->shape.type == SHAPE_ROD) {
    slots[0] = block->shape.rod.from;
    slots[1] = block->shape.rod.to;
  } else if (block->shape.type == SHAPE_WHEEL) {
    slots[0] = block->shape.wheel.center;
  }
  for (i = 0; i < 2; i++)
    joint_ref(design, block, slots[i], &ref_block[i], &ref_joint[i]);

  put_u8(p, block->type_id);
  if (block->shape.type == SHAPE_ROD) {
    /* the endpoints exactly, rather than XML's centre, length and angle */
    put_f64(p, block->shape.rod.from->x);
    put_f64(p, block->shape.rod.from->y);
    put_f64(p, block->shape.rod.to->x);
    put_f64(p, block->shape.rod.to->y);
    put_f64(p, 0.0);
  } else {
    put_f64(p, shell.x);
    put_f64(p, shell.y);
    put_f64(p, w);
    put_f64(p, h);
    put_f64(p, shell.angle);
  }
  put_u16(p, ref_block[0]);
  put_u16(p, ref_block[1]);
  put_u8(p, (uint8_t)(ref_joint[0] | (ref_joint[1] << 4)));
}

static void put_area(uint8_t **p, struct area *area) {
  put_f64(p, area->x);
  put_f64(p, area->y);
  put_f64(p, area->w);
  put_f64(p, area->h);
}

uint8_t *design_file_encode(struct design *design, int *len) {
  int n =
      block_count(&design->level_blocks) + block_count(&design->design_blocks);
  uint8_t *buf, *p;
  struct block *b;

  *len = HEADER_SIZE + n * BLOCK_SIZE;
  buf = p = (uint8_t *)malloc(*len);

  memcpy(p, MAGIC, 4);
  p += 4;
  put_u16(&p, VERSION);
  put_u32(&p, (uint32_t)design->level_id);
  put_area(&p, &design->build_area);
  put_area(&p, &design->goal_area);
  put_u32(&p, (uint32_t)n);

  for (b = design->level_blocks.head; b; b = b->next)
    put_block(&p, design, b);
  for (b = design->design_blocks.head; b; b = b->next)
    put_block(&p, design, b);

  return buf;
}

/* ---- decoding ---- */

bool design_file_is_binary(const char *buf, int len) {
  return len >= 4 && memcmp(buf, MAGIC, 4) == 0;
}

/* XML block type and goal flag for each type ID (README §Blocks) */
static const int xml_types[MAX_TYPE_ID + 1] = {
    XML_STATIC_RECTANGLE,
    XML_STATIC_CIRCLE,
    XML_DYNAMIC_RECTANGLE,
    XML_DYNAMIC_CIRCLE,
    XML_JOINTED_DYNAMIC_RECTANGLE,
    XML_NO_SPIN_WHEEL,
    XML_CLOCKWISE_WHEEL,
    XML_COUNTER_CLOCKWISE_WHEEL,
    XML_NO_SPIN_WHEEL,
    XML_CLOCKWISE_WHEEL,
    XML_COUNTER_CLOCKWISE_WHEEL,
    XML_HOLLOW_ROD,
    XML_SOLID_ROD,
};

/* how many joints a block of this type and width has (get_block_joints) */
static int joint_count(int type_id, double width) {
  if (type_id == FCSIM_ROD || type_id == FCSIM_SOLID_ROD)
    return 2;
  if (type_id == FCSIM_GOAL_RECT)
    return 5;
  if (type_id >= FCSIM_GOAL_CIRCLE && type_id <= FCSIM_CCW_WHEEL)
    return width / 2 > 20 ? 9 : 5;
  return 0;
}

static int used_slots(int type_id) {
  if (type_id == FCSIM_ROD || type_id == FCSIM_SOLID_ROD)
    return 2;
  if (type_id >= FCSIM_GOAL_CIRCLE && type_id <= FCSIM_CCW_WHEEL)
    return 1;
  return 0;
}

static void append(struct xml_block **head, struct xml_block **tail,
                   struct xml_block *block) {
  if (*tail)
    (*tail)->next = block;
  else
    *head = block;
  *tail = block;
}

static void read_area(const uint8_t **p, struct xml_zone *zone) {
  zone->position.x = get_f64(p);
  zone->position.y = get_f64(p);
  zone->width = get_f64(p);
  zone->height = get_f64(p);
}

static int decode(const uint8_t *buf, int len, struct xml_level *level) {
  const uint8_t *p = buf;
  struct xml_block *level_tail = NULL, *player_tail = NULL;
  uint8_t *types;
  double *widths;
  uint32_t n, i;
  int k;

  if (len < HEADER_SIZE || memcmp(p, MAGIC, 4) != 0)
    return 1;
  p += 4;
  if (get_u16(&p) != VERSION)
    return 1;
  level->level_id = (int32_t)get_u32(&p);
  if (level->level_id < -1)
    return 1;
  read_area(&p, &level->start);
  read_area(&p, &level->end);
  n = get_u32(&p);
  if (n > NO_BLOCK || (uint32_t)(len - HEADER_SIZE) != n * BLOCK_SIZE)
    return 1;

  /* types and widths of earlier blocks, to check joint references */
  types = (uint8_t *)malloc(n + 1);
  widths = (double *)malloc((n + 1) * sizeof(double));

  for (i = 0; i < n; i++) {
    struct xml_block *block = (struct xml_block *)calloc(1, sizeof(*block));
    int type_id = get_u8(&p);
    double v[5];
    uint16_t ref_block[2];
    uint8_t ref_joints;

    for (k = 0; k < 5; k++)
      v[k] = get_f64(&p);
    ref_block[0] = get_u16(&p);
    ref_block[1] = get_u16(&p);
    ref_joints = get_u8(&p);

    if (type_id > MAX_TYPE_ID) {
      free(block);
      goto fail;
    }
    if (type_id <= FCSIM_DYN_CIRCLE)
      append(&level->level_blocks, &level_tail, block);
    else
      append(&level->player_blocks, &player_tail, block);

    block->type = xml_types[type_id];
    block->goal_block =
        type_id >= FCSIM_GOAL_RECT && type_id <= FCSIM_CCW_GOAL_CIRCLE;
    block->id = (int)i;
    block->position.x = v[0];
    block->position.y = v[1];
    block->width = v[2];
    block->height = v[3];
    block->rotation = v[4];
    for (k = 0; k < 4; k++)
      block->rod_ends[k] = v[k];
    for (k = 0; k < 5; k++)
      if (!is_finite(v[k]))
        goto fail;

    block->has_joint_refs = type_id > FCSIM_DYN_CIRCLE;
    for (k = 0; k < 2; k++) {
      int joint = (ref_joints >> (4 * k)) & 0xF;
      block->joint_ref_block[k] = -1;
      block->joint_ref_joint[k] = -1;
      if (ref_block[k] == NO_BLOCK) {
        if (joint != 0)
          goto fail;
        continue;
      }
      /* only rods and wheels have slots; a slot refers to a joint of an
       * earlier design block */
      if (k >= used_slots(type_id) || ref_block[k] >= i ||
          joint >= joint_count(types[ref_block[k]], widths[ref_block[k]]))
        goto fail;
      block->joint_ref_block[k] = ref_block[k];
      block->joint_ref_joint[k] = joint;
    }
    types[i] = (uint8_t)type_id;
    widths[i] = v[2];
  }

  free(types);
  free(widths);
  return 0;

fail:
  free(types);
  free(widths);
  return 1;
}

int design_file_decode(const uint8_t *buf, int len, struct xml_level *level) {
  memset(level, 0, sizeof(*level));
  level->level_id = -1;
  if (decode(buf, len, level) == 0)
    return 0;
  xml_free(level);
  memset(level, 0, sizeof(*level));
  level->level_id = -1;
  return 1;
}
