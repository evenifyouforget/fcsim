#ifndef __XML_H__
#define __XML_H__

enum {
  XML_STATIC_RECTANGLE,
  XML_STATIC_CIRCLE,
  XML_DYNAMIC_RECTANGLE,
  XML_DYNAMIC_CIRCLE,
  XML_JOINTED_DYNAMIC_RECTANGLE,
  XML_NO_SPIN_WHEEL,
  XML_CLOCKWISE_WHEEL,
  XML_COUNTER_CLOCKWISE_WHEEL,
  XML_SOLID_ROD,
  XML_HOLLOW_ROD,
};

struct xml_position {
  double x;
  double y;
};

struct xml_joint {
  int id;
  struct xml_joint *next;
};

struct xml_block {
  int type;
  int id;
  double rotation;
  struct xml_position position;
  double width;
  double height;
  int goal_block;
  struct xml_joint *joints;
  /* Set only by the binary design file (src/design_file.c), which says
   * exactly which joint each rod end (slot 0 = left, 1 = right) or wheel
   * centre (slot 0) is connected to: a block id and a joint index within
   * that block (get_block_joints order), or -1. XML only has `joints`. */
  int has_joint_refs;
  int joint_ref_block[2];
  int joint_ref_joint[2];
  double rod_ends[4]; /* binary file rods: x0, y0, x1, y1 (left, right) */

  struct xml_block *next;
};

struct xml_zone {
  struct xml_position position;
  double width;
  double height;
};

struct xml_level {
  struct xml_block *level_blocks;
  struct xml_block *player_blocks;
  struct xml_zone start;
  struct xml_zone end;
  int level_id;
};

int xml_parse(char *xml, int len, struct xml_level *level);
void xml_free(struct xml_level *level);

#endif
