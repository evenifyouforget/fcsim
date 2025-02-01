#ifndef TEXT_H
#define TEXT_H

struct text {
	GLuint vertex_buf;
	size_t vertex_cnt;
};

struct text_stream {
	GLuint vertex_buf;
	size_t vertex_cnt;
	size_t len;
};

bool text_compile_shaders(void);

void text_create(struct text *text, char *str);
void text_render(struct text *text, int w, int h, int x, int y);

void text_stream_create(struct text_stream *text, size_t len);
void text_stream_update(struct text_stream *text, char *str);
void text_stream_render(struct text_stream *text, int w, int h, int x, int y);


#endif