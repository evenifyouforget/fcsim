#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <box2d/b2Body.h>
#include <box2d/b2World.h>
#include <box2d/b2CMath.h>

#define ARENA_C
#include "gl.h"
#include "xml.h"
#include "interval.h"
#include "graph.h"
#include "text.h"
#include "arena.h"
#include "graph.h"

#define MAX_RENDER_TEXT_LENGTH 1000

const char *block_vertex_shader_src =
	"attribute vec2 a_coords;"
	"attribute vec3 a_color;"
	"varying vec3 v_color;"
	"uniform vec2 u_scale;"
	"uniform vec2 u_shift;"
	"void main() {"
		"gl_Position = vec4(a_coords * u_scale + u_shift, 0.0, 1.0);"
		"v_color = a_color;"
	"}";

const char *block_fragment_shader_src =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"varying vec3 v_color;"
	"void main() {"
		"gl_FragColor = vec4(v_color, 1.0);\n"
	"}\n";

GLuint block_program;
GLuint block_program_coord_attrib;
GLuint block_program_color_attrib;
GLuint block_program_scale_uniform;
GLuint block_program_shift_uniform;

const char *joint_vertex_shader_src =
	"attribute vec2 a_coords;"
	"void main() {"
		"gl_Position = vec4(a_coords, 0.0, 1.0);"
	"}";

const char *joint_fragment_shader_src =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"void main() {"
		"gl_FragColor = vec4(0.5, 0.5, 0.5, 1.0);\n"
	"}\n";

GLuint joint_program;
GLuint joint_program_coord_attrib;

void pixel_to_world(struct view *view, int x, int y, float *x_world, float *y_world)
{
	*x_world = view->x + view->scale * (2 * x - view->width);
	*y_world = view->y + view->scale * (2 * y - view->height);
}

void world_to_view(struct view *view, float x, float y, float *x_view, float *y_view)
{
	*x_view = (x - view->x) / (view->width * view->scale);
	*y_view = (view->y - y) / (view->height * view->scale);
}

bool arena_compile_shaders(void)
{
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLint value;

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &block_vertex_shader_src, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &value);
	if (!value)
		return false;

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &block_fragment_shader_src, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &value);
	if (!value)
		return false;

	block_program = glCreateProgram();
	glAttachShader(block_program, vertex_shader);
	glAttachShader(block_program, fragment_shader);
	glLinkProgram(block_program);
	glGetProgramiv(block_program, GL_LINK_STATUS, &value);
	if (!value)
		return false;

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	block_program_coord_attrib = glGetAttribLocation(block_program, "a_coords");
	block_program_color_attrib = glGetAttribLocation(block_program, "a_color");
	block_program_scale_uniform = glGetUniformLocation(block_program, "u_scale");
	block_program_shift_uniform = glGetUniformLocation(block_program, "u_shift");

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &joint_vertex_shader_src, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &value);
	if (!value)
		return false;

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &joint_fragment_shader_src, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &value);
	if (!value)
		return false;

	joint_program = glCreateProgram();
	glAttachShader(joint_program, vertex_shader);
	glAttachShader(joint_program, fragment_shader);
	glLinkProgram(joint_program);
	glGetProgramiv(block_program, GL_LINK_STATUS, &value);
	if (!value)
		return false;

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	joint_program_coord_attrib = glGetAttribLocation(joint_program, "a_coords");

	return true;
}

void arena_init(struct arena *arena, float w, float h, char *xml, int len)
{
	struct xml_level level;

	arena->view.x = 0.0f;
	arena->view.y = 0.0f;
	arena->view.width = w;
	arena->view.height = h;
	arena->view.scale = 1.0f;
	arena->cursor_x = 0;
	arena->cursor_y = 0;

	arena->shift = false;
	arena->ctrl = false;

	arena->tool = TOOL_MOVE;
	arena->tool_hidden = TOOL_MOVE;
	arena->state = STATE_NORMAL;
	arena->hover_joint = NULL;
	arena->hover_block = NULL;

	arena->root_joints_moving = NULL;
	arena->root_blocks_moving = NULL;
	arena->blocks_moving = NULL;

	xml_parse(xml, len, &level);
	convert_xml(&level, &arena->design);

	arena->world = gen_world(&arena->design);

	block_graphics_init(arena);

	/*
	glGenBuffers(1, &arena->joint_coord_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, arena->joint_coord_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arena->joint_coords),
		     arena->joint_coords, GL_STREAM_DRAW);
	*/

	arena->tick = 0;
	text_stream_create(&arena->tick_counter, MAX_RENDER_TEXT_LENGTH);
	arena->has_won = false;

	arena->preview_gp_trajectory = false;
	arena->preview_design = NULL;
	arena->preview_world = NULL;

	change_speed_preset(arena, 2);
}

/* TODO: dedupe */
static void u64tostr(char *buf, uint64_t val)
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

void update_tool(struct arena *arena)
{
	if (arena->shift)
		arena->tool = TOOL_MOVE;
	else if (arena->ctrl)
		arena->tool = TOOL_DELETE;
	else
		arena->tool = arena->tool_hidden;
}

void arena_key_up_event(struct arena *arena, int key)
{
	switch (key) {
	case 50: /* shift */
		arena->shift = false;
		break;
	case 37: /* ctrl */
		arena->ctrl = false;
		break;
	}

	update_tool(arena);
}

static int block_inside_area(struct block *block, struct area *area);

bool goal_blocks_inside_goal_area(struct design *design)
{
	struct block *block;
	bool any = false;

	for (block = design->player_blocks.head; block; block = block->next) {
		if (block->goal) {
			any = true;
			if (!block_inside_area(block, &design->goal_area))
				return false;
		}
	}

	return any;
}

void start(struct arena *arena)
{
	free_world(arena->world, &arena->design);
	arena->world = gen_world(&arena->design);
	//arena->ival = set_interval(tick_func, arena->tick_ms, arena);
	arena->hover_joint = NULL;
	arena->tick = 0;
	arena->has_won = false;
}

void stop(struct arena *arena)
{
	free_world(arena->world, &arena->design);
	arena->world = gen_world(&arena->design);
	//clear_interval(arena->ival);
}

void change_speed(struct arena *arena, int ms, int multiply)
{
	arena->tick_ms = ms;
	arena->tick_multiply = multiply;
	clear_interval(arena->ival);
	arena->ival = set_interval(tick_func, ms, arena);
}

double _fcsim_speed_factor = 2;
int _fcsim_base_fps_mod = 0;
int _fcsim_base_fps_table[] = BASE_FPS_TABLE;
double _fcsim_target_tps = 60;
int _fcsim_speed_preset = 2;
void change_speed_factor(struct arena *arena, double new_factor, int new_base_fps_mod) {
	// only change values if not given NO_CHANGE
	if(new_factor != NO_CHANGE) {
		_fcsim_speed_factor = new_factor;
	}
	if(new_base_fps_mod != NO_CHANGE) {
		_fcsim_base_fps_mod = new_base_fps_mod;
	}
	// calculate new speed factor
	double factor = _fcsim_speed_factor;
	int base_fps = _fcsim_base_fps_table[_fcsim_base_fps_mod];
	if(factor < 1)factor = 1;
	_fcsim_target_tps = factor * base_fps;
	double mspt = 1000 / (base_fps * factor);
	long long int multiples = 1 + (long long int)(MIN_MSPT / mspt);
	long long int mspt_int = (long long int)(1000 * multiples / (base_fps * factor));
	change_speed(arena, (int)mspt_int, (int)multiples);
}

void change_speed_preset(struct arena *arena, int preset_index) {
	_fcsim_speed_preset = preset_index;
	double new_factor = 1;
	switch(preset_index) {
		case 1: {
			new_factor = 1;
			break;
		}
		case 2: {
			new_factor = 2;
			break;
		}
		case 3: {
			new_factor = 4;
			break;
		}
		case 4: {
			new_factor = 10;
			break;
		}
		case 5: {
			new_factor = 100;
			break;
		}
		case 6: {
			new_factor = 1000;
			break;
		}
		case 7: {
			new_factor = 1e4;
			break;
		}
		case 8: {
			new_factor = 1e5;
			break;
		}
		case 9: {
			new_factor = 2e9;
			break;
		}
	}
	change_speed_factor(arena, new_factor, NO_CHANGE);
}

void mouse_up_new_block(struct arena *arena);
void mouse_up_move(struct arena *arena);

bool is_running(struct arena *arena) {
	switch (arena->state) {
	case STATE_RUNNING:
	case STATE_RUNNING_PAN:
	return true;
	}
	return false;
}

void start_stop(struct arena *arena)
{
	switch (arena->state) {
	case STATE_NORMAL:
		start(arena);
		arena->state = STATE_RUNNING;
		break;
	case STATE_NORMAL_PAN:
		start(arena);
		arena->state = STATE_RUNNING_PAN;
		break;
	case STATE_NEW_ROD:
	case STATE_NEW_WHEEL:
		mouse_up_new_block(arena);
		start(arena);
		arena->state = STATE_RUNNING_PAN;
		break;
	case STATE_MOVE:
		mouse_up_move(arena);
		start(arena);
		arena->state = STATE_RUNNING_PAN;
		break;
	case STATE_RUNNING:
		stop(arena);
		arena->state = STATE_NORMAL;
		break;
	case STATE_RUNNING_PAN:
		stop(arena);
		arena->state = STATE_NORMAL_PAN;
		break;
	}
	// also unpause
	arena->single_ticks_remaining = -1;
}

void arena_key_down_event(struct arena *arena, int key)
{
	switch (key) {
	case 65: /* space */
		start_stop(arena);
		break;
	case 27: /* R */
		arena->tool_hidden = TOOL_ROD;
		break;
	case 58: /* M */
		arena->tool_hidden = TOOL_MOVE;
		break;
	case 39: /* S */
		arena->tool_hidden = TOOL_SOLID_ROD;
		break;
	case 40: /* D */
		arena->tool_hidden = TOOL_DELETE;
		break;
	case 30: /* U */
		arena->tool_hidden = TOOL_WHEEL;
		break;
	case 25: /* W */
		arena->tool_hidden = TOOL_CW_WHEEL;
		break;
	case 54: /* C */
		arena->tool_hidden = TOOL_CCW_WHEEL;
		break;
	case 10: /* 1 */
		change_speed_preset(arena, 1);
		break;
	case 11: /* 2 */
		change_speed_preset(arena, 2);
		break;
	case 12: /* 3 */
		change_speed_preset(arena, 3);
		break;
	case 13: /* 4 */
		change_speed_preset(arena, 4);
		break;
	case 14: /* 5 */
		change_speed_preset(arena, 5);
		break;
	case 15: /* 6 */
		change_speed_preset(arena, 6);
		break;
	case 16: /* 7 */
		change_speed_preset(arena, 7);
		break;
	case 17: /* 8 */
		change_speed_preset(arena, 8);
		break;
	case 18: /* 9 */
		// maximum hyperspeed
		change_speed_preset(arena, 9);
		break;
	case 19: /* 0 */
		// cycle base speeds
		change_speed_factor(arena, NO_CHANGE, (_fcsim_base_fps_mod + 1) % BASE_FPS_TABLE_SIZE);
		break;
	case 50: /* shift */
		arena->shift = true;
		break;
	case 37: /* ctrl */
		arena->ctrl = true;
		break;
	}

	update_tool(arena);
}

static float distance(float x0, float y0, float x1, float y1)
{
	float dx = x1 - x0;
	float dy = y1 - y0;

	return sqrtf(dx * dx + dy * dy);
}

struct joint *joint_hit_test(struct arena *arena, float x, float y)
{
	struct design *design = &arena->design;
	struct joint *best_joint = NULL;
	struct joint *joint;
	double best_dist = 8.0f;
	double dist;

	for (joint = design->joints.head; joint; joint = joint->next) {
		dist = distance(x, y, joint->x, joint->y);
		if (dist < best_dist) {
			best_dist = dist;
			best_joint = joint;
		}
	}

	return best_joint;
}

struct joint *joint_hit_test_exclude_rod(struct arena *arena, float x, float y, struct rod *rod, bool attached)
{
	struct design *design = &arena->design;
	struct joint *best_joint = NULL;
	struct joint *joint;
	double best_dist = 8.0f;
	double dist;

	for (joint = design->joints.head; joint; joint = joint->next) {
		if (joint == rod->from)
			continue;
		if (!attached && joint == rod->to)
			continue;
		if (!attached && share_block(design, rod->from, joint))
			continue;
		dist = distance(x, y, joint->x, joint->y);
		if (dist < best_dist) {
			best_dist = dist;
			best_joint = joint;
		}
	}

	return best_joint;
}

bool has_wheel(struct joint *joint, struct wheel *not_this_one)
{
	struct attach_node *node;

	if (joint->gen && joint->gen->shape.type == SHAPE_WHEEL)
		return true;

	for (node = joint->att.head; node; node = node->next) {
		if (node->block->shape.type == SHAPE_WHEEL &&
		    &node->block->shape.wheel != not_this_one)
			return true;
	}

	return false;
}

struct joint *joint_hit_test_exclude_wheel(struct arena *arena, float x, float y, struct wheel *wheel, bool attached)
{
	struct design *design = &arena->design;
	struct joint *best_joint = NULL;
	struct joint *joint;
	double best_dist = 8.0f;
	double dist;

	for (joint = design->joints.head; joint; joint = joint->next) {
		if (!attached && joint == wheel->center)
			continue;
		if (joint == wheel->spokes[0])
			continue;
		if (joint == wheel->spokes[1])
			continue;
		if (joint == wheel->spokes[2])
			continue;
		if (joint == wheel->spokes[3])
			continue;
		if (has_wheel(joint, wheel))
			continue;
		dist = distance(x, y, joint->x, joint->y);
		if (dist < best_dist) {
			best_dist = dist;
			best_joint = joint;
		}
	}

	return best_joint;
}

bool rect_is_hit(struct shell *shell, float x, float y)
{
	float dx = shell->x - x;
	float dy = shell->y - y;
	float s = sinf(shell->angle);
	float c = cosf(shell->angle);
	float dx_t =  dx * c + dy * s;
	float dy_t = -dx * s + dy * c;

	shell->rect.w += 8.0 * 2;
	shell->rect.h += 8.0 * 2;

	return fabsf(dx_t) < shell->rect.w / 2 && fabsf(dy_t) < shell->rect.h / 2;
}

bool circ_is_hit(struct shell *shell, float x, float y)
{
	float dx = shell->x - x;
	float dy = shell->y - y;
	float r = shell->circ.radius + 8.0;

	return dx * dx + dy * dy < r * r;
}

bool block_is_hit(struct block *block, float x, float y)
{
	struct shell shell;

	get_shell(&shell, &block->shape);
	if (shell.type == SHELL_CIRC)
		return circ_is_hit(&shell, x, y);
	else
		return rect_is_hit(&shell, x, y);
}

struct block *block_hit_test(struct arena *arena, float x, float y)
{
	struct design *design = &arena->design;
	struct block *block;

	for (block = design->player_blocks.tail; block; block = block->prev) {
		if (block_is_hit(block, x, y))
			return block;
	}

	return NULL;
}

static void get_rect_bb(struct shell *shell, struct area *area)
{
	float sina = fp_sin(shell->angle);
	float cosa = fp_cos(shell->angle);
	float wc = shell->rect.w * cosa;
	float ws = shell->rect.w * sina;
	float hc = shell->rect.h * cosa;
	float hs = shell->rect.h * sina;

	area->x = shell->x;
	area->y = shell->y;
	area->w = fabs(wc) + fabs(hs);
	area->h = fabs(ws) + fabs(hc);
}

static void get_circ_bb(struct shell *shell, struct area *area)
{
	area->x = shell->x;
	area->y = shell->y;
	area->w = shell->circ.radius * 2;
	area->h = shell->circ.radius * 2;
}

static void get_block_bb(struct block *block, struct area *area)
{
	struct shell shell;

	get_shell(&shell, &block->shape);
	if (block->body) {
		shell.x = block->body->m_position.x;
		shell.y = block->body->m_position.y;
		shell.angle = block->body->m_rotation;
	}

	if (shell.type == SHELL_CIRC)
		get_circ_bb(&shell, area);
	else
		get_rect_bb(&shell, area);
}

static int block_inside_area(struct block *block, struct area *area)
{
	struct area bb;

	get_block_bb(block, &bb);

	return bb.x - bb.w / 2 >= area->x - area->w / 2
	    && bb.x + bb.w / 2 <= area->x + area->w / 2
	    && bb.y - bb.h / 2 >= area->y - area->h / 2
	    && bb.y + bb.h / 2 <= area->y + area->h / 2;
}

void gen_block(b2World *world, struct block *block);
void b2World_CleanBodyList(b2World *world);

void mark_overlaps(struct arena *arena)
{
	b2Contact *contact;
	struct block *block;

	b2ContactManager_CleanContactList(&arena->world->m_contactManager);
	b2World_CleanBodyList(arena->world);
	b2ContactManager_Collide(&arena->world->m_contactManager);

	for (block = arena->design.player_blocks.head; block; block = block->next) {
		block->overlap = false;
		if (!block_inside_area(block, &arena->design.build_area))
			block->overlap = true;
	}

	for (contact = arena->world->m_contactList; contact; contact = contact->m_next) {
		if (contact->m_manifoldCount > 0) {
			block = (struct block *)contact->m_shape1->m_userData;
			block->overlap = true;
			block = (struct block *)contact->m_shape2->m_userData;
			block->overlap = true;
		}
	}

	for (block = arena->design.player_blocks.head; block; block = block->next) {
		if (!block->visited && block != arena->new_block)
			block->overlap = false;
	}

	for (block = arena->design.level_blocks.head; block; block = block->next)
		block->overlap = false;
}

void delete_rod_joints(struct design *design, struct rod *rod)
{
	remove_attach_node(&rod->from->att, rod->from_att);
	free(rod->from_att);
	if (!rod->from->att.head && !rod->from->gen) {
		remove_joint(&design->joints, rod->from);
		free(rod->from);
	}

	remove_attach_node(&rod->to->att, rod->to_att);
	free(rod->to_att);
	if (!rod->to->att.head && !rod->to->gen) {
		remove_joint(&design->joints, rod->to);
		free(rod->to);
	}
}

void delete_wheel_joints(struct design *design, struct wheel *wheel)
{
	int i;

	remove_attach_node(&wheel->center->att, wheel->center_att);
	free(wheel->center_att);
	if (!wheel->center->att.head && !wheel->center->gen) {
		remove_joint(&design->joints, wheel->center);
		free(wheel->center);
	}

	for (i = 0; i < 4; i++) {
		if (wheel->spokes[i]->att.head) {
			wheel->spokes[i]->gen = NULL;
		} else {
			remove_joint(&design->joints, wheel->spokes[i]);
			free(wheel->spokes[i]);
		}
	}
}

void delete_block(struct arena *arena, struct block *block)
{
	struct design *design = &arena->design;
	struct shape *shape = &block->shape;

	switch (shape->type) {
	case SHAPE_ROD:
		delete_rod_joints(design, &shape->rod);
		break;
	case SHAPE_WHEEL:
		delete_wheel_joints(design, &shape->wheel);
	}

	b2World_DestroyBody(arena->world, block->body);
	remove_block(&design->player_blocks, block);
	free(block);

	mark_overlaps(arena);

	design->modcount++;
}

void action_pan(struct arena *arena, int x, int y)
{
	float dx_world;
	float dy_world;

	dx_world = (x - arena->cursor_x) * arena->view.scale * 2;
	dy_world = (y - arena->cursor_y) * arena->view.scale * 2;

	arena->view.x -= dx_world;
	arena->view.y -= dy_world;
}

void action_none(struct arena *arena, int x, int y)
{
	float x_world;
	float y_world;
	struct joint *joint;
	struct block *block;

	if (arena->state == STATE_RUNNING || arena->state == STATE_RUNNING_PAN)
		return;

	pixel_to_world(&arena->view, x, y, &x_world, &y_world);

	joint = joint_hit_test(arena, x_world, y_world);
	if (joint) {
		arena->hover_joint = joint;
		arena->hover_block = NULL;
		return;
	}

	block = block_hit_test(arena, x_world, y_world);
	if (block) {
		arena->hover_joint = NULL;
		arena->hover_block = block;
		return;
	}

	arena->hover_joint = NULL;
	arena->hover_block = NULL;
}

void move_joint(struct arena *arena, struct joint *joint, double x, double y);

void update_wheel_joints2(struct wheel *wheel)
{
	double a[4] = {
		0.0,
		3.141592653589793 / 2,
		3.141592653589793,
		4.71238898038469,
	};
	double x, y;
	int i;

	x = wheel->center->x;
	y = wheel->center->y;

	for (i = 0; i < 4; i++) {
		wheel->spokes[i]->x = x + fp_cos(wheel->angle + a[i]) * wheel->radius;
		wheel->spokes[i]->y = y + fp_sin(wheel->angle + a[i]) * wheel->radius;
	}
}

void update_box_joints2(struct box *box)
{
	double x = box->x;
	double y = box->y;
	double w_half = box->w / 2;
	double h_half = box->h / 2;

	double x0 =  fp_cos(box->angle) * w_half;
	double y0 =  fp_sin(box->angle) * w_half;
	double x1 =  fp_sin(box->angle) * h_half;
	double y1 = -fp_cos(box->angle) * h_half;

	box->center->x = x;
	box->center->y = y;

	box->corners[0]->x = x + x0 + x1;
	box->corners[0]->y = y + y0 + y1;

	box->corners[1]->x = x - x0 + x1;
	box->corners[1]->y = y - y0 + y1;

	box->corners[2]->x = x + x0 - x1;
	box->corners[2]->y = y + y0 - y1;

	box->corners[3]->x = x - x0 - x1;
	box->corners[3]->y = y - y0 - y1;
}

void update_joints2(struct block *block)
{
	struct shape *shape = &block->shape;

	switch (shape->type) {
	case SHAPE_BOX:
		update_box_joints2(&shape->box);
		break;
	case SHAPE_WHEEL:
		update_wheel_joints2(&shape->wheel);
		break;
	}
}

void update_wheel_joints(struct arena *arena, struct wheel *wheel)
{
	double a[4] = {
		0.0,
		3.141592653589793 / 2,
		3.141592653589793,
		4.71238898038469,
	};
	double spoke_x, spoke_y;
	double x, y;
	int i;

	x = wheel->center->x;
	y = wheel->center->y;

	for (i = 0; i < 4; i++) {
		spoke_x = x + fp_cos(wheel->angle + a[i]) * wheel->radius;
		spoke_y = y + fp_sin(wheel->angle + a[i]) * wheel->radius;
		move_joint(arena, wheel->spokes[i], spoke_x, spoke_y);
	}
}

void update_joints(struct arena *arena, struct block *block)
{
	struct shape *shape = &block->shape;

	switch (shape->type) {
	case SHAPE_WHEEL:
		update_wheel_joints(arena, &shape->wheel);
		break;
	}
}

void update_body(struct arena *arena, struct block *block)
{
	b2World_DestroyBody(arena->world, block->body);
	gen_block(arena->world, block);

	arena->design.modcount++;
}

void move_joint(struct arena *arena, struct joint *joint, double x, double y)
{
	struct attach_node *node;

	joint->x = x;
	joint->y = y;

	for (node = joint->att.head; node; node = node->next) {
		update_joints(arena, node->block);
		update_body(arena, node->block);
	}
}

void move_root_block(struct block *block, double x, double y)
{
	if (block->shape.type == SHAPE_BOX) {
		block->shape.box.x = x;
		block->shape.box.y = y;
	}
}

void update_move(struct arena *arena, double dx, double dy)
{
	struct joint_head *joint_head;
	struct block_head *block_head;

	for (joint_head = arena->root_joints_moving; joint_head;
	     joint_head = joint_head->next) {
		joint_head->joint->x = joint_head->orig_x + dx;
		joint_head->joint->y = joint_head->orig_y + dy;
	}

	for (block_head = arena->root_blocks_moving; block_head;
	     block_head = block_head->next) {
		move_root_block(block_head->block,
				block_head->orig_x + dx,
				block_head->orig_y + dy);
	}

	for (block_head = arena->blocks_moving; block_head;
	     block_head = block_head->next) {
		update_joints2(block_head->block);
	}

	for (block_head = arena->blocks_moving; block_head;
	     block_head = block_head->next) {
		update_body(arena, block_head->block);
	}

	mark_overlaps(arena);

	arena->design.modcount++;
}

void action_move(struct arena *arena, int x, int y)
{
	float x_world;
	float y_world;
	float dx;
	float dy;

	pixel_to_world(&arena->view, x, y, &x_world, &y_world);

	dx = x_world - arena->move_orig_x;
	dy = y_world - arena->move_orig_y;

	update_move(arena, dx, dy);
}

bool new_rod_attached(struct rod *rod)
{
	return rod->to->gen || rod->to_att->prev;
}

void attach_new_rod(struct design *design, struct block *block, struct joint *joint)
{
	struct rod *rod = &block->shape.rod;

	remove_joint(&design->joints, rod->to);
	free(rod->to);
	free(rod->to_att);

	rod->to = joint;
	rod->to_att = new_attach_node(block);
	append_attach_node(&joint->att, rod->to_att);
}

void attach_new_wheel(struct arena *arena, struct block *block, struct joint *joint)
{
	struct design *design = &arena->design;
	struct wheel *wheel = &block->shape.wheel;

	remove_joint(&design->joints, wheel->center);
	free(wheel->center);
	free(wheel->center_att);

	wheel->center = joint;
	wheel->center_att = new_attach_node(block);
	append_attach_node(&joint->att, wheel->center_att);

	update_joints(arena, block);
}

void detach_new_rod(struct design *design, struct block *block, double x, double y)
{
	struct rod *rod = &block->shape.rod;

	remove_attach_node(&rod->to->att, rod->to_att);
	free(rod->to_att);

	rod->to = new_joint(NULL, x, y);
	append_joint(&design->joints, rod->to);
	rod->to_att = new_attach_node(block);
	append_attach_node(&rod->to->att, rod->to_att);
}

void detach_new_wheel(struct arena *arena, struct block *block, double x, double y)
{
	struct design *design = &arena->design;
	struct wheel *wheel = &block->shape.wheel;

	remove_attach_node(&wheel->center->att, wheel->center_att);
	free(wheel->center_att);

	wheel->center = new_joint(NULL, x, y);
	append_joint(&design->joints, wheel->center);
	wheel->center_att = new_attach_node(block);
	append_attach_node(&wheel->center->att, wheel->center_att);

	update_joints(arena, block);
}

void adjust_new_rod(struct rod *rod)
{
	double dx = rod->to->x - rod->from->x;
	double dy = rod->to->y - rod->from->y;
	double len = sqrt(dx * dx + dy * dy);

	if (len >= 10.0)
		return;

	if (len == 0.0) {
		dx = 10.0;
		dy = 0.0;
	} else {
		dx *= 10.0 / len;
		dy *= 10.0 / len;
	}
	rod->to->x = rod->from->x + dx;
	rod->to->y = rod->from->y + dy;
}

void action_new_rod(struct arena *arena, int x, int y)
{
	struct rod *rod = &arena->new_block->shape.rod;
	float x_world;
	float y_world;
	struct joint *joint;
	bool attached;

	pixel_to_world(&arena->view, x, y, &x_world, &y_world);

	attached = new_rod_attached(rod);

	joint = joint_hit_test_exclude_rod(arena, x_world, y_world, rod, attached);

	if (!attached) {
		if (joint) {
			attach_new_rod(&arena->design, arena->new_block, joint);
		} else {
			rod->to->x = x_world;
			rod->to->y = y_world;
			adjust_new_rod(rod);
		}
	} else {
		if (!joint) {
			detach_new_rod(&arena->design, arena->new_block, x_world, y_world);
		} else if (joint != rod->to) {
			detach_new_rod(&arena->design, arena->new_block, x_world, y_world);
			attach_new_rod(&arena->design, arena->new_block, joint);
		}
	}

	update_body(arena, arena->new_block);
	mark_overlaps(arena);

	arena->hover_joint = joint_hit_test(arena, x_world, y_world);
}

void action_new_wheel(struct arena *arena, int x, int y)
{
	struct wheel *wheel = &arena->new_block->shape.wheel;
	float x_world;
	float y_world;
	struct joint *joint;
	bool attached;

	pixel_to_world(&arena->view, x, y, &x_world, &y_world);

	attached = wheel->center->gen || wheel->center_att->prev;

	joint = joint_hit_test_exclude_wheel(arena, x_world, y_world, wheel, attached);

	if (!attached) {
		if (joint) {
			attach_new_wheel(arena, arena->new_block, joint);
		} else {
			move_joint(arena, wheel->center, x_world, y_world);
		}
	} else {
		if (!joint) {
			detach_new_wheel(arena, arena->new_block, x_world, y_world);
		} else if (joint != wheel->center) {
			detach_new_wheel(arena, arena->new_block, x_world, y_world);
			attach_new_wheel(arena, arena->new_block, joint);
		}
	}

	update_body(arena, arena->new_block);
	mark_overlaps(arena);

	arena->hover_joint = joint_hit_test(arena, x_world, y_world);
}

void mouse_down_delete(struct arena *arena, float x, float y)
{
	struct block *block;

	block = block_hit_test(arena, x, y);
	if (block && !block->goal) {
		delete_block(arena, block);
		arena->hover_joint = joint_hit_test(arena, x, y);
	}
}

void arena_mouse_move_event(struct arena *arena, int x, int y)
{
	switch (arena->state) {
	case STATE_NORMAL_PAN:
	case STATE_RUNNING_PAN:
		action_pan(arena, x, y);
		break;
	case STATE_NORMAL:
	case STATE_RUNNING:
		action_none(arena, x, y);
		break;
	case STATE_MOVE:
		action_move(arena, x, y);
		break;
	case STATE_NEW_ROD:
		action_new_rod(arena, x, y);
		break;
	case STATE_NEW_WHEEL:
		action_new_wheel(arena, x, y);
		break;
	}

	arena->cursor_x = x;
	arena->cursor_y = y;
}

void joint_dfs(struct arena *arena, struct joint *joint, bool value, bool all);
void block_dfs(struct arena *arena, struct block *block, bool value, bool all);

void mouse_up_move(struct arena *arena)
{
	struct block_head *block_head;
	bool overlap = false;

	if (arena->move_orig_joint)
		joint_dfs(arena, arena->move_orig_joint, false, true);
	else
		block_dfs(arena, arena->move_orig_block, false, true);

	arena->move_orig_joint = NULL;
	arena->move_orig_block = NULL;

	for (block_head = arena->blocks_moving; block_head;
	     block_head = block_head->next) {
		if (block_head->block->overlap) {
			overlap = true;
			break;
		}
	}

	if (overlap)
		update_move(arena, 0.0, 0.0);

	arena->root_joints_moving = NULL;
	arena->root_blocks_moving = NULL;
	arena->blocks_moving = NULL;
}

void mouse_up_new_block(struct arena *arena)
{
	if (arena->new_block->overlap) {
		delete_block(arena, arena->new_block);
		arena->hover_joint = NULL;
	}
}

void arena_mouse_button_up_event(struct arena *arena, int button)
{
	if (button != 1) /* left */
		return;

	switch (arena->state) {
	case STATE_NORMAL_PAN:
		arena->state = STATE_NORMAL;
		break;
	case STATE_MOVE:
		mouse_up_move(arena);
		arena->state = STATE_NORMAL;
		break;
	case STATE_NEW_ROD:
	case STATE_NEW_WHEEL:
		mouse_up_new_block(arena);
		arena->state = STATE_NORMAL;
		break;
	case STATE_RUNNING_PAN:
		arena->state = STATE_RUNNING;
		break;
	}

}

struct joint_head *append_joint_head(struct joint_head *head, struct joint *joint)
{
	struct joint_head *new_head;

	new_head = malloc(sizeof(*new_head));
	new_head->next = head;
	new_head->joint = joint;
	new_head->orig_x = joint->x;
	new_head->orig_y = joint->y;

	return new_head;
}

struct block_head *append_block_head(struct block_head *head, struct block *block)
{
	struct block_head *new_head;

	new_head = malloc(sizeof(*new_head));
	new_head->next = head;
	new_head->block = block;
	if (block->shape.type == SHAPE_BOX) {
		new_head->orig_x = block->shape.box.x;
		new_head->orig_y = block->shape.box.y;
	}

	return new_head;
}

void block_dfs(struct arena *arena, struct block *block, bool value, bool all)
{
	struct shape *shape = &block->shape;
	int i;

	if (block->visited == value)
		return;
	block->visited = value;

	arena->blocks_moving = append_block_head(arena->blocks_moving, block);

	switch (shape->type) {
	case SHAPE_BOX:
		arena->root_blocks_moving =
			append_block_head(arena->root_blocks_moving, block);
		joint_dfs(arena, shape->box.center, value, all);
		for (i = 0; i < 4; i++)
			joint_dfs(arena, shape->box.corners[i], value, all);
		break;
	case SHAPE_ROD:
		if (all) {
			joint_dfs(arena, shape->rod.from, value, all);
			joint_dfs(arena, shape->rod.to, value, all);
		}
		break;
	case SHAPE_WHEEL:
		if (all)
			joint_dfs(arena, shape->wheel.center, value, all);
		for (i = 0; i < 4; i++)
			joint_dfs(arena, shape->wheel.spokes[i], value, all);
		break;
	}

	arena->design.modcount++;
}

void joint_dfs(struct arena *arena, struct joint *joint, bool value, bool all)
{
	struct attach_node *node;

	if (joint->visited == value)
		return;
	joint->visited = value;

	if (!joint->gen) {
		arena->root_joints_moving =
			append_joint_head(arena->root_joints_moving, joint);
	}

	if (all && joint->gen)
		block_dfs(arena, joint->gen, value, all);

	for (node = joint->att.head; node; node = node->next)
		block_dfs(arena, node->block, value, all);

	arena->design.modcount++;
}

void resolve_joint(struct arena *arena, struct joint *joint)
{
	struct block *block;

	while (joint->gen) {
		block = joint->gen;
		if (block->shape.type == SHAPE_BOX) {
			arena->move_orig_block = block;
			return;
		} else if (block->shape.type == SHAPE_WHEEL) {
			joint = block->shape.wheel.center;
		}
	}

	arena->move_orig_joint = joint;
}

void mouse_down_move(struct arena *arena, float x, float y)
{
	arena->move_orig_joint = NULL;
	arena->move_orig_block = NULL;

	if (arena->hover_joint) {
		resolve_joint(arena, arena->hover_joint);
		arena->state = STATE_MOVE;
		arena->move_orig_x = x;
		arena->move_orig_y = y;
		if (arena->move_orig_joint)
			joint_dfs(arena, arena->move_orig_joint, true, false);
		else
			block_dfs(arena, arena->move_orig_block, true, false);
	} else if (arena->hover_block) {
		arena->state = STATE_MOVE;
		arena->move_orig_x = x;
		arena->move_orig_y = y;
		arena->move_orig_block = arena->hover_block;
		block_dfs(arena, arena->hover_block, true, true);
	} else {
		arena->state = STATE_NORMAL_PAN;
	}
}

void mouse_down_rod(struct arena *arena, float x, float y)
{
	struct design *design = &arena->design;
	struct block *block;
	struct joint *j0, *j1;
	struct attach_node *att0, *att1;
	bool solid = arena->tool == TOOL_SOLID_ROD;

	block = malloc(sizeof(*block));
	block->prev = NULL;
	block->next = NULL;

	j0 = arena->hover_joint;
	if (!j0) {
		j0 = new_joint(NULL, x, y);
		append_joint(&design->joints, j0);
	}
	att0 = new_attach_node(block);
	append_attach_node(&j0->att, att0);

	j1 = new_joint(NULL, x, y);
	append_joint(&design->joints, j1);
	att1 = new_attach_node(block);
	append_attach_node(&j1->att, att1);

	block->shape.type = SHAPE_ROD;
	block->shape.rod.from = j0;
	block->shape.rod.from_att = att0;
	block->shape.rod.to = j1;
	block->shape.rod.to_att = att1;
	block->shape.rod.width = solid ? 8.0 : 4.0;
	adjust_new_rod(&block->shape.rod);

	block->material = solid ? &solid_rod_material : &water_rod_material;
	block->type_id = solid ? FCSIM_SOLID_ROD : FCSIM_ROD;
	block->goal = false;
	block->overlap = false;
	block->visited = false;

	arena->new_block = block;

	gen_block(arena->world, block);

	append_block(&design->player_blocks, block);

	arena->hover_joint = j0;

	arena->state = STATE_NEW_ROD;

	mark_overlaps(arena);

	design->modcount++;
}

void mouse_down_wheel(struct arena *arena, float x, float y)
{
	struct design *design = &arena->design;
	struct block *block;
	struct joint *j0;
	struct attach_node *att0;

	block = malloc(sizeof(*block));
	block->prev = NULL;
	block->next = NULL;

	j0 = arena->hover_joint;
	if (!j0 || has_wheel(j0, NULL)) {
		j0 = new_joint(NULL, x, y);
		append_joint(&design->joints, j0);
	}
	att0 = new_attach_node(block);
	append_attach_node(&j0->att, att0);

	block->shape.type = SHAPE_WHEEL;
	block->shape.wheel.center = j0;
	block->shape.wheel.center_att = att0;
	block->shape.wheel.radius = 20.0;
	block->shape.wheel.angle = 0.0;

	switch (arena->tool) {
	case TOOL_WHEEL:
		block->shape.wheel.spin = 0;
		break;
	case TOOL_CW_WHEEL:
		block->shape.wheel.spin = 5;
		break;
	case TOOL_CCW_WHEEL:
		block->shape.wheel.spin = -5;
		break;
	}

	double a[4] = {
		0.0,
		3.141592653589793 / 2,
		3.141592653589793,
		4.71238898038469,
	};
	double spoke_x, spoke_y;
	int i;

	for (i = 0; i < 4; i++) {
		spoke_x = j0->x + fp_cos(a[i]) * 20.0;
		spoke_y = j0->y + fp_sin(a[i]) * 20.0;
		block->shape.wheel.spokes[i] = new_joint(block, spoke_x, spoke_y);
		append_joint(&design->joints, block->shape.wheel.spokes[i]);
	}

	block->material = &solid_material;
	block->goal = false;
	block->overlap = false;
	block->visited = false;

	switch (arena->tool) {
	case TOOL_WHEEL:
		block->type_id = FCSIM_WHEEL;
		break;
	case TOOL_CW_WHEEL:
		block->type_id = FCSIM_CW_WHEEL;
		break;
	case TOOL_CCW_WHEEL:
		block->type_id = FCSIM_CCW_WHEEL;
		break;
	}

	arena->new_block = block;

	gen_block(arena->world, block);

	append_block(&design->player_blocks, block);

	arena->hover_joint = j0;

	arena->state = STATE_NEW_WHEEL;

	mark_overlaps(arena);

	design->modcount++;
}

bool inside_area(struct area *area, double x, double y)
{
	double w_half = area->w / 2;
	double h_half = area->h / 2;

	return x > area->x - w_half
	    && x < area->x + w_half
	    && y > area->y - h_half
	    && y < area->y + h_half;
}

int block_list_len(struct block_list *list)
{
	struct block *block;
	int res = 0;

	for (block = list->head; block; block = block->next)
		res++;

	return res;
}

void mouse_down_tool(struct arena *arena, float x, float y)
{
	if (arena->tool == TOOL_MOVE) {
		mouse_down_move(arena, x, y);
		return;
	}

	if (arena->tool == TOOL_DELETE) {
		mouse_down_delete(arena, x, y);
		return;
	}

	if (block_list_len(&arena->design.player_blocks) >= 120)
		return;

	if (arena->tool == TOOL_ROD || arena->tool == TOOL_SOLID_ROD) {
		mouse_down_rod(arena, x, y);
		return;
	}

	if (arena->tool == TOOL_WHEEL ||
	    arena->tool == TOOL_CW_WHEEL ||
	    arena->tool == TOOL_CCW_WHEEL) {
		mouse_down_wheel(arena, x, y);
		return;
	}
}

void arena_mouse_button_down_event(struct arena *arena, int button)
{
	int x = arena->cursor_x;
	int y = arena->cursor_y;
	float x_world;
	float y_world;

	if (button != 1) // left
		return;

	pixel_to_world(&arena->view, x, y, &x_world, &y_world);

	if(arena_mouse_click_button(arena))return;

	switch (arena->state) {
	case STATE_NORMAL:
		if (inside_area(&arena->design.build_area, x_world, y_world))
			mouse_down_tool(arena, x_world, y_world);
		else
			arena->state = STATE_NORMAL_PAN;
		break;
	case STATE_RUNNING:
		arena->state = STATE_RUNNING_PAN;
		break;
	}
}

void arena_scroll_event(struct arena *arena, int delta)
{
	arena->view.scale *= (1.0f - delta * 0.05f);
}

void arena_size_event(struct arena *arena, float width, float height)
{
	arena->view.width = width;
	arena->view.height = height;
}
