#ifdef __wasm__
#include "algorithm.h"
#include "stdint.h"
#include "string.h"
#include "vector.h"
#else
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#endif
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
	float w = fmaxf(fabsf(shell.rect.w), 4.0);
	float h = fmaxf(fabsf(shell.rect.h), 4.0);
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
    float iw = abs(w-8);
    float ih = abs(h-8);
    float ow,oh;
    if(type_id == FCSIM_ROD || type_id == FCSIM_SOLID_ROD) {
        ow = w;
        oh = h;
        if(type_id == FCSIM_ROD && !graphics->wireframe)oh*=2;
        iw = abs(ow-4);
        ih = abs(oh-4);
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

void block_graphics_draw(struct block_graphics *graphics, struct view *view)
{
	glUseProgram(block_program);

	glUniform2f(block_program_scale_uniform,
		     1.0f / (view->width * view->scale),
		    -1.0f / (view->height * view->scale));
	glUniform2f(block_program_shift_uniform,
		    -view->x / (view->width * view->scale),
		     view->y / (view->height * view->scale));

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

static void draw_tick_counter(struct arena *arena)
{
    std::string buf = std::to_string(arena->tick);

	text_stream_update(&arena->tick_counter, buf.c_str());
	text_stream_render(&arena->tick_counter,
			arena->view.width, arena->view.height, 10, 10);
}

void arena_draw(struct arena *arena)
{
	color sky_color = get_color_by_type(FCSIM_SKY, 1);
	glClearColor(sky_color.r, sky_color.g, sky_color.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	block_graphics_reset(arena, &arena->design);
	block_graphics_draw((block_graphics*)arena->block_graphics_v2, &arena->view);

	draw_tick_counter(arena);
}

extern "C" void block_graphics_init(struct arena *ar)
{
    ar->block_graphics_v2 = new block_graphics();
    block_graphics* graphics = (block_graphics*)ar->block_graphics_v2;

	glGenBuffers(1, &graphics->_index_buffer);
	glGenBuffers(1, &graphics->_coord_buffer);
	glGenBuffers(1, &graphics->_color_buffer);
}