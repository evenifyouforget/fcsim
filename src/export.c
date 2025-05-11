#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

#include "graph.h"
#include "str.h"

void u64tostr(char *buf, uint64_t val)
{
	char tmp[20];
	int l = 0;
	int i;

	do {
		tmp[l++] = '0' + (val % 10);
		val /= 10;
	} while (val > 0);

	for (i = 0; i < l; i++)
		buf[i] = tmp[l - 1 - i];

	buf[l] = 0;
}

char *dtostr_int(char *buf, double val)
{
	u64tostr(buf, val); // TODO: handle big numbers

	while (*buf)
		buf++;

	return buf;
}

void dtostr_frac(char *buf, double val)
{
	uint8_t tmp[18];
	uint64_t u;
	int l;
	int i;

	u = val * 1e18;
	if (u == 0)
		return;

	for (i = 0; i < 18; i++) {
		tmp[i] = u % 10;
		u /= 10;
	}

	for (i = 0; i < 18; i++) {
		if (tmp[i] != 0) {
			l = i;
			break;
		}
	}

	*buf++ = '.';
	for (i = 17; i >= l; i--)
		*buf++ = '0' + tmp[i];
	*buf = 0;
}

void dtostr(char *buf, double val)
{
	double integer;
	double frac;

	if (val < 0) {
		*buf++ = '-';
		val = -val;
	}

	frac = modf(val, &integer);

	buf = dtostr_int(buf, integer);
	dtostr_frac(buf, frac);
}

void itostr(char *buf, int val)
{
	if (val < 0) {
		*buf = '-';
		buf++;
		val = -val;
	}

	u64tostr(buf, val);
}

void append_int(struct str *str, int x)
{
	char buf[12];

	itostr(buf, x);
	append_str(str, buf);
}

void append_double(struct str *str, double val)
{
	char buf[50];

	dtostr(buf, val);
	append_str(str, buf);
}

void update_block_ids(struct design *design)
{
	struct block *block;
	int cnt = 1;

	for (block = design->level_blocks.head; block; block = block->next)
		block->id = -1;

	for (block = design->player_blocks.head; block; block = block->next)
		block->id = cnt++;
}

char *get_block_name(struct block *block)
{
	switch (block->shape.type) {
	case SHAPE_RECT:
		if (block->material->density == 0.0)
			return "StaticRectangle";
		else
			return "DynamicRectangle";
	case SHAPE_CIRC:
		if (block->material->density == 0.0)
			return "StaticCircle";
		else
			return "DynamicCircle";
	case SHAPE_BOX:
		return "JointedDynamicRectangle";
	case SHAPE_ROD:
		if (block->shape.rod.width <= 4.0)
			return "HollowRod";
		else
			return "SolidRod";
	case SHAPE_WHEEL:
		if (block->shape.wheel.spin == 0)
			return "NoSpinWheel";
		else if (block->shape.wheel.spin > 0)
			return "ClockwiseWheel";
		else
			return "CounterClockwiseWheel";
	}
	return NULL;
}

void append_jointed_to(struct str *str, struct block *block, struct joint *joint)
{
	struct block *other;

	if (!joint)
		return;

	if (joint->gen)
		other = joint->gen;
	else
		other = joint->att.head->block;

	if (other == block)
		return;
	if (other->id == -1) /* just in case */
		return;

	append_str(str, "<jointedTo>");
	append_int(str, other->id);
	append_str(str, "</jointedTo>");
}

void append_joints(struct str *str, struct block *block)
{
	struct joint *joints[2] = { NULL, NULL };
	int i;

	if (block->shape.type == SHAPE_ROD) {
		joints[0] = block->shape.rod.from;
		joints[1] = block->shape.rod.to;
	} else if (block->shape.type == SHAPE_WHEEL) {
		joints[0] = block->shape.wheel.center;
	}

	append_str(str, "<joints>");
	for (i = 0; i < 2; i++)
		append_jointed_to(str, block, joints[i]);
	append_str(str, "</joints>");
}

void _append_block(struct str *str, struct block *block)
{
	char *name;
	struct shell shell;

	name = get_block_name(block);
	if (!name)
		return;

	get_shell(&shell, &block->shape);

	append_str(str, "<");
	append_str(str, name);
	if (block->id != -1) {
		append_str(str, " id=\"");
		append_int(str, block->id);
		append_str(str, "\"");
	}
	append_str(str, ">");

	append_str(str, "<rotation>");
	append_double(str, shell.angle);
	append_str(str, "</rotation>");

	append_str(str, "<position>");
	append_str(str, "<x>");
	append_double(str, shell.x);
	append_str(str, "</x>");
	append_str(str, "<y>");
	append_double(str, shell.y);
	append_str(str, "</y>");
	append_str(str, "</position>");

	append_str(str, "<width>");
	if (shell.type == SHELL_RECT)
		append_double(str, shell.rect.w);
	else if (block->shape.type == SHAPE_WHEEL)
		append_double(str, shell.circ.radius * 2);
	else
		append_double(str, shell.circ.radius);
	append_str(str, "</width>");

	append_str(str, "<height>");
	if (shell.type == SHELL_RECT)
		append_double(str, shell.rect.h);
	else if (block->shape.type == SHAPE_WHEEL)
		append_double(str, shell.circ.radius * 2);
	else
		append_double(str, shell.circ.radius);
	append_str(str, "</height>");

	append_str(str, "<goalBlock>");
	if (block->goal)
		append_str(str, "true");
	else
		append_str(str, "false");
	append_str(str, "</goalBlock>");

	append_joints(str, block);

	append_str(str, "</");
	append_str(str, name);
	append_str(str, ">");
}

void append_block_list(struct str *str, struct block_list *list, char *name)
{
	struct block *block;

	append_str(str, "<");
	append_str(str, name);
	append_str(str, ">");

	for (block = list->head; block; block = block->next)
		_append_block(str, block);

	append_str(str, "</");
	append_str(str, name);
	append_str(str, ">");
}

void append_area(struct str *str, struct area *area, char *name)
{
	append_str(str, "<");
	append_str(str, name);
	append_str(str, ">");

	append_str(str, "<position>");
	append_str(str, "<x>");
	append_double(str, area->x);
	append_str(str, "</x>");
	append_str(str, "<y>");
	append_double(str, area->y);
	append_str(str, "</y>");
	append_str(str, "</position>");

	append_str(str, "<width>");
	append_double(str, area->w);
	append_str(str, "</width>");

	append_str(str, "<height>");
	append_double(str, area->h);
	append_str(str, "</height>");

	append_str(str, "</");
	append_str(str, name);
	append_str(str, ">");
}

char *export_design(struct design *design, char *user, char *name, char *desc)
{
	struct str str;

	update_block_ids(design);

	make_str(&str, 16);
	append_str(&str, "<saveDesign>");
	append_str(&str, "<name>");
	append_str(&str, name);
	append_str(&str, "</name>");
	append_str(&str, "<description>");
	append_str(&str, desc);
	append_str(&str, "</description>");
	append_str(&str, "<userId>");
	append_str(&str, user);
	append_str(&str, "</userId>");
	append_str(&str, "<levelId>");
	if (design->level_id >= 0)
		append_int(&str, design->level_id);
	append_str(&str, "</levelId>");
	append_str(&str, "<isSolution>0</isSolution>");
	append_str(&str, "<level>");
	append_block_list(&str, &design->level_blocks, "levelBlocks");
	append_block_list(&str, &design->player_blocks, "playerBlocks");
	append_area(&str, &design->build_area, "start");
	append_area(&str, &design->goal_area, "end");
	append_str(&str, "</level>");
	append_str(&str, "</saveDesign>");

	return str.mem;
}
