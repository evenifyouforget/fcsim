#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <fpmath/fpmath.h>
#include <box2d/b2Body.h>
#include <box2d/b2World.h>
#include <box2d/b2RevoluteJoint.h>

#include "graph.h"

static bool share_joint(struct block *block1, struct block *block2)
{
	struct joint *j1[5];
	struct joint *j2[5];
	int n1, n2;
	int i1, i2;

	n1 = get_block_joints(block1, j1);
	n2 = get_block_joints(block2, j2);

	for (i1 = 0; i1 < n1; i1++) {
		for (i2 = 0; i2 < n2; i2++) {
			if (j1[i1] == j2[i2])
				return true;
		}
	}

	return false;
}

bool collision_filter(b2Shape *s1, b2Shape *s2)
{
	struct block *block1;
	struct block *block2;

	block1 = (struct block *)b2Shape_GetUserData(s1);
	block2 = (struct block *)b2Shape_GetUserData(s2);

	if (!(block1->material->collision_bit & block2->material->collision_mask))
		return false;

	if (share_joint(block1, block2))
		return false;

	return true;
}

static double distance(double x1, double y1, double x2, double y2)
{
	double dx = x2 - x1;
	double dy = y2 - y1;

	return sqrt(dx * dx + dy * dy);
}

static void get_rect_shell(struct shell *shell, struct rect *rect)
{
	shell->type = SHELL_RECT;
	shell->rect.w = rect->w;
	shell->rect.h = rect->h;
	shell->x = rect->x;
	shell->y = rect->y;
	shell->angle = rect->angle;
}

static void get_circ_shell(struct shell *shell, struct circ *circ)
{
	shell->type = SHELL_CIRC;
	shell->circ.radius = circ->radius;
	shell->x = circ->x;
	shell->y = circ->y;
	shell->angle = 0.0;
}

static void get_box_shell(struct shell *shell, struct box *box)
{
	shell->type = SHELL_RECT;
	shell->rect.w = box->w;
	shell->rect.h = box->h;
	shell->x = box->x;
	shell->y = box->y;
	shell->angle = box->angle;
}

static void get_wheel_shell(struct shell *shell, struct wheel *wheel)
{
	shell->type = SHELL_CIRC;
	shell->circ.radius = wheel->radius;
	shell->x = wheel->center->x;
	shell->y = wheel->center->y;
	shell->angle = wheel->angle;
}

static void get_rod_shell(struct shell *shell, struct rod *rod)
{
	double x0 = rod->from->x;
	double y0 = rod->from->y;
	double x1 = rod->to->x;
	double y1 = rod->to->y;

	shell->type = SHELL_RECT;
	shell->rect.w = distance(x0, y0, x1, y1);
	shell->rect.h = rod->width;
	shell->x = x0 + (x1 - x0) / 2.0;
	shell->y = y0 + (y1 - y0) / 2.0;
	shell->angle = fp_atan2(y1 - y0, x1 - x0);
}

void get_shell(struct shell *shell, struct shape *shape)
{
	switch (shape->type) {
	case SHAPE_RECT:
		get_rect_shell(shell, &shape->rect);
		break;
	case SHAPE_CIRC:
		get_circ_shell(shell, &shape->circ);
		break;
	case SHAPE_BOX:
		get_box_shell(shell, &shape->box);
		break;
	case SHAPE_ROD:
		get_rod_shell(shell, &shape->rod);
		break;
	case SHAPE_WHEEL:
		get_wheel_shell(shell, &shape->wheel);
		break;
	}
}

void gen_block(b2World *world, struct block *block)
{
	struct material *mat = block->material;
	struct shell shell;
	b2BoxDef box_def;
	b2CircleDef circle_def;
	b2ShapeDef *shape_def;
	b2BodyDef body_def;

	b2BoxDef_ctor(&box_def);
	b2CircleDef_ctor(&circle_def);
	b2BodyDef_ctor(&body_def);

	get_shell(&shell, &block->shape);

	if (shell.type == SHELL_CIRC) {
		circle_def.radius = shell.circ.radius;
		shape_def = &circle_def.m_shapeDef;
	} else {
		box_def.extents.x = shell.rect.w / 2;
		box_def.extents.y = shell.rect.h / 2;
		shape_def = &box_def.m_shapeDef;
	}
	shape_def->density = mat->density;
	shape_def->friction = mat->friction;
	shape_def->restitution = mat->restitution;
	shape_def->userData = block;
	body_def.position.x = shell.x;
	body_def.position.y = shell.y;
	body_def.rotation = shell.angle;
	body_def.linearDamping = mat->linear_damping;
	body_def.angularDamping = mat->angular_damping;
	b2BodyDef_AddShape(&body_def, shape_def);

	block->body = b2World_CreateBody(world, &body_def);
}

static void gen_joint(b2World *world, b2Body *b1, b2Body *b2, double x, double y, int spin)
{
	b2RevoluteJointDef joint_def;

	b2RevoluteJointDef_ctor(&joint_def);
	joint_def.m_jointDef.body1 = b1;
	joint_def.m_jointDef.body2 = b2;
	joint_def.anchorPoint.x = x;
	joint_def.anchorPoint.y = y;
	joint_def.m_jointDef.collideConnected = true;
	if (spin != 0) {
		joint_def.motorTorque = 50000000;
		joint_def.motorSpeed = spin;
		joint_def.enableMotor = true;
	}
	b2World_CreateJoint(world, &joint_def.m_jointDef);
}

static int block_spin(struct block *block)
{
	if (block->shape.type != SHAPE_WHEEL)
		return 0;

	return block->shape.wheel.spin;
}

static void gen_joint_stack(b2World *world, struct joint *joint)
{
	struct attach_node *node = joint->att.head;
	double x = joint->x;
	double y = joint->y;

	if (!node)
		return;

	if (joint->gen) {
		gen_joint(world, joint->gen->body, node->block->body,
			  x, y, block_spin(node->block));
	} else if (node->next) {
		gen_joint(world, node->block->body, node->next->block->body,
			  x, y, block_spin(node->next->block) - block_spin(node->block));
		node = node->next;
	}

	for (; node->next; node = node->next) {
		gen_joint(world, node->block->body, node->next->block->body,
			  x, y, block_spin(node->next->block));
	}
}

b2World *gen_world(struct design *design)
{
	b2World *world = malloc(sizeof(*world));
	b2Vec2 gravity;
	b2AABB aabb;
	struct block *block;
	struct joint *joint;

	gravity.x = 0;
	gravity.y = 0;
	aabb.minVertex.x = -2000;
	aabb.minVertex.y = -1450;
	aabb.maxVertex.x = 2000;
	aabb.maxVertex.y = 1450;
	b2World_ctor(world, &aabb, gravity, true);
	b2World_SetFilter(world, collision_filter);

	for (block = design->player_blocks.head; block; block = block->next)
		gen_block(world, block);

	for (block = design->level_blocks.head; block; block = block->next)
		gen_block(world, block);

	for (joint = design->joints.head; joint; joint = joint->next)
		gen_joint_stack(world, joint);

	return world;
}

void free_world(b2World *world, struct design *design)
{
	struct block *block;

	b2World_dtor(world);
	free(world);

	for (block = design->player_blocks.head; block; block = block->next)
		block->body = NULL;

	for (block = design->level_blocks.head; block; block = block->next)
		block->body = NULL;
}

void step(struct b2World *world)
{
	b2World_Step(world, 1.0 / 30.0, 10);

	b2Joint *joint = b2World_GetJointList(world);
	while (joint) {
		b2Joint *next = joint->m_next;
		b2Vec2 a1 = joint->GetAnchor1(joint);
		b2Vec2 a2 = joint->GetAnchor2(joint);
		if (fabs(a1.x - a2.x) + fabs(a1.y - a2.y) > 50.0)
			b2World_DestroyJoint(world, joint);
		joint = next;
	}
}
