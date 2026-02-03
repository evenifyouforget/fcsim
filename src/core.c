#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <box2d/b2Body.h>
#include "gl.h"
#include "graph.h"
#include "button.h"
#include "text.h"
#include "arena.h"
#include "xml.h"

struct arena the_arena;

void key_down(int key)
{
	arena_key_down_event(&the_arena, key);
}

void key_up(int key)
{
	arena_key_up_event(&the_arena, key);
}

void move(int x, int y)
{
	arena_mouse_move_event(&the_arena, x, y);
}

void button_down(int button)
{
	arena_mouse_button_down_event(&the_arena, button);
}

void button_up(int button)
{
	arena_mouse_button_up_event(&the_arena, button);
}

void scroll(int delta)
{
	arena_scroll_event(&the_arena, delta);
}

void resize(int w, int h)
{
	glViewport(0, 0, w, h);

	arena_size_event(&the_arena, w, h);
}

void init(char *xml, int len, int expect_checksum)
{
	bool res;

	res = arena_compile_shaders();
	res = text_compile_shaders();
	res = button_compile_shaders();
	/*
	if (!res)
		exit(1);
	*/

	arena_init(&the_arena, 800, 800, xml, len);
	the_arena.design.expect_checksum = expect_checksum;
}

int get_main_design_checksum() {
	return the_arena.design.actual_checksum;
}

char *export_design(struct design *design, char *user, char *name, char *desc);

char *export(char *user, char *name, char *desc)
{
	return export_design(&the_arena.design, user, name, desc);
}

void draw(void)
{
	arena_draw(&the_arena);
}

void call(void (*func)(void *arg), void *arg)
{
	func(arg);
}
