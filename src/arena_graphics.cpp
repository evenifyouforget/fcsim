#ifdef __wasm__
#include "stdint.h"
#include "stl_mock.h"
#else
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#endif
#include "stl_compat.h"
extern "C" {
#include "arena.h"
#include "gl.h"
#include "graph.h"
#include <box2d/b2Body.h>
}
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
    while(layers.size() <= z_offset) {
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
    float h = shell.type == SHELL_RECT ? shell.circ.radius * 2 : shell.rect.h;
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
	int i;

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
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 3}, 120 + 55, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"C", 2, 0, 5});
        button.texts.push_back(ui_button_text{"CCW Wheel", 1, 0, -10});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 4}, 120 + 55 * 2, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"U", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Wheel", 1, 0, -10});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 5}, 120 + 55 * 3, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"R", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Water", 1, 0, -10});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 6}, 120 + 55 * 4, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"S", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Wood", 1, 0, -10});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 7}, 120 + 55 * 5, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"M", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Move", 1, 0, -10});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{1, 8}, 120 + 55 * 6, vh - 30, 50, 50, 2};
        button.enabled = arena->ui_toolbar_opened;
        button.texts.push_back(ui_button_text{"D", 2, 0, 5});
        button.texts.push_back(ui_button_text{"Delete", 1, 0, -10});
        all_buttons->buttons.push_back(button);
    }
    {
        ui_button_single button{{2, 0}, 30, vh - 30, 50, 50};
        button.texts.push_back(ui_button_text{"Space", 1.5, 0, 8});
        button.texts.push_back(ui_button_text{is_running(arena)?"Stop":"Start", 1, 0, -8});
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
    float x = 10;
    x = draw_text_default(arena, std::to_string(arena->tick), x, 10);
    x = draw_text_default(arena, "ticks", x, 10, 1);
    if(arena->has_won) {
        x = std::max(x, 10 + FONT_X_INCREMENT * FONT_SCALE_DEFAULT * 8);
        x = draw_text_default(arena, std::to_string(arena->tick_solve), x, 10);
        x = draw_text_default(arena, "at solve", x, 10, 1);
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

void arena_draw(struct arena *arena)
{
	color sky_color = get_color_by_type(FCSIM_SKY, 1);
	glClearColor(sky_color.r, sky_color.g, sky_color.b, sky_color.a);
	glClear(GL_COLOR_BUFFER_BIT);

	block_graphics_reset(arena, &arena->design);
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
    ar->ui_buttons = new ui_button_collection();
    regenerate_ui_buttons(ar);
    ar->ui_toolbar_opened = false;
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