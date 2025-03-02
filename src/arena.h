#ifndef ARENA_H
#define ARENA_H

#include "graph.h"
#include "text.h"

#define BASE_FPS 30
#ifdef __wasm__
#define MIN_MSPT 33
#else
#define MIN_MSPT 5
#endif

#define TAU 6.28318530718

struct view {
	float x;
	float y;
	float scale;
	float width;
	float height;
};

enum state {
	STATE_NORMAL,
	STATE_NORMAL_PAN,
	STATE_NEW_ROD,
	STATE_NEW_WHEEL,
	STATE_MOVE,
	STATE_RUNNING,
	STATE_RUNNING_PAN,
};

enum tool {
	TOOL_MOVE,
	TOOL_ROD,
	TOOL_SOLID_ROD,
	TOOL_WHEEL,
	TOOL_CW_WHEEL,
	TOOL_CCW_WHEEL,
	TOOL_DELETE,
};

struct block_head {
	struct block_head *next;
	struct block *block;
	double orig_x;
	double orig_y;
};

struct joint_head {
	struct joint_head *next;
	struct joint *joint;
	double orig_x;
	double orig_y;
};

struct arena {
	struct design design;
	b2World *world;

	int ival;
	int tick_ms;
	int tick_multiply;

	struct view view;

	int cursor_x;
	int cursor_y;
	bool shift;
	bool ctrl;

	enum tool tool;
	enum tool tool_hidden;
	enum state state;
	struct joint *hover_joint;
	struct block *hover_block;

	struct joint_head *root_joints_moving;
	struct block_head *root_blocks_moving;
	struct block_head *blocks_moving;
	float move_orig_x;
	float move_orig_y;
	struct joint *move_orig_joint;
	struct block *move_orig_block;

	struct block *new_block;

	// for game graphics
	void* block_graphics_v2; // actual type: block_graphics*
	// for ui graphics
	void* block_graphics_v2b; // actual type: block_graphics*

	uint64_t tick;
	uint64_t tick_solve;
	// this text object has been repurposed for all text rendering
	struct text_stream tick_counter;
	bool has_won;
	int single_ticks_remaining; // -1 for normal playback, 0 to disable, positive for step n frames
	bool autostop_on_solve;

	// ui button templates
	void* ui_buttons; // actual type: ui_button_collection*
	bool ui_toolbar_opened;
};

bool arena_compile_shaders(void);
void arena_init(struct arena *arena, float w, float h, char *xml, int len);

void arena_show(struct arena *arena);
void arena_draw(struct arena *arena);

// test if any buttons are clicked, return true if clicked
bool arena_mouse_click_button(struct arena *arena);

void arena_key_up_event(struct arena *arena, int key);
void arena_key_down_event(struct arena *arena, int key);
void arena_mouse_move_event(struct arena *arena, int x, int y);
void arena_scroll_event(struct arena *arena, int delta);
void arena_mouse_button_up_event(struct arena *arena, int button);
void arena_mouse_button_down_event(struct arena *arena, int button);
void arena_size_event(struct arena *arena, float w, float h);

void start_stop(struct arena *arena);
bool is_running(struct arena *arena);
void update_tool(struct arena *arena);

extern GLuint block_program;
extern GLuint block_program_coord_attrib;
extern GLuint block_program_color_attrib;
extern GLuint block_program_scale_uniform;
extern GLuint block_program_shift_uniform;
extern GLuint joint_program;
extern GLuint joint_program_coord_attrib;

// c++ compat
void block_graphics_init(struct arena *ar);

#endif