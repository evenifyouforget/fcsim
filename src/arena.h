#ifndef ARENA_H
#define ARENA_H

#include "graph.h"
#include "text.h"

#define NO_CHANGE -1
#define BASE_FPS_TABLE {30, 36}
#define BASE_FPS_TABLE_SIZE 2
#ifdef __wasm__
#define MIN_MSPT 10
#else
#define MIN_MSPT 5
#endif

#define TAU 6.28318530718

#define GARDEN_ACTION_NONE -1
#define GARDEN_ACTION_RESET 3
#define GARDEN_ACTION_TAKE_BEST 4

#define GARDEN_MAX_CREATURES 3
#define GARDEN_TRIGGER_STALE_CREATURES 2
#define GARDEN_MAX_TICKS 10000
#define GARDEN_TIME_FACTOR 0.5

#define MAX_RENDER_TEXT_LENGTH 1000

#define INFINITY (1.0 / 0.0)

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
	struct design *design;
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

	uint64_t frame_counter;
	uint64_t tick;
	uint64_t tick_solve;
	// this text object has been repurposed for all text rendering
	struct text_stream tick_counter;
	bool has_won;
	int single_ticks_remaining; // -1 for normal playback, 0 to disable, positive for step n frames
	bool autostop_on_solve;

	bool preview_gp_trajectory;
	struct design* preview_design;
	b2World *preview_world;
	void *preview_trail; // real type: multi_trail_t*

	// autotweaker is called "garden" since we maintain a "garden" of designs and mutate them
	// keeping the highest fitness (lowest error score) designs, in hopes of reaching a solution
	bool use_garden; // note: disabling garden will not delete the garden, just stops iterating
	void* garden; // actual type: garden_t*
	int garden_queued_action;

	// ui button templates
	void* ui_buttons; // actual type: ui_button_collection*
	bool ui_toolbar_opened;
	bool ui_speedbar_opened;
	bool ui_garden_opened;
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

// utilities for modifying designs
void mouse_up_move(struct arena *arena);
void mouse_up_new_block(struct arena *arena);
bool is_design_legal(struct design *design);
void mark_overlaps(struct arena *arena);
void mark_overlaps_data(struct design *design, b2World *world, struct block* ignore_block);

void get_rect_bb(struct shell *shell, struct area *area);
void get_circ_bb(struct shell *shell, struct area *area);
void get_block_bb(struct block *block, struct area *area);

int block_inside_area(struct block *block, struct area *area);
bool goal_blocks_inside_goal_area(struct design *design);
void tick_func(void *arg);

// lower = better, actually solving will give the lowest possible value
double goal_heuristic(struct design *design);

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
#ifndef ARENA_C
extern int _fcsim_base_fps_mod;
extern double _fcsim_target_tps;
extern int _fcsim_speed_preset;
#endif
void change_speed_factor(struct arena *arena, double new_factor, int new_base_fps_mod);
void change_speed_preset(struct arena *arena, int preset_index);

#endif