#ifndef BUTTON_H
#define BUTTON_H

struct button {
	GLuint vertex_buf;
	float r, g, b;
};

bool button_compile_shaders(void);

void button_create(struct button *button, int w, int h, float r, float g, float b);
void button_render(struct button *button, int w, int h, int x, int y);


#endif