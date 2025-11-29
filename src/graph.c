#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <fpmath/fpmath.h>
#include "xml.h"
#include "graph.h"

const float static_r = 0.000f;
const float static_g = 0.745f;
const float static_b = 0.004f;

struct material static_env_material = {
	.density = 0.0,
	.friction = 0.7,
	.restitution = 0.0,
	.linear_damping = 0.0,
	.angular_damping = 0.0,
	.collision_bit = ENV_COLLISION_BIT,
	.collision_mask = ENV_COLLISION_MASK,
};

const float dynamic_r = 0.976f;
const float dynamic_g = 0.855f;
const float dynamic_b = 0.184f;

struct material dynamic_env_material = {
	.density = 1.0,
	.friction = 0.7,
	.restitution = 0.2,
	.linear_damping = 0.0,
	.angular_damping = 0.0,
	.collision_bit = ENV_COLLISION_BIT,
	.collision_mask = ENV_COLLISION_MASK,
};

const float wheel_r = 0.537f;
const float wheel_g = 0.980f;
const float wheel_b = 0.890f;

const float cw_wheel_r = 1.000f;
const float cw_wheel_g = 0.925f;
const float cw_wheel_b = 0.000f;

const float ccw_wheel_r = 1.000f;
const float ccw_wheel_g = 0.800f;
const float ccw_wheel_b = 0.800f;

struct material solid_material = {
	.density = 1.0,
	.friction = 0.7,
	.restitution = 0.2,
	.linear_damping = 0.0,
	.angular_damping = 0.0,
	.collision_bit = SOLID_COLLISION_BIT,
	.collision_mask = SOLID_COLLISION_MASK,
};

const float solid_rod_r = 0.420f;
const float solid_rod_g = 0.204f;
const float solid_rod_b = 0.000f;

struct material solid_rod_material = {
	.density = 1.0,
	.friction = 0.7,
	.restitution = 0.2,
	.linear_damping = 0.009,
	.angular_damping = 0.2,
	.collision_bit = SOLID_COLLISION_BIT,
	.collision_mask = SOLID_COLLISION_MASK,
};

const float water_rod_r = 0.000f;
const float water_rod_g = 0.000f;
const float water_rod_b = 1.000f;

struct material water_rod_material = {
	.density = 1.0,
	.friction = 0.7,
	.restitution = 0.2,
	.linear_damping = 0.009,
	.angular_damping = 0.2,
	.collision_bit = WATER_COLLISION_BIT,
	.collision_mask = WATER_COLLISION_MASK,
};

static void init_block_list(struct block_list *list)
{
	list->head = NULL;
	list->tail = NULL;
}

static void init_joint_list(struct joint_list *list)
{
	list->head = NULL;
	list->tail = NULL;
}

static void init_attach_list(struct attach_list *list)
{
	list->head = NULL;
	list->tail = NULL;
}

uint32_t piece_color_table[FCSIM_NUM_TYPES][2 * FCSIM_NUM_PALETTES] = {
	{0xff007f09, 0xff01be02, 0xff13e722, 0xff1b6c1b, 0xffdbe3ec, 0xff14243c}, // FCSIM_STAT_RECT
	{0xff007f09, 0xff01be02, 0xff13e722, 0xff1b6c1b, 0xffdbe3ec, 0xff14243c}, // FCSIM_STAT_CIRCLE
	{0xffc5550e, 0xfff8dc2f, 0xffdb8414, 0xff573b25, 0xfff2c790, 0xff202327}, // FCSIM_DYN_RECT
	{0xffbd591b, 0xfff98931, 0xffdb8414, 0xff573b25, 0xfff2c790, 0xff202327}, // FCSIM_DYN_CIRCLE
	{0xffb86461, 0xfffe6766, 0xffb4231e, 0xff3f2929, 0xffe7796a, 0xff2d1431}, // FCSIM_GOAL_RECT
	{0xffb86461, 0xfffe6766, 0xffb4231e, 0xff3f2929, 0xffe7796a, 0xff2d1431}, // FCSIM_GOAL_CIRCLE
	{0xffb86461, 0xfffe6766, 0xffb4231e, 0xff3f2929, 0xffe7796a, 0xff2d1431}, // FCSIM_CW_GOAL_CIRCLE
	{0xffb86461, 0xfffe6766, 0xffb4231e, 0xff3f2929, 0xffe7796a, 0xff2d1431}, // FCSIM_CCW_GOAL_CIRCLE
	{0xff0b6afc, 0xff8cfce4, 0xff1261db, 0xff484e4f, 0xffa4dcdc, 0xff43435c}, // FCSIM_WHEEL
	{0xfffd8003, 0xffffed00, 0xffe9de02, 0xff484e4f, 0xffeaec8f, 0xff43435c}, // FCSIM_CW_WHEEL
	{0xffce49a3, 0xffffcfce, 0xffe7e810, 0xff484e4f, 0xffe7aee6, 0xff43435c}, // FCSIM_CCW_WHEEL
	{0xfffeffff, 0xff0001fe, 0xff100525, 0xffe315b4, 0xff2c1466, 0xffb59ada}, // FCSIM_ROD
	{0xffb55a04, 0xff6a3502, 0xff484e4f, 0xff929c9e, 0xffa0e7ac, 0xff43435c}, // FCSIM_SOLID_ROD
	{0x00ffffff, 0xff808080, 0x00ffffff, 0xffc2c6c6, 0x00ffffff, 0xffc1c3c3}, // FCSIM_JOINT
	{0xff7878ee, 0xffbedcf8, 0xff9393f5, 0xff0e0b54, 0xff2d22e1, 0xff060e2d}, // FCSIM_BUILD_AREA
	{0xffbc6667, 0xfff29291, 0xfff97b7d, 0xff5b100f, 0xffe42512, 0xff1b100d}, // FCSIM_GOAL_AREA
	{0x00000000, 0xff87bdf1, 0x00000000, 0xff0c0c0c, 0x00000000, 0xff25487e}, // FCSIM_SKY
	{0xff404886, 0xff3a3b54, 0xffa0a2ba, 0xff4b559b, 0xffa1b1c8, 0xff213b64}, // FCSIM_UI_BUTTON
	{0xffb8100b, 0xffcf42fe, 0xffcb0330, 0xff5c0c1c, 0xffcb0330, 0xff5c0c1c}, // FCSIM_GOAL_RECT_OVERTURNED
// FCSIM_NUM_TYPES
};
uint32_t piece_color_palette_offset = 0;

// no dark mode detection for native
#ifndef __wasm__
int is_dark_mode() {
	return 0;
}
#endif

void get_color_by_type(int type_id, int slot, struct color * c) {
	uint32_t argb = piece_color_table[type_id][slot + 2 * piece_color_palette_offset];
	uint32_t b = argb & 0xff;
	uint32_t g = (argb >> 8) & 0xff;
	uint32_t r = (argb >> 16) & 0xff;
	uint32_t a = (argb >> 24) & 0xff;
	const float one_over_255 = 0.00392156862745098;
	c->a = a * one_over_255;
	c->r = r * one_over_255;
	c->g = g * one_over_255;
	c->b = b * one_over_255;
}

void get_color_by_block(struct block * bl, int slot, struct color * c) {
	get_color_by_type(bl->type_id, slot, c);
}

void append_block(struct block_list *list, struct block *block)
{
	if (list->tail) {
		list->tail->next = block;
		block->prev = list->tail;
		list->tail = block;
	} else {
		list->head = block;
		list->tail = block;
	}
}

void append_joint(struct joint_list *list, struct joint *joint)
{
	if (list->tail) {
		list->tail->next = joint;
		joint->prev = list->tail;
		list->tail = joint;
	} else {
		list->head = joint;
		list->tail = joint;
	}
}

void append_attach_node(struct attach_list *list, struct attach_node *node)
{
	if (list->tail) {
		list->tail->next = node;
		node->prev = list->tail;
		list->tail = node;
	} else {
		list->head = node;
		list->tail = node;
	}
}

void remove_attach_node(struct attach_list *list, struct attach_node *node)
{
	struct attach_node *next = node->next;
	struct attach_node *prev = node->prev;

	if (next)
		next->prev = prev;
	else
		list->tail = prev;

	if (prev)
		prev->next = next;
	else
		list->head = next;

	node->next = NULL;
	node->prev = NULL;
}

void remove_block(struct block_list *list, struct block *block)
{
	struct block *next = block->next;
	struct block *prev = block->prev;

	if (next)
		next->prev = prev;
	else
		list->tail = prev;

	if (prev)
		prev->next = next;
	else
		list->head = next;

	block->next = NULL;
	block->prev = NULL;
}

void remove_joint(struct joint_list *list, struct joint *joint)
{
	struct joint *next = joint->next;
	struct joint *prev = joint->prev;

	if (next)
		next->prev = prev;
	else
		list->tail = prev;

	if (prev)
		prev->next = next;
	else
		list->head = next;

	joint->next = NULL;
	joint->prev = NULL;
}

struct attach_node *new_attach_node(struct block *block)
{
	struct attach_node *node = malloc(sizeof(*node));

	node->prev = NULL;
	node->next = NULL;
	node->block = block;

	return node;
}

struct joint *new_joint(struct block *gen, double x, double y)
{
	struct joint *joint = malloc(sizeof(*joint));

	joint->prev = NULL;
	joint->next = NULL;
	joint->gen = gen;
	joint->x = x;
	joint->y = y;
	joint->visited = false;
	init_attach_list(&joint->att);

	return joint;
}

static void init_rect(struct shape *shape, struct xml_block *xml_block)
{
	shape->type = SHAPE_RECT;
	shape->rect.x = xml_block->position.x;
	shape->rect.y = xml_block->position.y;
	shape->rect.w = xml_block->width;
	shape->rect.h = xml_block->height;
	shape->rect.angle = xml_block->rotation;
}

static void init_circ(struct shape *shape, struct xml_block *xml_block)
{
	shape->type = SHAPE_CIRC;
	shape->circ.x = xml_block->position.x;
	shape->circ.y = xml_block->position.y;
	shape->circ.radius = xml_block->width;
}

static void add_box(struct design *design, struct block *block, struct xml_block *xml_block)
{
	struct shape *shape = &block->shape;

	double x = xml_block->position.x;
	double y = xml_block->position.y;
	double w_half = xml_block->width/2;
	double h_half = xml_block->height/2;

	double x0 =  fp_cos(xml_block->rotation) * w_half;
	double y0 =  fp_sin(xml_block->rotation) * w_half;
	double x1 =  fp_sin(xml_block->rotation) * h_half;
	double y1 = -fp_cos(xml_block->rotation) * h_half;

	shape->type = SHAPE_BOX;
	shape->box.x = xml_block->position.x;
	shape->box.y = xml_block->position.y;
	shape->box.w = xml_block->width;
	shape->box.h = xml_block->height;
	shape->box.angle = xml_block->rotation;

	shape->box.center = new_joint(block, x, y);
	append_joint(&design->joints, shape->box.center);

	shape->box.corners[0] = new_joint(block, x + x0 + x1, y + y0 + y1);
	append_joint(&design->joints, shape->box.corners[0]);

	shape->box.corners[1] = new_joint(block, x - x0 + x1, y - y0 + y1);
	append_joint(&design->joints, shape->box.corners[1]);

	shape->box.corners[2] = new_joint(block, x + x0 - x1, y + y0 - y1);
	append_joint(&design->joints, shape->box.corners[2]);

	shape->box.corners[3] = new_joint(block, x - x0 - x1, y - y0 - y1);
	append_joint(&design->joints, shape->box.corners[3]);
}

static void get_rod_endpoints(struct xml_block *xml_block,
			      double *x0, double *y0,
			      double *x1, double *y1)
{
	double w_half = xml_block->width / 2;
	double cw = fp_cos(xml_block->rotation) * w_half;
	double sw = fp_sin(xml_block->rotation) * w_half;
	double x = xml_block->position.x;
	double y = xml_block->position.y;

	*x0 = x - cw;
	*y0 = y - sw;
	*x1 = x + cw;
	*y1 = y + sw;
}

int get_block_joints(struct block *block, struct joint **res)
{
	struct shape *shape = &block->shape;
	int i;

	switch (shape->type) {
	case SHAPE_BOX:
		res[0] = shape->box.center;
		for (i = 0; i < 4; i++)
			res[i+1] = shape->box.corners[i];
		return 5;
	case SHAPE_ROD:
		res[0] = shape->rod.from;
		res[1] = shape->rod.to;
		return 2;
	case SHAPE_WHEEL:
		res[0] = shape->wheel.center;
		for (i = 0; i < 4; i++)
			res[i+1] = shape->wheel.spokes[i];
		return 5;
	default:
		return 0;
	}
}

static struct block *find_block(struct block_list *blocks, int id)
{
	struct block *block;

	for (block = blocks->head; block; block = block->next) {
		if (block->id == id)
			return block;
	}

	return NULL;
}

static void make_joints_list(struct attach_list *list,
			     struct block_list *blocks,
			     struct xml_joint *block_ids)
{
	struct block *block;
	struct attach_node *node;

	init_attach_list(list);

	for (; block_ids; block_ids = block_ids->next) {
		block = find_block(blocks, block_ids->id);
		if (block) {
			node = new_attach_node(block);
			append_attach_node(list, node);
		}
	}
}

static void destroy_joints_list(struct attach_list *list)
{
	struct attach_node *node;
	struct attach_node *next;

	node = list->head;
	while (node) {
		next = node->next;
		free(node);
		node = next;
	}
}

static double distance(double x1, double y1, double x2, double y2)
{
	double dx = x2 - x1;
	double dy = y2 - y1;

	return sqrt(dx * dx + dy * dy);
}

static struct joint *find_closest_joint(struct attach_list *list, double x, double y)
{
	double best_dist = 10.0;
	struct joint *best_joint = NULL;
	struct attach_node *att;

	for (att = list->head; att; att = att->next) {
		struct joint *joints[5];
		int joint_cnt;
		int i;

		joint_cnt = get_block_joints(att->block, joints);

		for (i = 0; i < joint_cnt; i++) {
			double dist = distance(joints[i]->x, joints[i]->y, x, y);
			if (dist < best_dist) {
				best_dist = dist;
				best_joint = joints[i];
			}
		}
	}

	return best_joint;
}

static bool block_has_joint(struct block *block, struct joint *joint)
{
	struct joint *j[5];
	int n;
	int i;

	n = get_block_joints(block, j);

	for (i = 0; i < n; i++) {
		if (j[i] == joint)
			return true;
	}

	return false;
}


bool share_block(struct design *design, struct joint *j1, struct joint *j2)
{
	struct block *block;

	for (block = design->player_blocks.head; block; block = block->next) {
		if (block_has_joint(block, j1) && block_has_joint(block, j2))
			return true;
	}

	return false;
}


static void add_rod(struct design *design, struct block *block, struct xml_block *xml_block)
{
	struct shape *shape = &block->shape;
	struct attach_list att_list;
	struct joint *j0, *j1;
	struct attach_node *att0, *att1;
	double x0, y0;
	double x1, y1;

	get_rod_endpoints(xml_block, &x0, &y0, &x1, &y1);

	make_joints_list(&att_list, &design->player_blocks, xml_block->joints);
	j0 = find_closest_joint(&att_list, x0, y0);
	j1 = find_closest_joint(&att_list, x1, y1);
	destroy_joints_list(&att_list);

	if (j0 && j1) {
		if (j0 == j1 || share_block(design, j0, j1))
			j1 = NULL;
	}

	if (j0) {
		x0 = j0->x;
		y0 = j0->y;
	} else {
		j0 = new_joint(NULL, x0, y0);
		append_joint(&design->joints, j0);
	}
	att0 = new_attach_node(block);
	append_attach_node(&j0->att, att0);

	if (j1) {
		x1 = j1->x;
		y1 = j1->y;
	} else {
		j1 = new_joint(NULL, x1, y1);
		append_joint(&design->joints, j1);
	}
	att1 = new_attach_node(block);
	append_attach_node(&j1->att, att1);

	shape->type = SHAPE_ROD;
	shape->rod.from = j0;
	shape->rod.from_att = att0;
	shape->rod.to = j1;
	shape->rod.to_att = att1;
	shape->rod.width = (xml_block->type == XML_SOLID_ROD ? 8 : 4);
}

static void add_wheel(struct design *design, struct block *block, struct xml_block *xml_block)
{
	struct shape *shape = &block->shape;
	struct attach_list att_list;
	struct joint *j0;
	struct attach_node *att0;
	double x0, y0;

	x0 = xml_block->position.x;
	y0 = xml_block->position.y;

	make_joints_list(&att_list, &design->player_blocks, xml_block->joints);
	j0 = find_closest_joint(&att_list, x0, y0);
	destroy_joints_list(&att_list);

	if (j0) {
		x0 = j0->x;
		y0 = j0->y;
	} else {
		j0 = new_joint(NULL, x0, y0);
		append_joint(&design->joints, j0);
	}
	att0 = new_attach_node(block);
	append_attach_node(&j0->att, att0);

	shape->type = SHAPE_WHEEL;
	shape->wheel.center = j0;
	shape->wheel.center_att = att0;
	shape->wheel.radius = xml_block->width / 2;
	shape->wheel.angle = xml_block->rotation;

	if (xml_block->type == XML_NO_SPIN_WHEEL)
		shape->wheel.spin = 0;
	else if (xml_block->type == XML_CLOCKWISE_WHEEL)
		shape->wheel.spin = 5;
	else
		shape->wheel.spin = -5;

	double a[4] = {
		0.0,
		3.141592653589793 / 2,
		3.141592653589793,
		4.71238898038469,
	};
	double spoke_x, spoke_y;
	int i;

	for (i = 0; i < 4; i++) {
		spoke_x = x0 + fp_cos(xml_block->rotation + a[i]) * xml_block->width / 2;
		spoke_y = y0 + fp_sin(xml_block->rotation + a[i]) * xml_block->width / 2;
		shape->wheel.spokes[i] = new_joint(block, spoke_x, spoke_y);
		append_joint(&design->joints, shape->wheel.spokes[i]);
	}
}

static void add_level_block(struct design *design, struct xml_block *xml_block)
{
	struct block *block = malloc(sizeof(*block));

	block->prev = NULL;
	block->next = NULL;

	switch (xml_block->type) {
	case XML_STATIC_RECTANGLE:
		init_rect(&block->shape, xml_block);
		block->material = &static_env_material;
		block->type_id = FCSIM_STAT_RECT;
		break;
	case XML_DYNAMIC_RECTANGLE:
		init_rect(&block->shape, xml_block);
		block->material = &dynamic_env_material;
		block->type_id = FCSIM_DYN_RECT;
		break;
	case XML_STATIC_CIRCLE:
		init_circ(&block->shape, xml_block);
		block->material = &static_env_material;
		block->type_id = FCSIM_STAT_CIRCLE;
		break;
	case XML_DYNAMIC_CIRCLE:
		init_circ(&block->shape, xml_block);
		block->material = &dynamic_env_material;
		block->type_id = FCSIM_DYN_CIRCLE;
		break;
	}

	block->goal = false;
	block->id = xml_block->id;
	block->body = NULL;
	block->overlap = false;
	block->visited = false;

	append_block(&design->level_blocks, block);
}

static void add_player_block(struct design *design, struct xml_block *xml_block)
{
	struct block *block = malloc(sizeof(*block));

	block->prev = NULL;
	block->next = NULL;

	switch (xml_block->type) {
	case XML_JOINTED_DYNAMIC_RECTANGLE:
		add_box(design, block, xml_block);
		block->material = &solid_material;
		block->goal = true;
		block->type_id = FCSIM_GOAL_RECT;
		break;
	case XML_SOLID_ROD:
		add_rod(design, block, xml_block);
		block->material = &solid_rod_material;
		block->goal = false;
		block->type_id = FCSIM_SOLID_ROD;
		break;
	case XML_HOLLOW_ROD:
		add_rod(design, block, xml_block);
		block->material = &water_rod_material;
		block->goal = false;
		block->type_id = FCSIM_ROD;
		break;
	case XML_NO_SPIN_WHEEL:
	case XML_CLOCKWISE_WHEEL:
	case XML_COUNTER_CLOCKWISE_WHEEL:
		add_wheel(design, block, xml_block);
		block->material = &solid_material;
		block->goal = xml_block->goal_block;
		// this covers 6 cases
		block->type_id = FCSIM_WHEEL - 3 * (block->goal) + (xml_block->type == XML_CLOCKWISE_WHEEL) + 2 * (xml_block->type == XML_COUNTER_CLOCKWISE_WHEEL);
		break;
	}

	block->id = xml_block->id;
	block->body = NULL;
	block->overlap = false;
	block->visited = false;

	append_block(&design->player_blocks, block);
}

void set_area(struct area *area, struct xml_zone *xml_zone, double expand)
{
	area->x = xml_zone->position.x;
	area->y = xml_zone->position.y;
	area->w = xml_zone->width;
	area->h = xml_zone->height;
	area->expand = expand;
}

void convert_xml(struct xml_level *xml_level, struct design *design)
{
	struct xml_block *block;

	init_joint_list(&design->joints);

	init_block_list(&design->level_blocks);
	for (block = xml_level->level_blocks; block; block = block->next)
		add_level_block(design, block);

	init_block_list(&design->player_blocks);
	for (block = xml_level->player_blocks; block; block = block->next)
		add_player_block(design, block);

	set_area(&design->build_area, &xml_level->start, 4.0);
	set_area(&design->goal_area, &xml_level->end, 0.0);

	design->level_id = xml_level->level_id;
}

static void free_attach_list(struct attach_list *list)
{
	struct attach_node *node;
	struct attach_node *next;

	node = list->head;

	while (node) {
		next = node->next;
		free(node);
		node = next;
	}
}

static void free_joint(struct joint *joint)
{
	free_attach_list(&joint->att);
	free(joint);
}

static void free_joint_list(struct joint_list *list)
{
	struct joint *joint;
	struct joint *next;

	joint = list->head;

	while (joint) {
		next = joint->next;
		free_joint(joint);
		joint = next;
	}
}

static void free_block_list(struct block_list *list)
{
	struct block *block;
	struct block *next;

	block = list->head;

	while (block) {
		next = block->next;
		free(block);
		block = next;
	}
}

void free_design(struct design *design)
{
	free_joint_list(&design->joints);
	free_block_list(&design->level_blocks);
	free_block_list(&design->player_blocks);
	free(design);
}