#ifdef __wasm__
#include "stdint.h"
#include "stl_mock.h"
#else
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#endif
#include "stl_compat.h"
#include "interval.h"
extern "C" {
#include "gl.h"
#include "graph.h"
#include <box2d/b2Body.h>
}
#include "arena.hpp"
#include "math.h"

template <typename T> void append_vector(std::vector<T>& a, std::vector<T>& b) {
    for(auto it = b.begin(); it != b.end(); ++it) {
        a.push_back(*it);
    }
}

struct block_graphics_layer {
    std::vector<uint16_t> indices;
    std::vector<float> coords;
    std::vector<float> colors;
    uint16_t push_vertex(float x, float y, float r, float g, float b);
    uint16_t push_vertex(float x, float y, color col);
    void push_triangle(uint16_t v1, uint16_t v2, uint16_t v3);
};

struct fps_tracker_t {
    // number of samples affects smoothing and latency
    // fps ranges from about 30 to 144, so this takes 3.5 ~ 17 seconds to update
    static const size_t buffer_size = 512;
    std::vector<double> records_ms;
    std::vector<int64_t> records_tick;
    size_t index = 0;
    size_t samples = 0;
    void push_record(int64_t);
    double get_tps(size_t interval = buffer_size - 1);
    void clear();
};

struct block_graphics {
    // flags
    bool simple_graphics = false;
    bool wireframe = false;
    // working values
    std::vector<block_graphics_layer> layers;
    // final values
    std::vector<uint16_t> _indices;
    std::vector<float> _coords;
    std::vector<float> _colors;
	GLuint _index_buffer;
	GLuint _coord_buffer;
	GLuint _color_buffer;
	int _triangle_cnt;
	int _vertex_cnt;
    // timing diagnostics
    fps_tracker_t fps_tracker;
    fps_tracker_t tps_tracker;
    // methods
    void clear();
    void ensure_layer(int z_offset);
    uint16_t push_vertex(float x, float y, color col, int z_offset);
    void push_triangle(uint16_t v1, uint16_t v2, uint16_t v3, int z_offset);
    void push_all_layers();
};

struct ui_button_id {
    int group, index;
    bool operator==(const ui_button_id& other) {
        return group == other.group && index == other.index;
    }
};

struct ui_button_text {
    std::string text;
    float scale = FONT_SCALE_DEFAULT;
    float relative_x = 0, relative_y = 0, align_x = 0.5f, align_y = 0.5f;
};

struct ui_button_single {
    ui_button_id id = {0, 0};
    float x = 0, y = 0, w = 0, h = 0;
    int z_offset = 0;
    bool enabled = true, clickable = true, highlighted = false;
    std::vector<ui_button_text> texts;
};

struct ui_button_collection {
    std::vector<ui_button_single> buttons;
    std::vector<area> buttons_buffer;
};

constexpr float lerp_exact(float a, float b, float t) {
  // Exact, monotonic, bounded, determinate, and (for a=b=0) consistent:
  if(a<=0 && b>=0 || a>=0 && b<=0) return t*b + (1-t)*a;

  if(t==1) return b;                        // exact
  // Exact at t=0, monotonic except near t=1,
  // bounded, determinate, and consistent:
  const float x = a + t*(b-a);
  return t>1 == b>a ? std::max(b,x) : std::min(b,x);  // monotonic near t=1
}

color get_color_by_type(int type_id, int slot) {
    color result;
    get_color_by_type(type_id, slot, &result);
    return result;
}

color alpha_over(const color& below, const color& above) {
    return color{
        lerp_exact(below.a, 1, above.a),
        lerp_exact(below.r, above.r, above.a),
        lerp_exact(below.g, above.g, above.a),
        lerp_exact(below.b, above.b, above.a),
    };
}

uint16_t block_graphics_layer::push_vertex(float x, float y, float r, float g, float b) {
    uint16_t offset = coords.size() / 2;
    coords.push_back(x);
    coords.push_back(y);
    colors.push_back(r);
    colors.push_back(g);
    colors.push_back(b);
    return offset;
}

uint16_t block_graphics_layer::push_vertex(float x, float y, color col) {
    return push_vertex(x, y, col.r, col.g, col.b);
}

void block_graphics::ensure_layer(int z_offset) {
    while((int)layers.size() <= z_offset) {
        layers.emplace_back();
    }
}

uint16_t block_graphics::push_vertex(float x, float y, color col, int z_offset) {
    ensure_layer(z_offset);
    return layers[z_offset].push_vertex(x, y, col);
}

void block_graphics_layer::push_triangle(uint16_t v1, uint16_t v2, uint16_t v3) {
    indices.push_back(v1);
    indices.push_back(v2);
    indices.push_back(v3);
}

void block_graphics::push_triangle(uint16_t v1, uint16_t v2, uint16_t v3, int z_offset) {
    ensure_layer(z_offset);
    return layers[z_offset].push_triangle(v1, v2, v3);
}

void block_graphics::push_all_layers() {
    _indices.clear();
    _coords.clear();
    _colors.clear();
    for(auto it = layers.begin(); it != layers.end(); ++it) {
        uint16_t offset = _coords.size() / 2;
        for(auto jt = it->indices.begin(); jt != it->indices.end(); ++jt) {
            _indices.push_back(offset + *jt);
        }
        append_vector(_coords, it->coords);
        append_vector(_colors, it->colors);
    }
    _triangle_cnt = _indices.size() / 3;
    _vertex_cnt = _coords.size() / 2;
}

void block_graphics::clear() {
    layers.clear();
}

void fps_tracker_t::push_record(int64_t tick_value) {
    // get new sample values
    double time_value = time_precise_ms();
    // fill arrays if not filled already
    while(records_ms.size() < buffer_size) {
        records_ms.push_back(0);
        records_tick.push_back(0);
    }
    // add new value
    index = (index + 1) % buffer_size;
    records_ms[index] = time_value;
    records_tick[index] = tick_value;
    samples++;
}

double fps_tracker_t::get_tps(size_t interval) {
    if(samples < interval + 1) {
        // not enough data
        return 0;
    }
    size_t prev_index = (index + buffer_size - interval) % buffer_size;
    return 1000 * (records_tick[index] - records_tick[prev_index]) / (records_ms[index] - records_ms[prev_index]);
}

void fps_tracker_t::clear() {
    records_ms.clear();
    records_tick.clear();
    samples = 0;
}

shell get_shell(block* block) {
    shell shell;
    get_shell(&shell, &block->shape);
	if (block->body) {
		shell.x = block->body->m_position.x;
		shell.y = block->body->m_position.y;
		shell.angle = block->body->m_rotation;
	}
    return shell;
}

joint joint_from_xy(float x, float y) {
    joint result;
    result.x = x;
    result.y = y;
    return result;
}

std::vector<joint> generate_joints(block* block) {
    // TODO: correct joint order
    const int type_id = block->type_id;
    shell shell = get_shell(block);
    std::vector<joint> result;
    float sina_half = sinf(shell.angle) / 2;
    float cosa_half = cosf(shell.angle) / 2;
    float w = shell.type == SHELL_CIRC ? shell.circ.radius * 2 : shell.rect.w;
    float h = shell.type == SHELL_CIRC ? shell.circ.radius * 2 : shell.rect.h;
    float wc = w * cosa_half;
    float ws = w * sina_half;
    float hc = h * cosa_half;
    float hs = h * sina_half;
    switch(type_id) {
        case FCSIM_ROD:
        case FCSIM_SOLID_ROD:
        {
            // pattern: rod (up to 2)
            result.push_back(joint_from_xy(shell.x - wc, shell.y - ws));
            result.push_back(joint_from_xy(shell.x + wc, shell.y + ws));
        }
        break;
        case FCSIM_GOAL_RECT:
        {
            // pattern: jointed rect (up to 5)
            result.push_back(joint_from_xy(shell.x, shell.y));
            result.push_back(joint_from_xy(shell.x + wc - hs, shell.y + ws + hc));
            result.push_back(joint_from_xy(shell.x - wc - hs, shell.y - ws + hc));
            result.push_back(joint_from_xy(shell.x - wc + hs, shell.y - ws - hc));
            result.push_back(joint_from_xy(shell.x + wc + hs, shell.y + ws - hc));
        }
        break;
        case FCSIM_GOAL_CIRCLE:
        case FCSIM_CW_GOAL_CIRCLE:
        case FCSIM_CCW_GOAL_CIRCLE:
        case FCSIM_WHEEL:
        case FCSIM_CW_WHEEL:
        case FCSIM_CCW_WHEEL:
        {
            // pattern: wheel (up to 9)
            result.push_back(joint_from_xy(shell.x, shell.y));
            result.push_back(joint_from_xy(shell.x + wc, shell.y + ws));
            result.push_back(joint_from_xy(shell.x - ws, shell.y + wc));
            result.push_back(joint_from_xy(shell.x - wc, shell.y - ws));
            result.push_back(joint_from_xy(shell.x + ws, shell.y - wc));
            if(w > 40) {
                // extra inner joints
                float wc2 = 40 * cosa_half;
                float ws2 = 40 * sina_half;
                result.push_back(joint_from_xy(shell.x + wc2, shell.y + ws2));
                result.push_back(joint_from_xy(shell.x - ws2, shell.y + wc2));
                result.push_back(joint_from_xy(shell.x - wc2, shell.y - ws2));
                result.push_back(joint_from_xy(shell.x + ws2, shell.y - wc2));
            }
        }
        break;
    }
    return result;
}

static void block_graphics_add_rect_single(struct block_graphics *graphics,
				    struct shell shell, color col, int z_offset)
{
	float sina_half = sinf(shell.angle) / 2;
	float cosa_half = cosf(shell.angle) / 2;
	float w = std::max(std::abs(shell.rect.w), 4.0);
	float h = std::max(std::abs(shell.rect.h), 4.0);
	float wc = w * cosa_half;
	float ws = w * sina_half;
	float hc = h * cosa_half;
	float hs = h * sina_half;

    uint16_t v1 = graphics->push_vertex(shell.x + wc - hs, shell.y + ws + hc, col, z_offset);
    uint16_t v2 = graphics->push_vertex(shell.x - wc - hs, shell.y - ws + hc, col, z_offset);
    uint16_t v3 = graphics->push_vertex(shell.x - wc + hs, shell.y - ws - hc, col, z_offset);
    uint16_t v4 = graphics->push_vertex(shell.x + wc + hs, shell.y + ws - hc, col, z_offset);

    graphics->push_triangle(v1, v2, v3, z_offset);
    graphics->push_triangle(v1, v4, v3, z_offset);
}

static void block_graphics_add_rect(struct block_graphics *graphics,
				    struct shell shell, int type_id, int z_offset, color overlay) {
    if(graphics->simple_graphics) {
        block_graphics_add_rect_single(graphics, shell, alpha_over(get_color_by_type(type_id, 1), overlay), z_offset + 1);
        return;
    }
    float w = shell.rect.w;
    float h = shell.rect.h;
    float iw = std::abs(w-8);
    float ih = std::abs(h-8);
    float ow,oh;
    if(type_id == FCSIM_ROD || type_id == FCSIM_SOLID_ROD) {
        ow = w;
        oh = h;
        if(type_id == FCSIM_ROD && !graphics->wireframe)oh*=2;
        iw = std::abs(ow-4);
        ih = std::abs(oh-4);
    }else{
        ow = std::max(w, iw+2);
		oh = std::max(h, ih+2);
    }
    shell.rect.w = ow;
    shell.rect.h = oh;
    block_graphics_add_rect_single(graphics, shell, alpha_over(get_color_by_type(type_id, 0), overlay), z_offset);
    shell.rect.w = iw;
    shell.rect.h = ih;
    block_graphics_add_rect_single(graphics, shell, alpha_over(get_color_by_type(type_id, 1), overlay), z_offset + 1);
}

static void block_graphics_add_area(struct block_graphics *graphics,
				    struct area area, int type_id)
{
    shell area_shell;
    area_shell.type = SHELL_RECT;
    area_shell.rect.w = area.w;
    area_shell.rect.h = area.h;
    area_shell.x = area.x;
    area_shell.y = area.y;
    area_shell.angle = 0;
    block_graphics_add_rect(graphics, area_shell, type_id, 0, color{0,0,0,0});
}

void block_graphics_add_line(block_graphics* graphics, b2Vec2 start, b2Vec2 end, double radius, color col, int z_offset) {
    const double dx1 = end.x - start.x;
    const double dy1 = end.y - start.y;

    const double dist = sqrt(dx1 * dx1 + dy1 * dy1); // hypot not available
    const double mul = radius / std::max(1e-10, dist);

    const double dx2 = dx1 * mul;
    const double dy2 = dy1 * mul;

    const double dx3 = dx2 - dy2;
    const double dy3 = dx2 + dy2;

    uint16_t v1 = graphics->push_vertex(end.x   + dx3, end.y   + dy3, col, z_offset);
    uint16_t v2 = graphics->push_vertex(end.x   + dy3, end.y   - dx3, col, z_offset);
    uint16_t v3 = graphics->push_vertex(start.x - dx3, start.y - dy3, col, z_offset);
    uint16_t v4 = graphics->push_vertex(start.x - dy3, start.y + dx3, col, z_offset);

    graphics->push_triangle(v1, v2, v3, z_offset);
    graphics->push_triangle(v1, v4, v3, z_offset);
}

static void block_graphics_add_circ_single(struct block_graphics *graphics,
				    struct shell shell, color col, int z_offset)
{
    const int circle_segments = 24;
    uint16_t v_last;

	float a;
	int i;

	for (int i = 0; i < circle_segments; i++) {
		a = shell.angle + TAU * i / circle_segments;
		float x = shell.x + cosf(a) * shell.circ.radius;
		float y = shell.y + sinf(a) * shell.circ.radius;
        v_last = graphics->push_vertex(x, y, col, z_offset);
	}

	for (i = 0; i < circle_segments - 2; i++) {
        graphics->push_triangle(v_last, v_last - i - 1, v_last - i - 2, z_offset);
	}
}


static void block_graphics_add_circ(struct block_graphics *graphics,
				    struct shell shell, int type_id, color overlay)
{
    const int z_offset = 2;
    if(graphics->simple_graphics) {
        block_graphics_add_circ_single(graphics, shell, alpha_over(get_color_by_type(type_id, 1), overlay), z_offset + 1);
        return;
    }
    float r = shell.circ.radius;
    float ir = r - 4;
    float _or = std::max(r, ir+1);
    shell.circ.radius = _or;
    block_graphics_add_circ_single(graphics, shell, alpha_over(get_color_by_type(type_id, 0), overlay), z_offset);
    shell.circ.radius = ir;
    block_graphics_add_circ_single(graphics, shell, alpha_over(get_color_by_type(type_id, 1), overlay), z_offset + 1);
}

#define JOINT_INNER_RADIUS 2
#define JOINT_OUTER_RADIUS 4

void block_graphics_add_joint(block_graphics* graphics, joint joint, color overlay) {
    const int z_offset = 4;
    color col = alpha_over(get_color_by_type(FCSIM_JOINT, 1), overlay);

    const int circle_segments = 24;
    uint16_t v_last;

	float a;
	int i;

	for (int i = 0; i < circle_segments; i++) {
		a = TAU * i / circle_segments;
		float x = joint.x + cosf(a) * JOINT_INNER_RADIUS;
		float y = joint.y + sinf(a) * JOINT_INNER_RADIUS;
        graphics->push_vertex(x, y, col, z_offset);
		x = joint.x + cosf(a) * JOINT_OUTER_RADIUS;
		y = joint.y + sinf(a) * JOINT_OUTER_RADIUS;
        v_last = graphics->push_vertex(x, y, col, z_offset);
	}

	for (i = 0; i < circle_segments - 1; i++) {
        graphics->push_triangle(v_last - i * 2, v_last - i * 2 - 1, v_last - i * 2 - 2, z_offset);
        graphics->push_triangle(v_last - i * 2 - 3, v_last - i * 2 - 1, v_last - i * 2 - 2, z_offset);
	}
    graphics->push_triangle(v_last - i * 2, v_last - i * 2 - 1, v_last, z_offset);
    graphics->push_triangle(v_last - 1, v_last - i * 2 - 1, v_last, z_offset);
}

static void block_graphics_add_block(struct block_graphics *graphics,
				     struct block *block)
{
	struct shell shell = get_shell(block);

    color overlay;

	if (block->overlap) {
		overlay = {1,1,0,0};
	} else if (block->visited) {
		overlay = {0.25,1,1,1};
	} else {
        overlay = {0,0,0,0};
	}

	if (shell.type == SHELL_CIRC)
		block_graphics_add_circ(graphics, shell, block->type_id, overlay);
	else
		block_graphics_add_rect(graphics, shell, block->type_id, 2, overlay);

    std::vector<joint> joints_generated = generate_joints(block);
    for(auto it = joints_generated.begin(); it != joints_generated.end(); ++it) {
        block_graphics_add_joint(graphics, *it, color{0,0,0,0});
    }
}

void block_graphics_push_and_bind(block_graphics* graphics) {
    // layers are ready
    graphics->push_all_layers();

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, graphics->_index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, graphics->_indices.size() * sizeof(uint16_t), &(graphics->_indices[0]),
		     GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, graphics->_coord_buffer);
	glBufferData(GL_ARRAY_BUFFER, graphics->_coords.size() * sizeof(float), &(graphics->_coords[0]), GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, graphics->_color_buffer);
	glBufferData(GL_ARRAY_BUFFER, graphics->_colors.size() * sizeof(float), &(graphics->_colors[0]), GL_STATIC_DRAW);
}

void block_graphics_reset(arena* arena, struct design *design)
{
    block_graphics* graphics = (block_graphics*)arena->block_graphics_v2;

    // clear old data
    graphics->clear();

    // fill all layers
	block_graphics_add_area(graphics, design->build_area, FCSIM_BUILD_AREA);
	block_graphics_add_area(graphics, design->goal_area, FCSIM_GOAL_AREA);

	struct block *block;
	for (block = design->level_blocks.head; block; block = block->next)
		block_graphics_add_block(graphics, block);

	for (block = design->player_blocks.head; block; block = block->next)
		block_graphics_add_block(graphics, block);
}

void block_graphics_draw(struct block_graphics *graphics, struct view *view, bool use_world_transform=true)
{
    block_graphics_push_and_bind(graphics);

	glUseProgram(block_program);

    if(use_world_transform) {
        glUniform2f(block_program_scale_uniform,
                1.0f / (view->width * view->scale),
                -1.0f / (view->height * view->scale));
        glUniform2f(block_program_shift_uniform,
                -view->x / (view->width * view->scale),
                view->y / (view->height * view->scale));
    } else {
        glUniform2f(block_program_scale_uniform,
                2.0f / view->width,
                2.0f / view->height);
        glUniform2f(block_program_shift_uniform, -1, -1);
    }

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, graphics->_index_buffer);

	glBindBuffer(GL_ARRAY_BUFFER, graphics->_coord_buffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, graphics->_color_buffer);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glDrawElements(GL_TRIANGLES, graphics->_triangle_cnt * 3, GL_UNSIGNED_SHORT, 0);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}

void on_button_clicked(arena* arena, ui_button_single& button) {
    if(button.id == ui_button_id{0, 0}) {
        arena->ui_toolbar_opened = true;
    }
    if(button.id == ui_button_id{1, 1}) {
        arena->ui_toolbar_opened = false;
    }
    if(button.id == ui_button_id{1, 2}) {
        arena->tool_hidden = TOOL_CW_WHEEL;
        update_tool(arena);
    }
    if(button.id == ui_button_id{1, 3}) {
        arena->tool_hidden = TOOL_CCW_WHEEL;
        update_tool(arena);
    }
    if(button.id == ui_button_id{1, 4}) {
        arena->tool_hidden = TOOL_WHEEL;
        update_tool(arena);
    }
    if(button.id == ui_button_id{1, 5}) {
        arena->tool_hidden = TOOL_ROD;
        update_tool(arena);
    }
    if(button.id == ui_button_id{1, 6}) {
        arena->tool_hidden = TOOL_SOLID_ROD;
        update_tool(arena);
    }
    if(button.id == ui_button_id{1, 7}) {
        arena->tool_hidden = TOOL_MOVE;
        update_tool(arena);
    }
    if(button.id == ui_button_id{1, 8}) {
        arena->tool_hidden = TOOL_DELETE;
        update_tool(arena);
    }
    if(button.id == ui_button_id{2, 0}) {
        start_stop(arena);
    }
    if(button.id == ui_button_id{2, 1}) {
        arena->single_ticks_remaining = arena->single_ticks_remaining==-1?0:-1;
    }
    if(button.id == ui_button_id{2, 2}) {
        arena->autostop_on_solve = !arena->autostop_on_solve;
    }
    if(button.id == ui_button_id{2, 3}) {
        arena->single_ticks_remaining = 1;
    }
    if(button.id == ui_button_id{3, 0}) {
        arena->ui_speedbar_opened = true;
    }
    if(button.id == ui_button_id{4, 1}) {
        arena->ui_speedbar_opened = false;
    }
    if(button.id == ui_button_id{4, 2}) {
        change_speed_preset(arena, 1);
    }
    if(button.id == ui_button_id{4, 3}) {
        change_speed_preset(arena, 2);
    }
    if(button.id == ui_button_id{4, 4}) {
        change_speed_preset(arena, 3);
    }
    if(button.id == ui_button_id{4, 5}) {
        change_speed_preset(arena, 4);
    }
    if(button.id == ui_button_id{4, 6}) {
        change_speed_preset(arena, 5);
    }
    if(button.id == ui_button_id{4, 7}) {
        change_speed_preset(arena, 6);
    }
    if(button.id == ui_button_id{4, 8}) {
        change_speed_preset(arena, 7);
    }
    if(button.id == ui_button_id{4, 9}) {
        change_speed_preset(arena, 8);
    }
    if(button.id == ui_button_id{4, 10}) {
        change_speed_preset(arena, 9);
    }
    if(button.id == ui_button_id{4, 11}) {
        change_speed_factor(arena, NO_CHANGE, (_fcsim_base_fps_mod + 1) % BASE_FPS_TABLE_SIZE);
    }
    if(button.id == ui_button_id{5, 0}) {
        arena->preview_gp_trajectory ^= 1; // toggle
    }
    if(button.id == ui_button_id{6, 0}) {
        arena->ui_garden_opened = true;
    }
    if(button.id == ui_button_id{7, 1}) {
        arena->ui_garden_opened = false;
    }
    if(button.id == ui_button_id{7, 2}) {
        arena->use_garden ^= 1; // toggle
    }
    if(button.id == ui_button_id{7, 3}) {
        arena->garden_queued_action = GARDEN_ACTION_RESET;
    }
    if(button.id == ui_button_id{7, 4}) {
        arena->garden_queued_action = GARDEN_ACTION_TAKE_BEST;
    }
}

void regenerate_ui_buttons(arena* arena) {
    ui_button_collection* all_buttons = (ui_button_collection*)arena->ui_buttons;
    all_buttons->buttons.clear();

    float vw = arena->view.width;
    float vh = arena->view.height;

    {
        ui_button_single button{{0, 0}, 75, vh, 30, 30};
        button.enabled = !arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"v", 2});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 0}, 120 + 55 * 3, vh, 60 + 55 * 6 + 8, 128};
        button.enabled = arena->ui_toolbar_opened;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 1}, 75, vh, 30, 128, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"^", 2, 0, -30});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 2}, 120, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"W", 2, 0, 5});
        button.texts.push_back(ui_button_text{"CW Wheel", 1, 0, -10});
        button.highlighted = arena->tool_hidden == TOOL_CW_WHEEL;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 3}, 120 + 55, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"C", 2, 0, 5});
        button.texts.push_back(ui_button_text{"CCW Wheel", 1, 0, -10});
        button.highlighted = arena->tool_hidden == TOOL_CCW_WHEEL;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 4}, 120 + 55 * 2, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"U", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Wheel", 1, 0, -10});
        button.highlighted = arena->tool_hidden == TOOL_WHEEL;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 5}, 120 + 55 * 3, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"R", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Water", 1, 0, -10});
        button.highlighted = arena->tool_hidden == TOOL_ROD;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 6}, 120 + 55 * 4, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"S", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Wood", 1, 0, -10});
        button.highlighted = arena->tool_hidden == TOOL_SOLID_ROD;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 7}, 120 + 55 * 5, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"M", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Move", 1, 0, -10});
        button.highlighted = arena->tool_hidden == TOOL_MOVE;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 8}, 120 + 55 * 6, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"D", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Delete", 1, 0, -10});
        button.highlighted = arena->tool_hidden == TOOL_DELETE;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{2, 0}, 30, vh - 30, 50, 50, 2};
        button.texts.push_back(ui_button_text{"Space", 1.5, 0, 8});
        button.texts.push_back(ui_button_text{is_running(arena)?"Stop":"Start", 1, 0, -8});
        button.highlighted = is_running(arena);
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{2, 1}, 30, vh - 55 - 20 * 0.5f + 4, 50, 20};
        button.texts.push_back(ui_button_text{arena->single_ticks_remaining==-1?"Pause":"Resume", 1});
        button.highlighted = arena->single_ticks_remaining!=-1;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{2, 2}, 30, vh - 55 - 20 * 1.5f + 4, 50, 20};
        button.texts.push_back(ui_button_text{arena->autostop_on_solve?"Cancel":"On solve", 1});
        button.highlighted = arena->autostop_on_solve;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{2, 3}, 30, vh - 55 - 20 * 2.5f + 4, 50, 20};
        button.texts.push_back(ui_button_text{"Step", 1});
        all_buttons->buttons.push_back(button);
    }

    float top_bar_x_offset = -50 + (!arena->ui_toolbar_opened?90:484);

    {
        ui_button_single button{{3, 0}, top_bar_x_offset + 75, vh, 30, 30};
        button.enabled = !arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"v", 2});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 0}, top_bar_x_offset + 120 + 55 * 4.5f, vh, 60 + 55 * 9 + 8, 128};
        button.enabled = arena->ui_speedbar_opened;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 1}, top_bar_x_offset + 75, vh, 30, 128, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"^", 2, 0, -30});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 2}, top_bar_x_offset + 120, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"1", 2, 0, 5});
        button.texts.push_back(ui_button_text{"1x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 1;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 3}, top_bar_x_offset + 120 + 55, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"2", 2, 0, 5});
        button.texts.push_back(ui_button_text{"2x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 2;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 4}, top_bar_x_offset + 120 + 55 * 2, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"3", 2, 0, 5});
        button.texts.push_back(ui_button_text{"4x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 3;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 5}, top_bar_x_offset + 120 + 55 * 3, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"4", 2, 0, 5});
        button.texts.push_back(ui_button_text{"8x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 4;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 6}, top_bar_x_offset + 120 + 55 * 4, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"5", 2, 0, 5});
        button.texts.push_back(ui_button_text{"100x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 5;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 7}, top_bar_x_offset + 120 + 55 * 5, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"6", 2, 0, 5});
        button.texts.push_back(ui_button_text{"1000x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 6;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 8}, top_bar_x_offset + 120 + 55 * 6, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"7", 2, 0, 5});
        button.texts.push_back(ui_button_text{"10000x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 7;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 9}, top_bar_x_offset + 120 + 55 * 7, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"8", 2, 0, 5});
        button.texts.push_back(ui_button_text{"100000x", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 8;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 10}, top_bar_x_offset + 120 + 55 * 8, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"9", 2, 0, 5});
        button.texts.push_back(ui_button_text{"MAX", 1, 0, -10});
        button.highlighted = _fcsim_speed_preset == 9;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{4, 11}, top_bar_x_offset + 120 + 55 * 9, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_speedbar_opened;
        button.texts.push_back(ui_button_text{"0", 2, 0, 10});
        button.texts.push_back(ui_button_text{"Change", 1, 0, -5});
        button.texts.push_back(ui_button_text{"base TPS", 1, 0, -15});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{5, 0}, vw - 30, vh - 30, 70, 50, 2};
        button.texts.push_back(ui_button_text{"Preview", 1, 0, 10});
        button.texts.push_back(ui_button_text{arena->preview_gp_trajectory?"ON":"OFF", 1, 0, -10});
        all_buttons->buttons.push_back(button);
    }

    top_bar_x_offset += !arena->ui_speedbar_opened?40:599;

    {
        ui_button_single button{{6, 0}, top_bar_x_offset + 75, vh, 30, 30};
        button.enabled = !arena->ui_garden_opened;
        button.texts.push_back(ui_button_text{"v", 2});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{7, 0}, top_bar_x_offset + 120 + 55 * 1.0f, vh, 60 + 55 * 2 + 8, 128};
        button.enabled = arena->ui_garden_opened;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{7, 1}, top_bar_x_offset + 75, vh, 30, 128, 2};
        button.enabled = arena->ui_garden_opened;
        button.texts.push_back(ui_button_text{"^", 2, 0, -30});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{7, 2}, top_bar_x_offset + 120, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_garden_opened;
        button.texts.push_back(ui_button_text{(
            arena->use_garden?"Active":arena->garden?"Frozen":"None"
        ), 1, 0, 5});
        button.texts.push_back(ui_button_text{"Garden", 1, 0, -5});
        button.highlighted = arena->use_garden;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{7, 3}, top_bar_x_offset + 120 + 55, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_garden_opened;
        button.texts.push_back(ui_button_text{"Reset", 1, 0, 0});
        button.highlighted = arena->garden_queued_action == GARDEN_ACTION_RESET;
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{7, 4}, top_bar_x_offset + 120 + 55 * 2, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_garden_opened;
        button.texts.push_back(ui_button_text{"Take", 1, 0, 5});
        button.texts.push_back(ui_button_text{"Best", 1, 0, -5});
        button.highlighted = arena->garden_queued_action == GARDEN_ACTION_TAKE_BEST;
        all_buttons->buttons.push_back(button);
    }
}

// Draw text, and return the x where the text ends
float draw_text_default(arena* arena, std::string text, float x, float y, float scale=FONT_SCALE_DEFAULT) {
    text_set_scale(scale);
    text_stream_update(&arena->tick_counter, text.c_str());
	text_stream_render(&arena->tick_counter,
			arena->view.width, arena->view.height, (int)x, (int)y);
    return x + FONT_X_INCREMENT * scale * text.size();
}

void draw_tick_counter(struct arena *arena)
{
    const bool is_running_arena = is_running(arena);
    const bool is_running_arena_and_not_paused = is_running_arena && arena->single_ticks_remaining==-1;
    block_graphics* graphics = (block_graphics*)arena->block_graphics_v2b;
    float x = 10;
    x = draw_text_default(arena, std::to_string(arena->tick), x, 10);
    x = draw_text_default(arena, "ticks", x, 10, 1);
    if(arena->has_won) {
        x = std::max(x, 10 + FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 8);
        x += FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 1;
        x = draw_text_default(arena, std::to_string(arena->tick_solve), x, 10);
        x = draw_text_default(arena, "at solve", x, 10, 1);
    }
    // fps/tps counter
    graphics->fps_tracker.push_record(arena->frame_counter++);
    graphics->tps_tracker.push_record(arena->tick);
    double fps_value = graphics->fps_tracker.get_tps();
    // try to average over 2 seconds
    size_t tps_interval = std::min<size_t>(fps_tracker_t::buffer_size - 1, std::max<size_t>(1, (size_t)(fps_value * 2)));
    double tps_value = graphics->tps_tracker.get_tps(tps_interval);
    bool tps_is_prediction = tps_value == 0 || !is_running_arena_and_not_paused;
    if(!is_running_arena_and_not_paused) {
        // reset when not running
        graphics->tps_tracker.clear();
    }
    if(tps_is_prediction) {
        // if not running, or not enough data, show a static prediction
        tps_value = _fcsim_target_tps;
    }
    x = std::max(x, 10 + FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 8);
    x += FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 1;
    x = draw_text_default(arena, std::to_string((int64_t)rint(fps_value)), x, 10);
    x = draw_text_default(arena, "FPS", x, 10, 1);
    x = std::max(x, 10 + FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 8);
    x += FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 1;
    x = draw_text_default(arena, tps_value >= 1e9?"Infinity":std::to_string((int64_t)rint(tps_value)), x, 10);
    x = draw_text_default(arena, !tps_is_prediction?"TPS average":"TPS predicted", x, 10, 1);
    x += FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 1;
    x = draw_text_default(arena, std::to_string(arena->state), x, 10);
    x += FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 1;
    if(!arena->garden) {
        x = draw_text_default(arena, "No garden", x, 10, 1);
    } else {
        garden_t* garden = (garden_t*)arena->garden;
        x = draw_text_default(arena, std::to_string(garden->creatures[0].tick), x, 10, 1);
        if(garden->creatures[0].trails.trails.size() > 0) {
            x += FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 1;
            x = draw_text_default(arena, std::to_string(garden->creatures[0].trails.trails[0].datapoints.size()), x, 10, 1);
        }
    }
}

void draw_ui(arena* arena) {
    block_graphics* graphics = (block_graphics*)arena->block_graphics_v2b;
    ui_button_collection* all_buttons = (ui_button_collection*)arena->ui_buttons;
    graphics->clear();

    for(auto it = all_buttons->buttons.begin(); it != all_buttons->buttons.end(); ++it) {
        if(!it->enabled)continue;
        shell ishell{SHELL_RECT, {}, it->x, it->y, 0};
        ishell.rect = {it->w, it->h};
        block_graphics_add_rect(graphics, ishell, FCSIM_UI_BUTTON, it->z_offset, it->highlighted?color{0.25,1,1,1}:color{0,0,0,0});
    }

	block_graphics_draw(graphics, &arena->view, false);

	draw_tick_counter(arena);

    for(auto it = all_buttons->buttons.begin(); it != all_buttons->buttons.end(); ++it) {
        if(!it->enabled)continue;
        for(auto jt = it->texts.begin(); jt != it->texts.end(); ++jt) {
            float predict_w = FONT_X_INCREMENT * jt->scale * jt->text.size();
            float predict_h = FONT_Y_INCREMENT * jt->scale;
            float x = it->x + jt->relative_x - jt->align_x * predict_w;
            float y = it->y + jt->relative_y - jt->align_y * predict_h;
            draw_text_default(arena, jt->text, x, y, jt->scale);
        }
    }
}

void draw_single_trail(block_graphics* graphics, const trail_t& the_trail, color col) {
    if(the_trail.datapoints.size() < 2) {
        return;
    }
    const double LINE_RADIUS = 2;
    b2Vec2 last = the_trail.datapoints[0];
    for(size_t datapoint_index = 1; datapoint_index < the_trail.datapoints.size(); ++datapoint_index) {
        b2Vec2 current = the_trail.datapoints[datapoint_index];
        block_graphics_add_line(graphics, last, current, LINE_RADIUS, col, 4);
        last = current;
    }
}

void draw_multi_trail(block_graphics* graphics, const multi_trail_t& all_trails, color col) {
    for(size_t trail_index = 0; trail_index < all_trails.trails.size(); ++trail_index) {
        const trail_t& the_trail = all_trails.trails[trail_index];
        draw_single_trail(graphics, the_trail, col);
    }
}

void preview_trail_draw(arena* arena) {
    block_graphics* graphics = (block_graphics*)arena->block_graphics_v2;

    // draw main path trails
    multi_trail_t* all_trails = (multi_trail_t*)arena->preview_trail;
    draw_multi_trail(graphics, *all_trails, get_color_by_type(FCSIM_GOAL_CIRCLE, 0));
}

void garden_trail_draw(arena* arena) {
    block_graphics* graphics = (block_graphics*)arena->block_graphics_v2;

    if(!arena->garden) {
        return;
    }
    garden_t* garden = (garden_t*)arena->garden;

    // draw garden trails
    for(size_t i = 0; i < garden->creatures.size(); ++i) {
        creature_t& the_creature = garden->creatures[i];
        draw_multi_trail(graphics, the_creature.trails, get_color_by_type(FCSIM_GOAL_CIRCLE, 1));
    }
}

void arena_draw(struct arena *arena)
{
	color sky_color = get_color_by_type(FCSIM_SKY, 1);
	glClearColor(sky_color.r, sky_color.g, sky_color.b, sky_color.a);
	glClear(GL_COLOR_BUFFER_BIT);

	block_graphics_reset(arena, &arena->design);
    if(arena->preview_gp_trajectory && arena->preview_trail) {
        preview_trail_draw(arena);
    }
    garden_trail_draw(arena);
	block_graphics_draw((block_graphics*)arena->block_graphics_v2, &arena->view);

    regenerate_ui_buttons(arena);
    draw_ui(arena);
}

void block_graphics_init_single(void*& _graphics) {
    block_graphics*& graphics = *(block_graphics**)&_graphics;
    graphics = _new<block_graphics>();

	glGenBuffers(1, &graphics->_index_buffer);
	glGenBuffers(1, &graphics->_coord_buffer);
	glGenBuffers(1, &graphics->_color_buffer);
}

extern "C" void block_graphics_init(struct arena *ar)
{
    block_graphics_init_single(ar->block_graphics_v2);
    block_graphics_init_single(ar->block_graphics_v2b);
    ar->ui_buttons = _new<ui_button_collection>();
    regenerate_ui_buttons(ar);
    ar->ui_toolbar_opened = false;
    ar->ui_speedbar_opened = false;
    ar->ui_garden_opened = false;
    ar->single_ticks_remaining = -1;
    ar->autostop_on_solve = false;
}

extern "C" bool arena_mouse_click_button(struct arena *arena) {
    // O(NK)
    // odd z indices won't be used, but we check them anyway
    ui_button_collection* all_buttons = (ui_button_collection*)arena->ui_buttons;
    int max_z_offset = 0;
    for(auto it = all_buttons->buttons.begin(); it != all_buttons->buttons.end(); ++it) {
        max_z_offset = std::max(max_z_offset, it->z_offset);
    }
    for(int z_offset = max_z_offset; z_offset >= 0; --z_offset) {
        for(auto it = all_buttons->buttons.begin(); it != all_buttons->buttons.end(); ++it) {
            if(it->z_offset != z_offset)continue;
            float cx = arena->cursor_x;
            float cy = arena->view.height - arena->cursor_y;
            if(it->enabled && it->clickable
            && it->x - it->w * 0.5 <= cx && cx <= it->x + it->w * 0.5
            && it->y - it->h * 0.5 <= cy && cy <= it->y + it->h * 0.5) {
                on_button_clicked(arena, *it);
                return true;
            }
        }
    }
    return false;
}